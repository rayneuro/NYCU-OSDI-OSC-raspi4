Need to modify the config.txt to enable uart0



printk rpi4b 會檢查 
MESS:00:00:05.347148:0: 'console=ttyAMA0,115200 earlyprintk loglevel=7'
MESS:00:00:05.473610:0: brfs: File read: 45 bytes
MESS:00:00:05.481116:0: brfs: File read: /mfs/sd/bootloader.img
MESS:00:00:05.483922:0: Loaded 'bootloader.img' to 0x60000 size 0xbcc
MESS:00:00:05.490104:0: Kernel relocated to 0x80000

## Lab 5: preemptive round-robin kernel threads

After memory/interrupt initialization, `thread_init()` registers the bootstrap
context as idle (ID 0). The shell runs as thread 1 and yields on each input-loop
iteration. Enter `thread_test` to create three workers, each printing its thread
ID and loop counter 0 through 9 with `schedule()` between iterations:

```text
Thread id: 2 0
Thread id: 3 0
Thread id: 4 0
Thread id: 2 1
Thread id: 3 1
Thread id: 4 1
...
Thread id: 4 9
```

Repeat `thread_test` to create more workers; IDs keep increasing. One worker
explicitly calls `thread_exit()`, and the others return to a trampoline which
calls it for them. Shell echo/prompt output may appear between demo lines.

- `thread_create(void (*entry)(void))` returns a TCB pointer or NULL. A single
  order-2 buddy allocation holds the TCB and a downward-growing stack, for 16 KiB
  total per worker. Stack top is aligned to 16 bytes. The pointer becomes invalid
  after idle reclaims the thread; do not retain it for later dereference.
- `schedule()` saves x19–x28, FP/LR, SP and the AAPCS64 callee-saved scalar FP
  registers d8–d15, plus FPCR/FPSR. `TPIDR_EL1` points to the running TCB, accessible through
  `get_current()` or `current_thread()`.
- All threads have equal priority. Runnable threads rotate through a FIFO;
  idle participates in the rotation so it can reap zombies while the shell is
  continuously runnable. When idle is alone, scheduling it again needs no context switch.
- `thread_exit()` marks the running thread DEAD, queues it for reclamation and
  switches away permanently. `kill_zombies()` only frees from idle's stack.
- Queue/allocator updates and switching mask IRQs. Resumed threads restore their
  own saved interrupt state; first entry restores the creation-time state.

Timer preemption is enabled by `thread_init()`, with a default **10 ms** quantum.
The scheduler is single-core (core 0); the other cores remain parked. A thread
need not call `schedule()` to let other threads run, but explicit yielding still
works. CPU time spent with IRQs masked or preemption disabled can extend a slice;
this is not a hard real-time deadline. Do not yield or exit from IRQ handlers,
bottom-half callbacks, or inside a `preempt_disable()` section. Join and stack guards are not implemented. EL0 processes use the same scheduler.

### Set the time quantum

In the kernel shell:

```text
quantum
quantum 5
preempt_test
```

`quantum` shows the current value; `quantum <ms>` accepts 1..1000 milliseconds.
The C API is `thread_set_quantum_ms(ms)` (1 on success, 0 on invalid input) and
`thread_get_quantum_ms()`. To change the boot default, edit
`THREAD_DEFAULT_QUANTUM_MS` in `inc/thread.h` and rebuild.

`preempt_test` creates three CPU-bound workers that never call `schedule()`.
They first spin until all three have started: without preemption this would
hang in the first worker. Each prints `Preempt id: <id> <0..9>` and checks live
caller-saved integer/FP values across timer interrupts. It also verifies that a
`preempt_disable()` section prevents other workers from entering their guarded
section, while IRQs remain enabled. One worker explicitly exits; the others
return normally. Printing order is not a direct measure of time slices: a worker
can print multiple times within a slice, and timer preemption can occur before
an explicit yield in the older `thread_test` demo.

### Timer and interrupt flow

CNTP (`cntp_cval_el0`) is programmed for the earlier of the running thread's slice
deadline and the earliest `SetTimeout` callback. Every dispatch starts a fresh
slice. Timer IRQ 30 sets a reschedule request and rearms the hardware; ordinary
timeout callbacks still execute through the existing priority bottom-half queue.
Expired callbacks awaiting their bottom half do not suppress scheduler ticks.

