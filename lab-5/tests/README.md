Startup allocation tests
========================

Run from `lab-5` with a host C compiler:

```sh
cc -Wall -Wextra -Wno-sign-compare -g -fsanitize=address,undefined \
  -ffunction-sections -fdata-sections -I inc \
  tests/startup_test.c src/startup.c src/mm.c \
  -Wl,--gc-sections -o /tmp/lab4-startup-test
ASAN_OPTIONS=detect_leaks=0 /tmp/lab4-startup-test
```

Leak detection is disabled for environments running under ptrace; address and
undefined-behavior checks remain enabled. The test covers overlapping reservations,
alignment, overflow, table exhaustion, disabling startup allocation after handoff,
reserved-page exclusion, allocator exhaustion, repeated slab reuse without aliasing,
and buddy splitting/coalescing with
a non-power-of-two page count.

Boot flow: `main` discovers initramfs, then calls `mm_init` before enabling IRQs.
The Raspberry Pi mailbox ARM-memory response supplies the contiguous managed RAM
range (additional RAM banks are not managed). The startup allocator records kernel
code/data/BSS/stack, the low firmware page, initramfs, DTB, and FDT reserve-map
entries. It allocates and records the page-frame array without calling buddy or
`kmalloc`. Reservations are rounded outward to pages during handoff; only remaining
pages enter buddy free lists. The bootstrap reservation table is in kernel BSS and
supports 128 entries; initialization halts on reservation/allocation failure.

`mm_init` is idempotent, so the existing `ma` shell command does not reset live
allocations. The old demonstration allocations have been removed from initialization.


Thread integration test
=======================

Build a fresh kernel first (avoid copied/stale object files):

```sh
make clean
make kernel8.img
python3 tests/thread_qemu_test.py --qemu /path/to/qemu-system-aarch64
```

Requires Python 3, `aarch64-linux-gnu-nm`, and QEMU with `raspi4b` support.
`--rounds N` changes the number of create/run/exit/reap cycles (default 3).
The test connects UART0 and the QEMU monitor through pipes, requiring no sockets.
It verifies 30 worker lines per cycle with monotonically increasing per-thread
counters, explicit and implicit exit, complete recovery of the 12 worker pages,
an empty zombie queue, and continued shell input after cleanup.
It also runs workers that never yield at 1, 10, and 50 ms quanta. Their startup
barrier cannot complete without preemption. The test checks live integer/FP
registers across interrupts, exclusion inside preempt-disabled sections,
coexisting immediate/future timeouts, and rejection of invalid quantum values. The monitor checks use this lab's AArch64
`free_area_t` layout and `MAX_ORDER = 9`.


User process integration test
=============================

```sh
make -B kernel8.img
python3 tests/process_qemu_test.py --qemu /path/to/qemu-system-aarch64
```

Requires the cross compiler, objcopy, cpio, Python 3 and raspi4b QEMU. Uses a
private temporary archive, preserving the supplied initramfs. At 1/10/50 ms,
checks getpid, fork parent/child results and copied stack values, exec and failed
exec, exit, kill of a non-yielding user loop, mailbox response, exact binary UART
read/write, invalid syscalls, and zero-length I/O. A user assembly probe checks
live caller/callee integer registers and both low/high SIMD registers over timer
interrupts. The parent/child startup barrier requires EL0 timer preemption.
Monitor snapshots check the zombie list and free-page recovery (allowing the
allocator's single cached descriptor slab). The original kernel-thread test
also covers EL1 preemption, nested interrupts and timeout callbacks.
