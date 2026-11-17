#include "vm.h"
#include "mm.h"
#include "thread.h"
#include "string.h"
#include "uart.h"

#define PAGE_MASK (PAGE_SIZE - 1UL)
#define MMAP_BASE 0x10000000UL
extern uint64_t empty_pgd[512];

static void copy_bytes(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
}

static void invalidate(void)
{
    asm volatile("dsb ishst\n tlbi vmalle1is\n dsb ish\n isb" ::: "memory");
}

void vm_switch(struct vm_space *vm)
{
    uintptr_t pgd = virt_to_phys(vm ? vm->pgd : empty_pgd);
    asm volatile("dsb ish\n msr ttbr0_el1, %0\n tlbi vmalle1is\n dsb ish\n isb"
                 :: "r"(pgd) : "memory");
}

/* All callers hold preemption disabled on this single-core kernel. The buddy
 * allocator also masks IRQs internally. Page tables and data own one reference. */
static page_t *frame(uintptr_t pa)
{
    return &bookkeep[(pa - memory_base) >> PAGE_SHIFT];
}

static void *new_page(void)
{
    page_t *p = buddy_block_alloc(0);
    if (!p) return NULL;
    p->refs = 1;
    void *v = phys_to_virt(p->phy_addr);
    strset(v, 0, PAGE_SIZE);
    return v;
}

static void put_page(uintptr_t pa)
{
    page_t *p = frame(pa);
    if (!--p->refs) buddy_block_free(p);
}

uint64_t *vm_walk(uint64_t *pgd, uintptr_t va, int alloc)
{
    if (va >= USER_LIMIT) return NULL;
    for (int shift = 39; shift > 12; shift -= 9) {
        uint64_t *entry = &pgd[(va >> shift) & 511];
        if (!(*entry & PTE_VALID)) {
            if (!alloc) return NULL;
            void *next = new_page();
            if (!next) return NULL;
            *entry = virt_to_phys(next) | PTE_PAGE;
        }
        if ((*entry & 3) != PTE_PAGE) return NULL;
        pgd = phys_to_virt(*entry & PTE_ADDR);
    }
    return &pgd[(va >> 12) & 511];
}

int vm_mappages(uint64_t *pgd, uintptr_t va, size_t size, uintptr_t pa, uint64_t attr)
{
    if (!size || ((va | pa | size) & PAGE_MASK) || va >= USER_LIMIT ||
        size > USER_LIMIT - va) return -1;
    for (size_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t *pte = vm_walk(pgd, va + off, 1);
        if (!pte || (*pte & PTE_VALID)) return -1;
        *pte = (pa + off) | attr | PTE_PAGE;
    }
    return 0;
}

struct vm_space *vm_create(void)
{
    preempt_disable();
    struct vm_space *vm = kmalloc(sizeof(*vm));
    if (vm) {
        vm->regions = NULL;
        vm->pgd = new_page();
        if (!vm->pgd) { kfree(vm); vm = NULL; }
    }
    preempt_enable();
    return vm;
}

static void free_table(uint64_t *table, unsigned level)
{
    for (unsigned i = 0; i < 512; ++i) {
        uint64_t e = table[i];
        if (!(e & PTE_VALID)) continue;
        if (level < 3) free_table(phys_to_virt(e & PTE_ADDR), level + 1);
        else if (!(e & PTE_EXTERNAL)) put_page(e & PTE_ADDR);
    }
    put_page(virt_to_phys(table));
}

void vm_destroy(struct vm_space *vm)
{
    if (!vm) return;
    preempt_disable();
    free_table(vm->pgd, 0);
    while (vm->regions) {
        struct vm_region *r = vm->regions;
        vm->regions = r->next;
        kfree(r);
    }
    kfree(vm);
    preempt_enable();
}

static struct vm_region *region_at(struct vm_space *vm, uintptr_t va)
{
    if (!vm || va >= USER_LIMIT) return NULL;
    for (struct vm_region *r = vm->regions; r; r = r->next)
        if (va >= r->start && va < r->end) return r;
    return NULL;
}

static int range_free(struct vm_space *vm, uintptr_t start, size_t len)
{
    if (start >= USER_LIMIT || !len || len > USER_LIMIT - start) return 0;
    for (struct vm_region *r = vm->regions; r; r = r->next)
        if (start < r->end && start + len > r->start) return 0;
    return 1;
}

static struct vm_region *add_region(struct vm_space *vm, uintptr_t va, size_t len, unsigned prot)
{
    if (!range_free(vm, va, len)) return NULL;
    struct vm_region *r = kmalloc(sizeof(*r));
    if (!r) return NULL;
    strset((char *)r, 0, sizeof(*r));
    r->start = va;
    r->end = va + len;
    r->prot = prot;
    struct vm_region **p = &vm->regions;
    while (*p && (*p)->start < va) p = &(*p)->next;
    r->next = *p;
    *p = r;
    return r;
}

