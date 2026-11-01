#ifndef VM_H
#define VM_H
#include <stdint.h>
#include <stddef.h>
#include "address.h"
#define USER_LIMIT (1UL << 48)
#define USER_STACK_TOP 0xfffffffff000UL
#define USER_STACK_BASE (USER_STACK_TOP - 4 * 4096)
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_ANONYMOUS 0x20
#define MAP_POPULATE 0x8000
#define MAP_FAILED (~0UL)
#define PTE_VALID 1UL
#define PTE_PAGE 3UL
#define PTE_USER (1UL << 6)
#define PTE_RO (1UL << 7)
#define PTE_AF (1UL << 10)
#define PTE_NG (1UL << 11)
#define PTE_PXN (1UL << 53)
#define PTE_UXN (1UL << 54)
#define PTE_COW (1UL << 55) /* software bit */
#define PTE_EXTERNAL (1UL << 56) /* framebuffer: never free or COW */
#define PTE_ADDR 0x0000fffffffff000UL

struct vm_region {
    uintptr_t start, end;
    unsigned prot;
    const unsigned char *image; /* immutable bytes in reserved initramfs */
    size_t image_size;
    uintptr_t physical; /* nonzero for external framebuffer mappings */
    struct vm_region *next;
};
struct vm_space {
    uint64_t *pgd;
    struct vm_region *regions;
};
struct vm_space *vm_create(void);
void vm_destroy(struct vm_space *vm);
struct vm_space *vm_fork(struct vm_space *parent);
void vm_switch(struct vm_space *vm);
uint64_t *vm_walk(uint64_t *pgd, uintptr_t va, int alloc);
int vm_mappages(uint64_t *pgd, uintptr_t va, size_t size, uintptr_t pa, uint64_t attr);
int vm_add_image(struct vm_space *vm, const void *image, size_t size);
uintptr_t vm_mmap(struct vm_space *vm, uintptr_t hint, size_t len, unsigned prot, unsigned flags);
int vm_map_framebuffer(struct vm_space *vm, uintptr_t pa, size_t size);
int vm_fault(struct vm_space *vm, uintptr_t addr, uint64_t esr);
int vm_user_check(struct vm_space *vm, uintptr_t addr, size_t len, unsigned access);
int vm_copyin(struct vm_space *vm, void *dst, uintptr_t src, size_t len);
int vm_copyout(struct vm_space *vm, uintptr_t dst, const void *src, size_t len);
int vm_copystr(struct vm_space *vm, char *dst, uintptr_t src, size_t capacity);
void vm_log(const char *kind, uintptr_t addr);
#endif
