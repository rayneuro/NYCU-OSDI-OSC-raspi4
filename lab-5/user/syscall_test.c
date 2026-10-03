/* Interactive EL0 smoke test. All kernel services use svc #0 via syscall.h.
 * The shared flags intentionally use this lab's pre-MMU fork image model. */
#include "syscall.h"

static volatile unsigned fork_ready = 0xffffffff;
static volatile unsigned spinner_ready = 0xffffffff;

static void say(const char *s)
{
    size_t size = 0;
    while (s[size]) ++size;
    uart_write(s, size);
}

static void number(unsigned value)
{
    char digits[10];
    unsigned n = 0;
    do { digits[n++] = '0' + value % 10; value /= 10; } while (value);
    while (n) uart_write(&digits[--n], 1);
}

static void require(int ok, const char *name)
{
    if (!ok) {
        say("[FAIL] ");
        say(name);
        say("\r\n");
        exit(1);
    }
}

__attribute__((section(".text.entry"), noreturn))
void _start(void)
{
    say("\r\n=== EL0 syscall test ===\r\n");
    int pid = getpid();
    require(pid > 0 && getpid() == pid, "getpid");
    say("[PASS] 0 getpid: pid=");
    number(pid);
    say("\r\n");

    static const char message[] = "[PASS] 2 uart_write\r\n";
    require(uart_write(message, sizeof(message) - 1) == sizeof(message) - 1,
            "uart_write byte count");
    require(uart_read(0, 0) == 0 && uart_write(0, 0) == 0, "zero-length UART");
    require(syscall3(99, 0, 0, 0) == -1, "unknown syscall");
    require(exec("__no_such_syscall_test_program__", 0) == -1, "failed exec");
    say("[PASS] invalid syscall, failed exec, zero-length UART\r\n");

    unsigned int box[7] __attribute__((aligned(16))) = {
        28, 0, 0x00010002, 4, 0, 0, 0
    };
    require(mbox_call(8, box) == 1 && box[1] == 0x80000000 && box[5] != 0,
            "mbox_call board revision");
    say("[PASS] 6 mbox_call: board revision=");
    number(box[5]);
    say("\r\n");

    volatile unsigned local = 42;
    volatile unsigned *pointer = &local;
    int child = fork();
    require(child >= 0, "fork allocation");
    if (child == 0) {
        require(getpid() != pid && *pointer == 42, "fork child registers/stack");
        *pointer = 99;
        say("[PASS] 4 fork child: return=0, independent stack\r\n");
        fork_ready = 1;
        /* On success this process continues in syscall_exec_test.bin. */
        exec("syscall_exec_test.bin", 0);
        say("[FAIL] exec replacement returned\r\n");
        exit(1);
    }
    /* No syscall/yield in this barrier: a timer must schedule the child. */
    while (fork_ready != 1) asm volatile("nop");
    require(*pointer == 42, "fork parent stack isolation");
    say("[PASS] 4 fork parent: child pid=");
    number(child);
    say("\r\n");

    child = fork();
    require(child >= 0, "spinner fork");
    if (child == 0) {
        spinner_ready = 1;
        for (;;) asm volatile("nop");
    }
    while (spinner_ready != 1) asm volatile("nop");
    require(syscall3(7, child, 0, 0) == 0, "kill spinner");
    say("[PASS] 7 kill: CPU-bound child, EL0 timer preemption\r\n");

    say("[INPUT] Type test then press Enter: ");
    char input[5];
    require(uart_read(input, sizeof(input)) == sizeof(input), "uart_read byte count");
    require(input[0] == 't' && input[1] == 'e' && input[2] == 's' && input[3] == 't'
            && (input[4] == '\r' || input[4] == '\n'), "uart_read: expected test + Enter");
    say("\r\n[PASS] 1 uart_read: 5 bytes\r\n");
    say("[DONE] Parent checks passed; child exec PASS should also appear.\r\n");
    say("[EXIT] 5 exit: returning to kernel shell\r\n");
    exit(0);
}
