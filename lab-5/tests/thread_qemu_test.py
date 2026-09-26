#!/usr/bin/env python3
"""Boot the real kernel and check round-robin output and zombie reclamation."""
import argparse
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time

LAB = Path(__file__).resolve().parents[1]


def read_until(fd, predicate, timeout=15):
    data = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if select.select([fd], [], [], 0.1)[0]:
            chunk = os.read(fd, 65536)
            if not chunk:
                raise AssertionError("QEMU closed its output: " + repr(data))
            data += chunk
            if predicate(data):
                return data
    raise AssertionError("Timed out: " + repr(data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", default=shutil.which("qemu-system-aarch64"))
    parser.add_argument("--rounds", type=int, default=3)
    args = parser.parse_args()
    if not args.qemu or args.rounds < 1:
        parser.error("provide --qemu PATH and a positive --rounds")
    symbols = {}
    for line in subprocess.check_output(
            ["aarch64-linux-gnu-nm", "kernel8.elf"], cwd=LAB, text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            symbols[fields[2]] = int(fields[0], 16)

    with tempfile.TemporaryDirectory(prefix="lab5-threads-") as tmp:
        serial = str(Path(tmp) / "serial")
        for suffix in (".in", ".out"):
            os.mkfifo(serial + suffix)
        rx = os.open(serial + ".out", os.O_RDWR | os.O_NONBLOCK)
        tx = os.open(serial + ".in", os.O_RDWR | os.O_NONBLOCK)
        proc = subprocess.Popen([
            args.qemu, "-M", "raspi4b", "-cpu", "cortex-a72", "-smp", "4",
            "-m", "2G", "-display", "none", "-monitor", "stdio",
            "-chardev", "pipe,id=uart,path=" + serial, "-serial", "chardev:uart",
            "-kernel", "kernel8.img", "-initrd", "initramfs.cpio",
            "-dtb", "bcm2711-rpi-4-b.dtb"], cwd=LAB,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        def send(data):
            # Pace host input to the emulated PL011 FIFO's capacity.
            for byte in data:
                os.write(tx, bytes([byte]))
                time.sleep(0.003)

        def monitor(command):
            proc.stdin.write(command.encode() + b"\n")
            proc.stdin.flush()
            return read_until(proc.stdout.fileno(), lambda b: b.endswith(b"(qemu) "))

        def memory(address, count):
            output = monitor(f"xp /{count}gx 0x{address:x}")
            words = []
            for line in output.splitlines():
                if re.match(rb"^[0-9a-fA-F]{8,16}:", line):
                    words.extend(int(w, 16) for w in re.findall(rb"0x[0-9a-fA-F]+", line))
            assert len(words) == count, output
            return words

        def snapshot():
            monitor("stop")
            try:
                # AArch64 free_area_t: count (4 bytes), padding, two pointers.
                areas = memory(symbols["free_area"], 30)
                pages = sum((areas[3 * order] & 0xffffffff) << order
                            for order in range(10))
                zombies = memory(symbols["zombies"], 2)
                return pages, zombies
            finally:
                monitor("cont")

        try:
            read_until(proc.stdout.fileno(), lambda b: b.endswith(b"(qemu) "))
            read_until(rx, lambda b: b"# " in b)
            baseline, zombies = snapshot()
            assert zombies == [symbols["zombies"]] * 2
            for run in range(args.rounds):
                ids = list(range(2 + run * 3, 5 + run * 3))
                expected = [(tid, iteration) for iteration in range(10) for tid in ids]
                send(b"thread_test\r")
                output = read_until(rx, lambda b: len(re.findall(
                    rb"Thread id: (\d+) (\d+)\r?\n", b)) >= 30)
                actual = [(int(tid), int(i)) for tid, i in re.findall(
                    rb"Thread id: (\d+) (\d+)\r?\n", output)]
                # Timer expiry can occur before a worker's explicit yield.
                # Global print order need not match the dispatch order exactly.
                assert sorted(actual) == sorted(expected), (actual, expected)
                for tid in ids:
                    assert [i for t, i in actual if t == tid] == list(range(10)), actual
                # The last print precedes its final yield/return. Allow idle to
                # run, then inspect actual allocator state with the VM stopped.
                deadline = time.monotonic() + 10
                while True:
                    pages, zombies = snapshot()
                    if pages == baseline and zombies == [symbols["zombies"]] * 2:
                        break
                    assert time.monotonic() < deadline, (pages, baseline, zombies)
                    time.sleep(0.02)
                print(f"PASS round {run + 1}: 30 valid lines; all 12 thread pages reclaimed")
            for quantum in (1, 10, 50):
                send(f"quantum {quantum}\r".encode())
                read_until(rx, lambda b: f"Quantum: {quantum} ms".encode() in b)
                # Both immediate and future callbacks must coexist with slices.
                send(b"SetTimeout\r0 slice-now\rSetTimeout\r1 slice-later\rpreempt_test\r")
                output = read_until(rx, lambda b: len(re.findall(
                    rb"Preempt id: (\d+) (\d+)\r?\n", b)) >= 30 and
                    b"Timeout message: slice-now" in b and
                    b"Timeout message: slice-later" in b, timeout=20)
                assert b"corruption" not in output, output
                rows = [(int(t), int(i)) for t, i in re.findall(
                    rb"Preempt id: (\d+) (\d+)\r?\n", output)]
                ids = sorted(set(t for t, i in rows))
                assert len(rows) == 30 and len(ids) == 3, rows
                for tid in ids:
                    assert [i for t, i in rows if t == tid] == list(range(10)), rows
                # All workers must make progress before any one finishes.
                assert max(next(j for j, (t, _) in enumerate(rows) if t == tid)
                           for tid in ids) < next(j for j, (_, i) in enumerate(rows) if i == 9), rows
                deadline = time.monotonic() + 10
                while True:
                    pages, zombies = snapshot()
                    if pages == baseline and zombies == [symbols["zombies"]] * 2:
                        break
                    assert time.monotonic() < deadline, (pages, baseline, zombies)
                    time.sleep(0.02)
                print(f"PASS {quantum} ms: no-yield progress, saved registers, timeouts, reclamation")
            for invalid in ("0", "1001", "-1", "abc", "99999999999999999999"):
                send(f"quantum {invalid}\r".encode())
                read_until(rx, lambda b: b"Usage: quantum" in b)
            send(b"quantum 10\r")
            read_until(rx, lambda b: b"Quantum: 10 ms" in b)
            send(b"hello\r")
            read_until(rx, lambda b: b"Hello world!" in b)
            print("PASS shell remains responsive after thread exits")
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            os.close(rx)
            os.close(tx)


if __name__ == "__main__":
    main()
