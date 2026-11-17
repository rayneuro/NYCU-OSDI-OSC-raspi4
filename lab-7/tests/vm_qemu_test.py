#!/usr/bin/env python3
"""End-to-end MMU, demand paging, COW and official video tests on raspi4b."""
import argparse
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time
from thread_qemu_test import read_until

LAB = Path(__file__).resolve().parents[1]

class Machine:
    def __init__(self, qemu, tmp):
        self.tmp = Path(tmp)
        self.transcript = bytearray()
        self.symbols = {}
        for row in subprocess.check_output(['aarch64-linux-gnu-nm', 'kernel8.elf'], cwd=LAB, text=True).splitlines():
            f = row.split()
            if len(f) == 3:
                self.symbols[f[2]] = int(f[0], 16)
        serial = str(self.tmp / 'serial')
        for suffix in ('.in', '.out'):
            os.mkfifo(serial + suffix)
        self.rx = os.open(serial + '.out', os.O_RDWR | os.O_NONBLOCK)
        self.tx = os.open(serial + '.in', os.O_RDWR | os.O_NONBLOCK)
        self.proc = subprocess.Popen([
            qemu, '-M', 'raspi4b', '-cpu', 'cortex-a72', '-smp', '4', '-m', '2G',
            '-display', 'none', '-monitor', 'stdio',
            '-chardev', 'pipe,id=uart,path=' + serial, '-serial', 'chardev:uart',
            '-kernel', 'kernel8.img', '-initrd', 'initramfs.cpio',
            '-dtb', 'bcm2711-rpi-4-b.dtb'], cwd=LAB,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        read_until(self.proc.stdout.fileno(), lambda b: b.endswith(b'(qemu) '))
        boot = self.until(lambda b: b'# ' in b)
        assert b'[vm] MMU enabled: kernel at 0xffff' in boot, boot

    def until(self, predicate, timeout=20):
        data = read_until(self.rx, predicate, timeout)
        self.transcript.extend(data)
        assert b'[kernel] synchronous exception' not in data, data
        return data

    def send(self, data):
        for byte in data:
            os.write(self.tx, bytes([byte]))
            time.sleep(.003)

    def monitor(self, command):
        self.proc.stdin.write(command.encode() + b'\n')
        self.proc.stdin.flush()
        return read_until(self.proc.stdout.fileno(), lambda b: b.endswith(b'(qemu) '))

    def memory(self, address, count):
        output = self.monitor(f'xp /{count}gx 0x{address & 0x0000ffffffffffff:x}')
        words = []
        for row in output.splitlines():
            if re.match(rb'^[0-9a-fA-F]{8,16}:', row):
                words.extend(int(w, 16) for w in re.findall(rb'0x[0-9a-fA-F]+', row))
        assert len(words) == count, output
        return words

    def snapshot(self):
        self.monitor('stop')
        try:
            a = self.memory(self.symbols['free_area'], 30)
            return sum((a[3 * i] & 0xffffffff) << i for i in range(10)), self.memory(self.symbols['zombies'], 2)
        finally:
            self.monitor('cont')

    def reclaimed(self, baseline):
        deadline = time.monotonic() + 10
        while True:
            pages, zombies = self.snapshot()
            if pages >= baseline - 3 and zombies == [self.symbols['zombies']] * 2:
                return pages
            assert time.monotonic() < deadline, (pages, baseline, zombies)
            time.sleep(.02)

    def command(self, cmd):
        self.send(cmd.encode() + b'\r')
        return self.until(lambda b: b'# ' in b)

    def launch(self, name):
        self.send(b'exec\r')
        self.until(lambda b: b'Program: ' in b)
        self.send(name.encode() + b'\r')

    def screenshot(self, name):
        # Works with headless QEMU builds that omit the screendump command.
        # Capture the framebuffer's physical RAM, then write a portable PPM.
        self.monitor('stop')
        try:
            geometry, layout, address = self.memory(self.symbols['fb'], 3)
            width, height = geometry & 0xffffffff, geometry >> 32
            pitch, rgb = layout & 0xffffffff, layout >> 32
            assert 0 < width <= 4096 and 0 < height <= 4096, (width, height)
            raw = self.tmp / (name + '.raw')
            result = self.monitor(f'pmemsave 0x{address & 0x0000ffffffffffff:x} {pitch * height} "{raw}"')
            assert raw.exists(), result[-200:]
            pixels = raw.read_bytes()
        finally:
            self.monitor('cont')
        image = bytearray(f'P6\n{width} {height}\n255\n'.encode())
        for y in range(height):
            row = pixels[y * pitch:y * pitch + width * 4]
            for x in range(0, len(row), 4):
                image.extend((row[x + 2], row[x + 1], row[x]) if rgb else row[x:x + 3])
        path = self.tmp / (name + '.ppm')
        path.write_bytes(image)
        return bytes(image)

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        os.close(self.rx)
        os.close(self.tx)


def exercise_vm(m):
    stable = None
    for quantum in (1, 10, 50):
        m.command(f'quantum {quantum}')
        baseline, _ = m.snapshot()
        m.launch('vm_test.bin')
        output = m.until(lambda b: b'CHECK lazy allocated\n' in b)
        allocated, _ = m.snapshot()
        assert baseline - allocated < 64, (baseline, allocated)
        m.send(b'x')
        output += m.until(lambda b: b'CHECK lazy touched\n' in b)
        touched, _ = m.snapshot()
        assert 2 <= allocated - touched <= 8, (allocated, touched)
        m.send(b'x')
        output += m.until(lambda b: b'READ cross-page ready\n' in b)
        m.send(b'A\0\rZ')
        output += m.until(lambda b: b'PASS VM complete\n' in b and b'# ' in b)
        assert b'FAIL' not in output, output
        for marker in (b'PASS COW grandchild', b'PASS COW child and mailbox',
                       b'PASS COW parent and last reference', b'PASS exec replacement',
                       b'PASS executable mapping', b'PASS cross-page UART COW'):
            assert marker in output, output
        assert output.count(b'[Segmentation fault]') == 7, output
        assert b'[Copy-on-write fault]' in output and b'[Translation fault]' in output, output
        pages = m.reclaimed(baseline)
        if stable is None:
            stable = pages
        assert pages == stable, (pages, stable)
        print(f'PASS VM {quantum} ms: sparse 512 MiB ({baseline - allocated} pages before touch, '
              f'{allocated - touched} extra after touch), nested COW, mailbox/UART copyout, '
              '7 protection faults, exec, full reclamation', flush=True)


def exercise_official(m):
    m.command('quantum 10')
    baseline, _ = m.snapshot()
    m.launch('vm.img')
    output = m.until(lambda b: b'Process start, pid is:' in b and b'# ' in b)
    assert b'Segmentation fault' not in output, output
    output += m.command('help')
    assert b'This is user process' in output, output
    before = m.screenshot('before-video')
    output = m.command('fork')
    match = re.search(rb'Child pid: (\d+)', output)
    assert match, output
    child = int(match[1])
    # Allow the child to configure the framebuffer and fault in its image.
    time.sleep(1)
    output += m.command('pid')
    assert b'Pid is:' in output and b'Segmentation fault' not in output, output
    after = m.screenshot('video-frame')
    assert before != after, 'Official child did not update the framebuffer'
    # Frame output must continue while its parent accepts UART commands.
    frames = [after]
    for i in range(3):
        time.sleep(.3)
        frames.append(m.screenshot(f'video-frame-{i}'))
        output += m.command('pid')
    assert len(set(frames)) > 1, 'Video frames did not change during preemption'
    assert b'Unable to set screen' not in output and b'Segmentation fault' not in output, output
    m.command(f'kill {child}')
    # After one child already exercised framebuffer/COW, repeat the path.
    output = m.command('fork')
    match = re.search(rb'Child pid: (\d+)', output)
    assert match, output
    time.sleep(.3)
    output += m.command(f'kill {int(match[1])}')
    assert b'Segmentation fault' not in output, output
    m.send(b'exec\r')
    output = m.until(lambda b: b'Process start, pid is:' in b and b'# ' in b)
    assert b'Segmentation fault' not in output, output
    output = m.command('demand')
    assert b'[Translation fault]' in output, output
    output = m.command('mmap_w')
    assert b'[Segmentation fault]' in output, output
    m.reclaimed(baseline)
    m.launch('vm.img')
    m.until(lambda b: b'Process start, pid is:' in b and b'# ' in b)
    output = m.command('mmap_r')
    assert b'[Segmentation fault]' in output, output
    m.reclaimed(baseline)
    print('PASS official vm.img: help/pid/fork/kill/exec, changing video frames with concurrent UART, '
          'demand/mmap_w/mmap_r, reclamation', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=shutil.which('qemu-system-aarch64'))
    parser.add_argument('--only', choices=('vm', 'official'))
    args = parser.parse_args()
    if not args.qemu:
        parser.error('provide --qemu PATH')
    subprocess.run(['make', 'all'], cwd=LAB, check=True)
    results = LAB / 'tests' / 'results'
    results.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='lab6-vm-') as tmp:
        m = Machine(args.qemu, tmp)
        try:
            if args.only != 'official':
                exercise_vm(m)
            if args.only != 'vm':
                exercise_official(m)
            assert b'Hello world!' in m.command('hello')
        finally:
            (results / 'vm-serial.log').write_bytes(m.transcript)
            for screenshot in Path(tmp).glob('*.ppm'):
                shutil.copyfile(screenshot, results / screenshot.name)
            m.close()

if __name__ == '__main__':
    main()
