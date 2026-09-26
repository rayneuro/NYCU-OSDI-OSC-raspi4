#!/usr/bin/env python3
"""Exercise real EL0 syscalls with a temporary initramfs; preserve user's archive."""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
from thread_qemu_test import read_until

LAB = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=shutil.which('qemu-system-aarch64'))
    args = parser.parse_args()
    if not args.qemu:
        parser.error('provide --qemu PATH')
    subprocess.run(['make', 'kernel8.img', 'process-fixtures'], cwd=LAB, check=True)
    symbols = {}
    for row in subprocess.check_output(['aarch64-linux-gnu-nm', 'kernel8.elf'], cwd=LAB, text=True).splitlines():
        fields = row.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)
    with tempfile.TemporaryDirectory(prefix='lab5-process-') as tmp:
        tmp = Path(tmp)
        archive = tmp / 'initramfs.cpio'
        with archive.open('wb') as out:
            subprocess.run(['cpio', '-o', '-H', 'newc'], cwd=LAB / 'rootfs',
                           input=b'process_test.bin\nexec_test.bin\n', stdout=out, check=True)
        serial = str(tmp / 'serial')
        for suffix in ('.in', '.out'):
            os.mkfifo(serial + suffix)
        rx = os.open(serial + '.out', os.O_RDWR | os.O_NONBLOCK)
        tx = os.open(serial + '.in', os.O_RDWR | os.O_NONBLOCK)
        proc = subprocess.Popen([
            args.qemu, '-M', 'raspi4b', '-cpu', 'cortex-a72', '-smp', '4',
            '-m', '2G', '-display', 'none', '-monitor', 'stdio',
            '-chardev', 'pipe,id=uart,path=' + serial, '-serial', 'chardev:uart',
            '-kernel', 'kernel8.img', '-initrd', str(archive),
            '-dtb', 'bcm2711-rpi-4-b.dtb'], cwd=LAB,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        def monitor(command):
            proc.stdin.write(command.encode() + b'\n')
            proc.stdin.flush()
            return read_until(proc.stdout.fileno(), lambda b: b.endswith(b'(qemu) '))

        def memory(address, count):
            output = monitor(f'xp /{count}gx 0x{address:x}')
            words = []
            for row in output.splitlines():
                if re.match(rb'^[0-9a-fA-F]{8,16}:', row):
                    words.extend(int(w, 16) for w in re.findall(rb'0x[0-9a-fA-F]+', row))
            assert len(words) == count, output
            return words

        def snapshot():
            monitor('stop')
            try:
                areas = memory(symbols['free_area'], 30)
                pages = sum((areas[3 * order] & 0xffffffff) << order for order in range(10))
                return pages, memory(symbols['zombies'], 2)
            finally:
                monitor('cont')

        def send(data):
            for byte in data:
                os.write(tx, bytes([byte]))
                time.sleep(0.003)

        try:
            read_until(proc.stdout.fileno(), lambda b: b.endswith(b'(qemu) '))
            read_until(rx, lambda b: b'# ' in b)
            baseline, _ = snapshot()
            for quantum in (1, 10, 50):
                send(f'quantum {quantum}\r'.encode())
                read_until(rx, lambda b: f'Quantum: {quantum} ms'.encode() in b)
                send(b'exec\rprocess_test.bin\r')
                output = read_until(rx, lambda b: b'READ ready\n' in b, timeout=30)
                os.write(tx, b'A\0\rZ')
                output += read_until(rx, lambda b: b'PASS process complete\n' in b and b'# ' in b)
                for marker in (b'PASS fork child', b'PASS fork parent', b'PASS exec replacement',
                               b'PASS mailbox', b'PASS kill spinner', b'PASS read/write'):
                    assert marker in output, output
                assert b'FAIL' not in output and b'fault' not in output, output
                assert b'A\0\rZ' in output, output
                deadline = time.monotonic() + 10
                while True:
                    pages, zombies = snapshot()
                    # The allocator may retain one empty 16-byte-object slab.
                    if pages >= baseline - 1 and zombies == [symbols['zombies']] * 2:
                        break
                    assert time.monotonic() < deadline, (pages, baseline, zombies)
                    time.sleep(0.02)
                print(f'PASS {quantum} ms: EL0 fork/exec/exit/kill, mailbox, binary UART, register preservation, reclamation, shell return')
            send(b'hello\r')
            read_until(rx, lambda b: b'Hello world!' in b)
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            os.close(rx)
            os.close(tx)

if __name__ == '__main__':
    main()
