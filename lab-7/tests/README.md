# Lab 6 tests

Run inherited regression tests from `lab-7` (see the top-level README for Lab 7 tests):

```sh
make test-all
```

| Command | Coverage |
| --- | --- |
| `make test-allocator` | Inherited startup/buddy/slab tests with ASan/UBSan. `VM_HOST_TEST` uses host pointers only; production keeps the high kernel map. |
| `python3 tests/thread_qemu_test.py` | Kernel round-robin, EL1 preemption, integer/SIMD state, timeouts, zombie reclamation. |
| `python3 tests/process_qemu_test.py` | EL0 syscall 0–7, register preservation, fork isolation, exec/kill/exit, binary UART and mailbox. |
| `python3 tests/syscall_qemu_test.py` | Interactive `syscall_test`, successful and incorrect input. |
| `python3 tests/vm_qemu_test.py --only vm` | Anonymous mmap, lazy allocation measured through buddy state, nested COW, mailbox/UART COW, permissions, exec and recycling. |
| `python3 tests/vm_qemu_test.py --only official` | Unmodified official `vm.img`, concurrent shell/video, framebuffer changes, fork/kill/exec and official mmap/fault tests. |

QEMU tests accept `--qemu PATH` and require `raspi4b`. They use pipes and the monitor, with no TCP sockets or desktop session. The VM test writes its serial transcript and framebuffer captures to `tests/results/`. Illegal-access children intentionally print segmentation faults; the test asserts exactly seven faults and that the parent and kernel shell survive.

Unlike lab-5, parent and child globals are isolated. Inherited process fixtures now verify that isolation and use the accessible physical timer for bounded waits rather than relying on shared writable globals. Tests inspect high kernel symbols through their physical addresses in the QEMU monitor.

`vm_test.bin` pauses at two lazy-allocation checkpoints and a binary UART checkpoint for the Python harness. Use the harness to run it; it is not a fully automatic interactive demo. `el0_demo.bin` and `syscall_test` are suitable for manual use.
