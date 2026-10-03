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


Task-list kmalloc integration
============================

Task nodes use `kmalloc(sizeof(task_t))` and `kfree(task)` after callbacks.
`mm_init()` must run before task creation; main does so before UART interrupts
and asynchronous output. Reserved firmware/kernel/initramfs/DTB pages stay out
of the buddy pool used by kmalloc. Allocator metadata updates save/mask/restore
IRQ state so nested IRQ bottom halves cannot interrupt partial updates.
Timer/message allocations still use the separate simple allocator.

```sh
cc -Wall -Wextra -Wno-sign-compare -g -fsanitize=address,undefined \
  -ffunction-sections -fdata-sections -I inc \
  tests/tasklist_mm_test.c src/tasklist.c src/mm.c \
  -Wl,--gc-sections -o /tmp/lab4-tasklist-test
ASAN_OPTIONS=detect_leaks=0 /tmp/lab4-tasklist-test
```

Checks reserved-page exclusion, repeated task allocation/reclamation, FIFO
priority ordering, higher-priority nested callbacks, restored interrupt masks,
and queue integrity on allocator exhaustion. Empty slabs may stay cached.
