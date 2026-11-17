#ifndef ADDRESS_H
#define ADDRESS_H
#include <stdint.h>
#ifdef VM_HOST_TEST
/* Allocator-only host tests use ordinary host pointers. */
#define KERNEL_BASE 0UL
#else
#define KERNEL_BASE 0xffff000000000000UL
#endif
static inline void *phys_to_virt(uintptr_t pa) { return (void *)(pa + KERNEL_BASE); }
static inline uintptr_t virt_to_phys(const volatile void *va) { return (uintptr_t)va - KERNEL_BASE; }
#endif
