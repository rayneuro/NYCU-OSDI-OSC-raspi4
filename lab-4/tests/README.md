Startup allocation tests
========================

Run from `lab-4` with a host C compiler:

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
reserved-page exclusion, allocator exhaustion, and buddy splitting/coalescing with
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
