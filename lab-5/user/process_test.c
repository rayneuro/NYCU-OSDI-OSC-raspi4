#include "syscall.h"
static volatile unsigned child_started = 0xffffffff;
extern int user_register_probe(void);
static void say(const char *s) { size_t n = 0; while (s[n]) ++n; uart_write(s, n); }
static void check(int ok) { if (!ok) { say("FAIL process\n"); exit(1); } }
__attribute__((section(".text.entry"), noreturn)) void _start(void)
{
    check(user_register_probe());
    int parent = getpid();
    check(parent > 1);
    check(syscall3(99, 0, 0, 0) == -1);
    check(exec("missing.bin", 0) == -1);
    check(uart_write(0, 0) == 0 && uart_read(0, 0) == 0);
    volatile unsigned local = 42;
    volatile unsigned *ptr = &local;
    int child = fork();
    check(child >= 0);
    if (!child) {
        check(getpid() != parent && *ptr == 42);
        *ptr = 99;
        say("PASS fork child\n");
        check(exec("exec_test.bin", 0) == -1); /* successful exec never returns */
        exit(1);
    }
    check(*ptr == 42);
    say("PASS fork parent\n");
    unsigned int box[7] __attribute__((aligned(16))) = {28, 0, 0x10002, 4, 0, 0, 0};
    check(mbox_call(8, box) == 1 && box[1] == 0x80000000 && box[5]);
    say("PASS mailbox\n");
    child = fork();
    check(child >= 0);
    if (!child) { child_started = 1; for (;;) asm volatile("nop"); }
    /* Parent and spinning child must both be timer-preemptible. */
    while (child_started != 1) asm volatile("nop");
    kill(child);
    say("PASS kill spinner\n");
    say("READ ready\n");
    char bytes[4];
    check(uart_read(bytes, 4) == 4);
    check(bytes[0] == 'A' && bytes[1] == 0 && bytes[2] == '\r' && bytes[3] == 'Z');
    check(uart_write(bytes, 4) == 4);
    say("\nPASS read/write\n");
    say("PASS process complete\n");
    exit(0);
}
