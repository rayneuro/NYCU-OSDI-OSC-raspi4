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
#endif
