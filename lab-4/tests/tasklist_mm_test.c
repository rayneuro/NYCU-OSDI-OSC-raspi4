#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "mm.h"
#include "tasklist.h"

static uint64_t irq_flags;
uint64_t irq_save(void) { uint64_t old = irq_flags; irq_flags |= 0x80; return old; }
void irq_restore(uint64_t flags) { irq_flags = flags; }
void tfp_printf(char *fmt, ...) { (void)fmt; }
extern free_area_t free_area[MAX_ORDER + 1];
extern obj_allocator_t obj_alloc_pool[MAX_OBJ_ALLOCTOR_NUM];
static int order[8], count;
static void record(int value) { assert(!irq_flags); order[count++] = value; }
static void high(void) { record(1); }
static void middle(void) { record(2); }
static void low(void) { record(4); }
static void equal(void) { record(5); }
static void nesting(void)
{
    record(3);
    create_task(equal, 3);
    create_task(high, 1);
    create_task(low, 4);
    execute_tasks();
    /* Only high-priority work may nest this priority-3 callback. */
    assert(count == 2 && order[1] == 1);
    record(6);
}
static unsigned long free_pages(void)
{
    unsigned long sum = 0;
    for (int i = 0; i <= MAX_ORDER; ++i) sum += (unsigned long)free_area[i].nr_free << i;
    return sum;
}
int main(void)
{
    page_frame_count = 64;
    void *ram = aligned_alloc(PAGE_SIZE, page_frame_count * PAGE_SIZE);
    assert(ram);
    bookkeep = calloc(page_frame_count, sizeof(*bookkeep));
    assert(bookkeep);
    memory_base = (uintptr_t)ram;
    page_init();
    memory_reserve(memory_base, memory_base + 2 * PAGE_SIZE);
    free_area_init();
    __init_kmalloc();
    unsigned long before = free_pages();
    for (int run = 0; run < 64; ++run) {
        count = 0;
        create_task(low, 4);
        create_task(high, 1);
        create_task(middle, 2);
        for (task_t *t = task_head; t; t = t->next)
            assert((uintptr_t)t >= memory_base + 2 * PAGE_SIZE);
        execute_tasks();
        assert(count == 3 && order[0] == 1 && order[1] == 2 && order[2] == 4);
        assert(!task_head && !irq_flags);
        for (int i = 0; i < MAX_OBJ_ALLOCTOR_NUM; ++i) assert(!obj_alloc_pool[i].obj_used);
    }
    assert(free_pages() == before - 1); /* Existing slab caches one empty page. */
    count = 0;
    create_task(nesting, 3);
    execute_tasks();
    assert(count == 5 && order[0] == 3 && order[1] == 1 && order[2] == 6
           && order[3] == 5 && order[4] == 4);
    assert(!task_head && !irq_flags);

    /* An already-masked caller must retain its IRQ state. */
    irq_flags = 0x80;
    void *object = kmalloc(sizeof(task_t));
    assert(object && irq_flags == 0x80);
    kfree(object);
    assert(irq_flags == 0x80);
    irq_flags = 0;

    /* Fill the cached task slab, then exhaust the buddy pool. */
    void *held[PAGE_SIZE / sizeof(task_t)];
    for (unsigned i = 0; i < sizeof(held) / sizeof(*held); ++i) {
        held[i] = kmalloc(sizeof(task_t));
        assert(held[i]);
    }
    page_t *pages[64];
    unsigned n = 0;
    while ((pages[n] = buddy_block_alloc(0))) ++n;
    create_task(high, 1);
    assert(!task_head && !irq_flags); /* Allocation failure leaves queue intact. */
    while (n) buddy_block_free(pages[--n]);
    for (unsigned i = 0; i < sizeof(held) / sizeof(*held); ++i) kfree(held[i]);
    assert(free_pages() == before - 1);
    free(bookkeep);
    free(ram);
    puts("PASS task kmalloc/kfree: reserve exclusion, reuse, priorities, nesting, IRQ state, OOM");
}
