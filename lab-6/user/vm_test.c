#include "syscall.h"
#include <stdint.h>
static volatile unsigned global = 42;
static unsigned char bss[5000];
static unsigned int mailbox[7] __attribute__((aligned(4096))) = {28, 0, 0x10002, 4, 0, 0, 0};
static void say(const char *s) { size_t n = 0; while (s[n]) ++n; uart_write(s, n); }
static void check(int ok, const char *s) { if (!ok) { say("FAIL "); say(s); say("\n"); exit(1); } }
static void *anon(void *hint, size_t n, int prot, int extra)
{ return mmap(hint, n, prot, MAP_ANONYMOUS | extra, -1, 0); }

__attribute__((section(".text.entry"), noreturn)) void _start(void)
{
    uintptr_t sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    check(sp >= 0xffffffffb000UL && sp < 0xfffffffff000UL, "stack VA");
    for (unsigned i = 0; i < sizeof(bss); ++i) check(bss[i] == 0, "BSS");
    check(uart_write((void *)0xffff000000080000UL, 4) == (size_t)-1, "kernel syscall pointer");
    check(uart_read((void *)0x8000000000000000UL, 1) == (size_t)-1, "invalid copyout");
    check(anon(0, 0, 3, 0) == MAP_FAILED, "zero length");
    check(anon(0, (size_t)-1, 3, 0) == MAP_FAILED, "overflow length");
    check(anon(0, 4096, 8, 0) == MAP_FAILED, "invalid prot");
    check(mmap(0, 4096, 3, 0, -1, 0) == MAP_FAILED, "non anonymous");
    volatile char *hint = anon((void *)0x20000000UL, 4097, 3, 0);
    check(hint == (void *)0x20000000UL, "valid hint");
    check(hint[0] == 0 && hint[8191] == 0, "rounding zero fill");
    hint[8191] = 11;
    void *overlap = anon((void *)hint, 4096, 3, 0);
    check(overlap != MAP_FAILED && overlap != (void *)hint, "overlap hint");
    void *unaligned = anon((void *)0x25000001UL, 4096, 3, 0);
    check(unaligned != MAP_FAILED && !((uintptr_t)unaligned & 4095), "unaligned hint");
    volatile char *huge = anon((void *)0x40000000UL, 512UL << 20, 3, 0);
    check(huge == (void *)0x40000000UL, "sparse mmap");
    say("CHECK lazy allocated\n");
    char ack;
    uart_read(&ack, 1); /* host checks physical usage before touching 512 MiB */
    check(huge[0] == 0 && huge[(512UL << 20) - 1] == 0, "sparse zero fill");
    huge[0] = 31; huge[(512UL << 20) - 1] = 32;
    say("CHECK lazy touched\n");
    uart_read(&ack, 1);
    say("PASS mmap hints, rounding, zero fill, lazy allocation\n");

    volatile unsigned *rw = anon(0, 8192, 3, MAP_POPULATE);
    check(rw != MAP_FAILED && rw[0] == 0 && rw[1024] == 0, "populate");
    rw[0] = 17;
    int pid = fork();
    check(pid >= 0, "fork");
    if (!pid) {
        check(global == 42 && rw[0] == 17 && hint[8191] == 11, "child snapshot");
        global = 99; rw[0] = 88;
        /* No user write to the aligned mailbox page before the syscall. */
        check(mbox_call(8, mailbox) == 1 && mailbox[5], "mailbox COW copyout");
        int grand = fork();
        check(grand >= 0, "grandchild fork");
        if (!grand) {
            check(global == 99 && rw[0] == 88, "grandchild snapshot");
            global = 101; rw[0] = 100;
            say("PASS COW grandchild\n");
            exit(0);
        }
        delay_ms(300);
        check(global == 99 && rw[0] == 88, "child isolation");
        say("PASS COW child and mailbox\n");
        exec("exec_test.bin", 0);
        check(0, "exec replacement returned");
    }
    delay_ms(700);
    check(global == 42 && rw[0] == 17 && mailbox[1] == 0 && mailbox[5] == 0, "parent isolation");
    rw[0] = 19; /* sole remaining COW reference */
    check(rw[0] == 19, "last reference");
    say("PASS COW parent and last reference\n");

    unsigned *code = anon(0, 4096, 7, 0);
    check(code != MAP_FAILED, "exec mapping");
    code[0] = 0x52800540; /* mov w0, #42 */
    code[1] = 0xd65f03c0; /* ret */
    check(((int (*)(void))code)() == 42, "PROT_EXEC");
    say("PASS executable mapping\n");
    for (unsigned mode = 0; mode < 7; ++mode) {
        pid = fork();
        check(pid >= 0, "protection fork");
        if (!pid) {
            say("CHECK protection child\n");
            if (mode == 0) *(volatile char *)anon(0, 4096, PROT_READ, MAP_POPULATE) = 1;
            if (mode == 1) *(volatile char *)anon(0, 4096, PROT_READ, 0) = 1;
            if (mode == 2) { volatile char v = *(volatile char *)anon(0, 4096, PROT_NONE, 0); (void)v; }
            if (mode == 3) { unsigned *nx = anon(0, 4096, 3, 0); nx[0] = 0xd65f03c0; ((void (*)(void))nx)(); }
            if (mode == 4) { volatile char v = *(volatile char *)0xffff000000080000UL; (void)v; }
            if (mode == 5) { volatile char v = *(volatile char *)0xffffffffa000UL; (void)v; }
            if (mode == 6) { volatile char v = *(volatile char *)0x123456789000UL; (void)v; }
            check(0, "protection allowed access");
        }
        delay_ms(300);
    }
    say("PASS protection parent survives\n");
    /* Kernel copyout spans two noncontiguous pages and must split both COW PTEs. */
    char *cross = anon(0, 8192, 3, MAP_POPULATE);
    check(cross != MAP_FAILED, "cross-page allocation");
    pid = fork();
    check(pid >= 0, "UART fork");
    if (!pid) {
        say("READ cross-page ready\n");
        check(uart_read(cross + 4094, 4) == 4, "cross-page read");
        check(cross[4094] == 'A' && cross[4095] == 0 && cross[4096] == '\r' && cross[4097] == 'Z', "cross-page bytes");
        say("PASS cross-page UART COW\n");
        exit(0);
    }
    delay_ms(700);
    check(cross[4094] == 0 && cross[4097] == 0, "UART parent isolation");
    say("PASS VM complete\n");
    exit(0);
}