int vm_add_image(struct vm_space *vm, const void *image, size_t size)
{
    if (!size || size > MMAP_BASE - PAGE_MASK) return -1;
    preempt_disable();
    /* Flat binaries have no ELF segment metadata; text/data share RWX. mmap
     * regions retain their requested permissions, including RO and NX. */
    struct vm_region *r = add_region(vm, 0, (size + PAGE_MASK) & ~PAGE_MASK, 7);
    if (r) { r->image = image; r->image_size = size; }
    int ok = r && add_region(vm, USER_STACK_BASE, 4 * PAGE_SIZE, PROT_READ | PROT_WRITE);
    preempt_enable();
    return ok ? 0 : -1;
}

static uint64_t attributes(unsigned prot)
{
    return PTE_PAGE | PTE_USER | PTE_AF | PTE_NG | PTE_PXN | (1UL << 2) |
           (3UL << 8) | ((prot & PROT_WRITE) ? 0 : PTE_RO) |
           ((prot & PROT_EXEC) ? 0 : PTE_UXN);
}

void vm_log(const char *kind, uintptr_t addr)
{
    preempt_disable();
    uart_puts((char *)kind);
    uart_hex(addr);
    uart_puts("\n");
    preempt_enable();
}

/* Resolve translation faults and COW both for EL0 faults and kernel copyout.
 * Never access user VA directly in EL1, especially after a schedule or fork. */
static int resolve(struct vm_space *vm, uintptr_t addr, unsigned access, int log)
{
    struct vm_region *r = region_at(vm, addr);
    if (!r || !(r->prot & access)) return -1;
    uintptr_t va = addr & ~PAGE_MASK;
    uint64_t *pte = vm_walk(vm->pgd, va, 1);
    if (!pte) return -1;
    if (!(*pte & PTE_VALID)) {
        if (r->physical) {
            *pte = (r->physical + va - r->start) | attributes(r->prot) | PTE_EXTERNAL;
        } else {
            void *page = new_page();
            if (!page) return -1;
            size_t off = va - r->start;
            if (r->image && off < r->image_size) {
                size_t n = r->image_size - off;
                if (n > PAGE_SIZE) n = PAGE_SIZE;
                copy_bytes(page, r->image + off, n);
            }
            *pte = virt_to_phys(page) | attributes(r->prot);
        }
        invalidate();
        if (log) vm_log("[Translation fault]: ", addr);
    } else if ((access & PROT_WRITE) && (*pte & PTE_RO)) {
        if (!(*pte & PTE_COW)) return -1;
        uintptr_t old = *pte & PTE_ADDR;
        uint64_t replacement = *pte & ~(PTE_RO | PTE_COW);
        if (frame(old)->refs > 1) {
            void *page = new_page();
            if (!page) return -1;
            copy_bytes(page, phys_to_virt(old), PAGE_SIZE);
            replacement = (replacement & ~PTE_ADDR) | virt_to_phys(page);
        }
        /* Break-before-make before changing a valid mapping's output PA. */
        *pte = 0;
        invalidate();
        *pte = replacement;
        invalidate();
        if ((replacement & PTE_ADDR) != old) put_page(old);
        if (log) vm_log("[Copy-on-write fault]: ", addr);
    }
    return 0;
}

int vm_fault(struct vm_space *vm, uintptr_t addr, uint64_t esr)
{
    unsigned ec = esr >> 26, status = esr & 63;
    /* Only lower-EL instruction/data translation or permission faults. */
    if ((ec != 0x20 && ec != 0x24) || (esr & (1UL << 10)) ||
        !((status >= 4 && status <= 7) || (status >= 12 && status <= 15))) return -1;
    unsigned access = ec == 0x20 ? PROT_EXEC : ((esr & 64) ? PROT_WRITE : PROT_READ);
    preempt_disable();
    int result = resolve(vm, addr, access, 1);
    preempt_enable();
    return result;
}

static void remove_region(struct vm_space *vm, struct vm_region *r)
{
    for (uintptr_t va = r->start; va < r->end; va += PAGE_SIZE) {
        uint64_t *pte = vm_walk(vm->pgd, va, 0);
        if (pte && (*pte & PTE_VALID)) {
            uint64_t old = *pte;
            *pte = 0;
            invalidate();
            if (!(old & PTE_EXTERNAL)) put_page(old & PTE_ADDR);
        }
    }
    struct vm_region **p = &vm->regions;
    while (*p != r) p = &(*p)->next;
    *p = r->next;
    kfree(r);
}

uintptr_t vm_mmap(struct vm_space *vm, uintptr_t hint, size_t len, unsigned prot, unsigned flags)
{
    if (!vm || !len || len > USER_LIMIT - PAGE_MASK || (prot & ~7U) ||
        !(flags & MAP_ANONYMOUS) || (flags & ~(MAP_ANONYMOUS | MAP_POPULATE | 2U)))
        return MAP_FAILED;
    len = (len + PAGE_MASK) & ~PAGE_MASK;
    preempt_disable();
    uintptr_t va = hint;
    if (!va || (va & PAGE_MASK) || !range_free(vm, va, len)) {
        va = MMAP_BASE;
        for (struct vm_region *r = vm->regions; r; r = r->next) {
            if (r->end <= va) continue;
            if (va < r->start && len <= r->start - va) break;
            va = r->end;
        }
    }
    struct vm_region *r = add_region(vm, va, len, prot);
    if (!r) va = MAP_FAILED;
    else if ((flags & MAP_POPULATE) && prot) {
        unsigned access = prot & PROT_WRITE ? PROT_WRITE : (prot & PROT_READ ? PROT_READ : PROT_EXEC);
        for (uintptr_t a = va; a < r->end; a += PAGE_SIZE) {
            if (resolve(vm, a, access, 0) < 0) {
                /* Roll back only the populated prefix, even for a huge VMA. */
                r->end = a + PAGE_SIZE;
                remove_region(vm, r);
                va = MAP_FAILED;
                break;
            }
        }
    }
    preempt_enable();
    return va;
}

