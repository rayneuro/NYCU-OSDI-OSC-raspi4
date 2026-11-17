#!/usr/bin/env python3
"""Boot the interactive syscall_test command and verify all eight syscalls."""
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

def check_fork_trace(output):
    start = re.findall(rb'Fork Test, pid (\d+)\r\n', output)
    parent = re.findall(rb'parent here, pid (\d+), child (\d+)\r\n', output)
    assert len(start) == len(parent) == 1, output
    parent_pid, child_pid = map(int, parent[0])
    assert parent_pid == int(start[0]) and child_pid != parent_pid, output
    rows = re.findall(
        rb'(first|second) child pid: (\d+), cnt: (\d+), '
        rb'ptr: (0x[0-9a-f]{16}), sp : (0x[0-9a-f]{16})\r\n', output)
    assert len(rows) == 5, output
    first = [row for row in rows if row[0] == b'first']
    second = [row for row in rows if row[0] == b'second']
    assert [int(row[2]) for row in first] == [1, 2], output
    assert [int(row[2]) for row in second] == [2, 3, 4], output
    assert {int(row[1]) for row in first} == {child_pid}, output
    grandchild_pids = {int(row[1]) for row in second}
    assert len(grandchild_pids) == 1 and not grandchild_pids & {parent_pid, child_pid}, output
    stacks = []
    for group in (first, second):
        addresses = {(int(row[3], 16), int(row[4], 16)) for row in group}
        assert len(addresses) == 1, output
        ptr, sp = addresses.pop()
        assert sp > 0 and sp % 16 == 0 and sp <= ptr < sp + 16384, output
        stacks.append((ptr, sp))
    # Lab 5 copies and relocates physical stacks; both frames must be distinct.
    assert stacks[0][0] != stacks[1][0] and stacks[0][1] != stacks[1][1], output
    assert stacks[0][0] - stacks[0][1] == stacks[1][0] - stacks[1][1], output

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=shutil.which('qemu-system-aarch64'))
    args = parser.parse_args()
    if not args.qemu:
        parser.error('provide --qemu PATH')
    subprocess.run(['make', 'kernel8.img', 'syscall-fixtures'], cwd=LAB, check=True)
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
                           input=b'syscall_test.bin\nsyscall_exec_test.bin\n', stdout=out, check=True)
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
                send(b'syscall_test\r')
                output = read_until(rx, lambda b: b'[INPUT] Type test then press Enter: ' in b, timeout=30)
                send(b'test\r')
                output += read_until(rx, lambda b: b'[EXIT] 5 exit: returning to kernel shell' in b and b'# ' in b)
                for marker in (b'[PASS] 0 getpid', b'[PASS] 1 uart_read',
                               b'[PASS] 2 uart_write', b'[PASS] 3 exec',
                               b'[PASS] 4 fork child', b'[PASS] 4 fork parent',
                               b'[EXIT] 5 exit: replacement process',
                               b'[PASS] 6 mbox_call', b'[PASS] 7 kill',
                               b'[PASS] invalid syscall'):
                    assert marker in output, output
                assert b'[FAIL]' not in output and b'fault' not in output, output
                check_fork_trace(output)
                deadline = time.monotonic() + 10
                while True:
                    pages, zombies = snapshot()
                    # The allocator may retain one empty 16-byte-object slab.
                    if pages >= baseline - 1 and zombies == [symbols['zombies']] * 2:
                        break
                    assert time.monotonic() < deadline, (pages, baseline, zombies)
                    time.sleep(0.02)
                print(f'PASS {quantum} ms: interactive syscall 0..7, nested EL0 fork counters/PIDs/64-bit SP, timer preemption, reclamation, shell return')
            # A manual typo must report a named failure and return to shell.
            send(b'syscall_test\r')
            read_until(rx, lambda b: b'[INPUT] Type test then press Enter: ' in b)
            send(b'nope\r')
            output = read_until(rx, lambda b: b'[FAIL] uart_read: expected test + Enter' in b and b'# ' in b)
            assert b'fault' not in output, output
            print('PASS invalid input reports FAIL and returns to shell')
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