IRQ entry saves x0–x30, SP_EL0, ELR/SPSR, all q0–q31, and FPCR/FPSR in an aligned 800-byte
frame on the interrupted thread's stack. After bottom halves finish and GIC EOI
is written, only the outermost IRQ returning to EL1h or EL0t may invoke `schedule()`.
A resumed thread returns through its own IRQ frame and `eret` restores the
interrupted instruction and state. Nested IRQs never switch threads.

`preempt_disable()` / `preempt_enable()` protect the bottom-half priority state;
a pending switch is honored when the outermost protected section ends. Shared
buddy/object allocator metadata operations and bounded console chunks mask IRQs
to avoid partial updates on this single-core kernel; formatting is preemptible.

Build from this directory:

```sh
make clean
make kernel8.img
```

Run the integration test (QEMU must support `raspi4b`):

```sh
python3 tests/thread_qemu_test.py --qemu /path/to/qemu-system-aarch64
```

The test uses UART0 (the first QEMU serial port), checks three rounds of yielding
workers and no-yield workers at 1, 10, and 50 ms, including register preservation,
preemption guards, immediate/future timeouts, and invalid quantum values. It
inspects the stopped VM's zombie list and free-page count to verify worker
allocations were reclaimed, then checks shell response.
See `tests/README.md` for the existing allocator tests.

## User processes and system calls

`exec` in the shell prompts for the exact name shown by `ls`, then starts a
foreground EL0 process. The shell resumes when that PID exits. Each process has
its own 16 KiB kernel/TCB allocation, a 16 KiB user stack, and a page-aligned raw
program image. `exec` preserves the PID and replaces the image and user register
state. Invalid programs leave the calling process intact and return -1.

The ABI is `svc #0`, syscall number in x8, arguments in x0/x1/x2, result in x0:

| x8 | Call | Result |
|---|---|---|
| 0 | getpid() | Current PID |
| 1 | uart_read(buf, size) | Exactly size bytes, including NUL/CR |
| 2 | uart_write(buf, size) | Bytes written, without newline conversion |
| 3 | exec(name, argv) | No return on success; -1 on error; argv ignored |
| 4 | fork() | Child PID in parent, 0 in child, -1 on failure |
| 5 | exit(status) | Terminates caller; status currently ignored |
| 6 | mbox_call(channel, message) | 1 on firmware success, 0 on failure |
| 7 | kill(pid) | Terminates a user process; internal result 0/-1 |

Unknown syscall numbers return -1. `user/syscall.h` provides user-side wrappers.
Mailbox requests use aligned private bounce buffers and serialize hardware
transactions while keeping interrupts enabled. UART writers serialize whole
syscall writes, but waiting writers and readers remain schedulable. A kill is
honored at a safe syscall boundary or on return from an EL0 timer interrupt,
so a killed process cannot abandon a hardware lock. Idle reaps stacks and images;
forked processes retain their shared image until the last reference is released.

Syscalls enable IRQs after the complete exception frame has been saved. Both
user code and ordinary kernel code can be timer-preempted; queue/allocator
updates, hardware FIFO commits and exception return use short IRQ-masked sections.
Existing interrupt bottom halves protect their shared priority state from task
switching, but still permit higher-priority IRQs.

### Build and test

```sh
make -B kernel8.img
python3 tests/process_qemu_test.py --qemu /home/ray/qemu-11.0.0-rc1/build/qemu-system-aarch64
python3 tests/thread_qemu_test.py --qemu /home/ray/qemu-11.0.0-rc1/build/qemu-system-aarch64
```

The process test builds fixtures and a temporary cpio; it does **not** replace
`initramfs.cpio`. To include the fixtures in your own archive, use
`make process-fixtures initramfs` deliberately. For the Video Player, place its
provided `initramfs.cpio` in this directory, boot with it, then enter `exec` and
the program name reported by `ls`. Its actual archive has not been tested here.

### Current lab limitations

This is the pre-MMU lab model, not full UNIX address-space isolation. Fork copies
the user stack and exception frame while sharing the program image (including
writable globals). Stack-valued GPRs and aligned active stack slots are relocated
to the child's stack; numeric values coincidentally in the stack address range
can be relocated too, and packed/encoded pointers are unsupported. General POSIX
fork semantics require per-process virtual address spaces in a later lab.

Programs must be position-independent AArch64 raw binaries with the entry at
file offset zero; ELF and fixed-address binaries are unsupported. Include static
storage in the raw image or initialize it in user startup; the raw format has no
BSS-size metadata. Use `-mstrict-align` with MMU disabled. User pointers are trusted
and there is no memory protection. Arguments, wait/status collection, and signals
are outside this implementation. User synchronous faults terminate the process;
kernel faults print diagnostics and halt.