int vm_map_framebuffer(struct vm_space *vm, uintptr_t pa, size_t size)
{
    if (!pa || (pa & PAGE_MASK) || !size || size > 0xfc000000UL ||
        pa > 0xfc000000UL - size) return -1;
    size = (size + PAGE_MASK) & ~PAGE_MASK;
    preempt_disable();
    struct vm_region *r = region_at(vm, pa);
    int result = -1;
    if (r && r->start == pa && r->physical == pa && r->end == pa + size) result = 0;
    else if (!r) {
        r = add_region(vm, pa, size, PROT_READ | PROT_WRITE);
        if (r) {
            r->physical = pa;
            result = vm_mappages(vm->pgd, pa, size, pa, attributes(r->prot) | PTE_EXTERNAL);
            if (result < 0) remove_region(vm, r);
            invalidate();
        }
    }
    preempt_enable();
    return result;
}

static int clone_table(uint64_t *src, uint64_t *dst, unsigned level)
{
    for (unsigned i = 0; i < 512; ++i) {
        uint64_t e = src[i];
        if (!(e & PTE_VALID)) continue;
        if (level < 3) {
            uint64_t *child = new_page();
            if (!child) return -1;
            dst[i] = virt_to_phys(child) | PTE_PAGE;
            if (clone_table(phys_to_virt(e & PTE_ADDR), child, level + 1) < 0) return -1;
        } else {
            if (!(e & PTE_EXTERNAL)) {
                if (!(e & PTE_RO)) src[i] = e = e | PTE_RO | PTE_COW;
                ++frame(e & PTE_ADDR)->refs;
            }
            dst[i] = e;
        }
    }
    return 0;
}

struct vm_space *vm_fork(struct vm_space *parent)
{
    preempt_disable();
    struct vm_space *child = vm_create();
    if (!child) goto out;
    struct vm_region **tail = &child->regions;
    for (struct vm_region *r = parent->regions; r; r = r->next) {
        *tail = kmalloc(sizeof(**tail));
        if (!*tail) goto fail;
        **tail = *r;
        (*tail)->next = NULL;
        tail = &(*tail)->next;
    }
    if (clone_table(parent->pgd, child->pgd, 0) < 0) goto fail;
    invalidate(); /* parent must also lose its previously writable TLB entries */
    goto out;
fail:
    invalidate();
    vm_destroy(child);
    child = NULL;
out:
    preempt_enable();
    return child;
}

int vm_user_check(struct vm_space *vm, uintptr_t addr, size_t len, unsigned access)
{
    if (!len) return 0;
    if (addr >= USER_LIMIT || len > USER_LIMIT - addr) return -1;
    uintptr_t end = addr + len;
    while (addr < end) {
        struct vm_region *r = region_at(vm, addr);
        if (!r || !(r->prot & access)) return -1;
        addr = r->end < end ? r->end : end;
    }
    return 0;
}

static int user_copy(struct vm_space *vm, uintptr_t user, void *kernel, size_t len, int write)
{
    unsigned access = write ? PROT_WRITE : PROT_READ;
    if (vm_user_check(vm, user, len, access) < 0) return -1;
    unsigned char *k = kernel;
    while (len) {
        size_t n = PAGE_SIZE - (user & PAGE_MASK);
        if (n > len) n = len;
        preempt_disable();
        if (resolve(vm, user, access, 1) < 0) { preempt_enable(); return -1; }
        uint64_t *pte = vm_walk(vm->pgd, user, 0);
        unsigned char *p = phys_to_virt((*pte & PTE_ADDR) + (user & PAGE_MASK));
        if (write) copy_bytes(p, k, n);
        else copy_bytes(k, p, n);
        preempt_enable();
        len -= n; user += n; k += n;
    }
    return 0;
}

int vm_copyin(struct vm_space *vm, void *dst, uintptr_t src, size_t len)
{ return user_copy(vm, src, dst, len, 0); }
int vm_copyout(struct vm_space *vm, uintptr_t dst, const void *src, size_t len)
{ return user_copy(vm, dst, (void *)src, len, 1); }
int vm_copystr(struct vm_space *vm, char *dst, uintptr_t src, size_t capacity)
{
    for (size_t i = 0; i < capacity; ++i) {
        if (vm_copyin(vm, dst + i, src + i, 1) < 0) return -1;
        if (!dst[i]) return 0;
    }
    return -1;
}
