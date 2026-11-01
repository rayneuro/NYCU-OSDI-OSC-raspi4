#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H
#include <stddef.h>
static inline long syscall3(long nr, unsigned long a, unsigned long b, unsigned long c)
{
    register unsigned long x0 asm("x0") = a;
    register unsigned long x1 asm("x1") = b;
    register unsigned long x2 asm("x2") = c;
    register unsigned long x8 asm("x8") = nr;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory", "cc");
    return x0;
}
static inline int getpid(void) { return syscall3(0, 0, 0, 0); }
static inline size_t uart_read(char *b, size_t n) { return syscall3(1, (unsigned long)b, n, 0); }
static inline size_t uart_write(const char *b, size_t n) { return syscall3(2, (unsigned long)b, n, 0); }
static inline int exec(const char *n, char *const a[]) { return syscall3(3, (unsigned long)n, (unsigned long)a, 0); }
static inline int fork(void) { return syscall3(4, 0, 0, 0); }
static inline __attribute__((noreturn)) void exit(int s) { syscall3(5, s, 0, 0); for (;;) {} }
static inline int mbox_call(unsigned char c, unsigned int *b) { return syscall3(6, c, (unsigned long)b, 0); }
static inline void kill(int p) { syscall3(7, p, 0, 0); }

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE 0x8000
#define MAP_FAILED ((void *)-1UL)
static inline void *mmap(void *addr, size_t len, int prot, int flags, int fd, int offset)
{
    register unsigned long x0 asm("x0") = (unsigned long)addr;
    register unsigned long x1 asm("x1") = len;
    register unsigned long x2 asm("x2") = prot;
    register unsigned long x3 asm("x3") = flags;
    register unsigned long x4 asm("x4") = fd;
    register unsigned long x5 asm("x5") = offset;
    register unsigned long x8 asm("x8") = 10;
    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8) : "memory", "cc");
    return (void *)x0;
}
static inline void delay_ms(unsigned ms)
{
    unsigned long now, hz, end;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(hz));
    asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    end = now + hz * ms / 1000;
    do { asm volatile("mrs %0, cntpct_el0" : "=r"(now)); } while (now < end);
}
#endif
