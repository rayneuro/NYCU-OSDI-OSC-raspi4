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

#define O_CREAT 0100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define SEEK_SET 0
struct framebuffer_info { unsigned width, height, pitch, isrgb; };
static inline int open(const char *p, int f) { return syscall3(11,(unsigned long)p,f,0); }
static inline int close(int fd) { return syscall3(12,fd,0,0); }
static inline long write(int fd,const void *b,size_t n) { return syscall3(13,fd,(unsigned long)b,n); }
static inline long read(int fd,void *b,size_t n) { return syscall3(14,fd,(unsigned long)b,n); }
static inline int mkdir(const char *p,unsigned mode) { return syscall3(15,(unsigned long)p,mode,0); }
/* Lab 7 ignores source, flags, and data. */
static inline int mount(const char *src,const char *target,const char *fs,unsigned long flags,const void *data) {
	(void)flags; (void)data;
	return syscall3(16,(unsigned long)src,(unsigned long)target,(unsigned long)fs);
}
static inline int chdir(const char *p) { return syscall3(17,(unsigned long)p,0,0); }
static inline long lseek64(int fd,long offset,int whence) { return syscall3(18,fd,offset,whence); }
static inline int ioctl(int fd,unsigned long request,void *arg) { return syscall3(19,fd,request,(unsigned long)arg); }

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
