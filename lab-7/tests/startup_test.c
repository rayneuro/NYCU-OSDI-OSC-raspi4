#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "startup.h"
#include "mm.h"

/* Host tests have no asynchronous interrupts. */
uint64_t irq_save(void) { return 0; }
void irq_restore(uint64_t flags) { (void)flags; }

extern free_area_t free_area[MAX_ORDER + 1];
void tfp_printf(char *fmt, ...) { (void)fmt; }

static unsigned long free_pages(void)
{
    unsigned long total = 0;
    for (int order = 0; order <= MAX_ORDER; order++)
        total += (unsigned long)free_area[order].nr_free << order;
    return total;
}

int main(void)
{
    startup_init(0x1000, 0x10000);
    assert(startup_reserve(0x3000, 0x2000) == 0);
    assert(startup_reserve(0x2000, 0x2000) == 0);
    assert(startup_alloc(0x1800, 0x1000) == (void *)0x5000);
    assert(startup_alloc(1, 0x1000) == (void *)0x7000);
    assert(!startup_alloc(0, 8));
    assert(!startup_alloc(1, 3));
    assert(!startup_alloc(SIZE_MAX, 8));
    assert(startup_reserve(UINTPTR_MAX, 2) < 0);
    assert(!startup_alloc(0x10000, 8));
    startup_finish();
    assert(!startup_alloc(1, 8));
    assert(startup_reserve(1, 1) < 0);

    startup_init(0x1000, 0x10000);
    for (int i = 0; i < STARTUP_MAX_BLOCKS; i++)
        assert(startup_reserve(0x1000, 1) == 0);
    assert(!startup_alloc(1, 8));
    assert(startup_block_count == STARTUP_MAX_BLOCKS);
    startup_init(UINTPTR_MAX - 4, UINTPTR_MAX);
    assert(!startup_alloc(1, 8));

    /* Exercise real metadata allocation and the startup-to-buddy handoff. */
    page_frame_count = 1031; // intentionally not a power of two
    void *ram = aligned_alloc(PAGE_SIZE, page_frame_count * PAGE_SIZE);
    assert(ram);
    memory_base = (uintptr_t)ram;
    startup_init(memory_base, memory_base + page_frame_count * PAGE_SIZE);
    assert(startup_reserve(memory_base, PAGE_SIZE + 1) == 0);
    assert(startup_reserve(memory_base + 200 * PAGE_SIZE + 1, PAGE_SIZE) == 0);
    bookkeep = startup_alloc(page_frame_count * sizeof(*bookkeep), PAGE_SIZE);
    assert(bookkeep);
    page_init();
    for (unsigned int i = 0; i < startup_block_count; i++)
        memory_reserve(startup_blocks[i].start,
                       startup_blocks[i].start + startup_blocks[i].size);
    free_area_init();
    startup_finish();
    unsigned long expected = 0;
    for (unsigned long pfn = 0; pfn < page_frame_count; pfn++) {
        int reserved = 0;
        uintptr_t address = memory_base + pfn * PAGE_SIZE;
        for (unsigned int i = 0; i < startup_block_count; i++)
            if (address < startup_blocks[i].start + startup_blocks[i].size &&
                address + PAGE_SIZE > startup_blocks[i].start)
                reserved = 1;
        assert((bookkeep[pfn].used == Taken) == reserved);
        expected += !reserved;
    }
    assert(free_pages() == expected);
    page_t *allocated[1031];
    unsigned long count = 0;
    page_t *page;
    while ((page = buddy_block_alloc(0))) {
        assert(page->pfn < page_frame_count);
        for (unsigned int i = 0; i < startup_block_count; i++)
            assert(page->phy_addr >= startup_blocks[i].start + startup_blocks[i].size ||
                   page->phy_addr + PAGE_SIZE <= startup_blocks[i].start);
        allocated[count++] = page;
        assert(count <= expected);
    }
    assert(count == expected);
    while (count)
        buddy_block_free(allocated[--count]);
    assert(free_pages() == expected);
    /* Splitting and merging after exhaustion must preserve all free pages. */
    for (int order = 0; order <= MAX_ORDER; order++) {
        page = buddy_block_alloc(order);
        if (page) {
            assert(free_pages() == expected - (1UL << order));
            buddy_block_free(page);
            assert(free_pages() == expected);
        }
    }
    /* Reusing a freed slab object must advance the free-list head. Otherwise
     * consecutive image descriptors alias after an exec/exit/restart cycle. */
    __init_kmalloc();
    for (unsigned round = 0; round < 32; ++round) {
        unsigned long *a = kmalloc(16);
        unsigned long *b = kmalloc(16);
        assert(a && b && a != b);
        a[0] = 0x11223344;
        b[0] = 0x55667788;
        assert(a[0] == 0x11223344);
        kfree(a);
        kfree(b);
    }
    assert(free_pages() == expected - 1); /* one cached slab */
    free(ram);
    puts("startup, buddy handoff and slab reuse tests passed");
}
