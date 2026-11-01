#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include <stddef.h>
/* Must match start.S: padding slot now holds SP_EL0. */
struct trap_frame {
    uint64_t x[31], sp, pc, pstate;
    unsigned char simd[512];
    uint64_t fpcr, fpsr;
};
_Static_assert(offsetof(struct trap_frame, sp) == 248, "SP_EL0 offset");
_Static_assert(offsetof(struct trap_frame, pc) == 256, "ELR offset");
_Static_assert(offsetof(struct trap_frame, simd) == 272, "SIMD offset");
_Static_assert(offsetof(struct trap_frame, fpcr) == 784, "FPCR offset");
_Static_assert(sizeof(struct trap_frame) == 800, "exception frame size");
void syscall_dispatch(struct trap_frame *f);
/* start runs on the new process's kernel stack and enters user mode. */
int process_spawn(const char *name, void (*start)(void));
void process_return(struct trap_frame *f) __attribute__((noreturn));
#endif
