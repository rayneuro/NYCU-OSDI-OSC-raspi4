#include "process.h"
#include "thread.h"
#include "cpio.h"
#include "utils.h"
#include "string.h"
#include "uart.h"
#include "mailbox.h"
#include "irq.h"
#include <stddef.h>
#define STACK_ORDER 2
static int writer_busy;
#define STACK_BYTES (PAGE_SIZE << STACK_ORDER)

static void copy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
}

struct process_image {
    page_t *pages;
    unsigned refs;
};

void process_image_put(struct process_image *im)
{
    if (!im) return;
    uint64_t flags = irq_save();
    if (!--im->refs) {
        buddy_block_free(im->pages);
        kfree(im);
    }
    irq_restore(flags);
}

static struct process_image *image(const char *name)
{
    if (!name || !cpio_addr) return 0;
    char *file = findFile((char *)name);
    if (!file) return 0;
    struct cpio_header *h = (void *)file;
    unsigned long size = utils_atoi(h->c_filesize, 8);
    unsigned long offset = sizeof(*h) + utils_atoi(h->c_namesize, 8);
    utils_align(&offset, 4);
    unsigned char *entry = (void *)(file + offset);
    if ((utils_atoi(h->c_mode, 8) & 0170000) != 0100000 || size < 4 ||
        ((uintptr_t)entry & 3) ||
        (entry[0] == 0x7f && entry[1] == 'E' && entry[2] == 'L' && entry[3] == 'F'))
        return 0;
    unsigned order = 0;
    while (order < MAX_ORDER && (PAGE_SIZE << order) < size) ++order;
    if ((PAGE_SIZE << order) < size) return NULL;
    struct process_image *im = kmalloc(sizeof(*im));
    if (!im) return NULL;
    im->pages = buddy_block_alloc(order);
    if (!im->pages) { kfree(im); return NULL; }
    im->refs = 1;
    void *dst = (void *)im->pages->phy_addr;
    strset(dst, 0, PAGE_SIZE << order);
    copy(dst, entry, size);
    /* Page alignment preserves ADRP's PC-relative page addressing. */
    uint64_t ctr;
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
    unsigned line = 4U << ((ctr >> 16) & 15);
    for (uintptr_t a = (uintptr_t)dst; a < (uintptr_t)dst + size; a += line)
        asm volatile("dc cvau, %0" :: "r"(a) : "memory");
    asm volatile("dsb ish\n ic iallu\n dsb ish\n isb" ::: "memory");
    return im;
}

static void user_start(void)
{
    if (get_current()->killed) thread_exit();
    process_return(get_current()->user_frame);
}

static thread_t *allocate_process(void)
{
    thread_t *t = thread_alloc(user_start);
    if (!t) return NULL;
    t->user_stack = buddy_block_alloc(STACK_ORDER);
    if (!t->user_stack) {
        buddy_block_free(t->allocation);
        return NULL;
    }
    /* Keep the initial exception frame above the bootstrap C stack. */
    t->context.sp -= sizeof(struct trap_frame);
    t->user_frame = (void *)t->context.sp;
    strset((char *)t->user_stack->phy_addr, 0, STACK_BYTES);
    return t;
}

static void setup(struct trap_frame *f, thread_t *t, uintptr_t pc)
{
    strset((char *)f, 0, sizeof(*f));
    f->pc = pc;
    f->sp = t->user_stack->phy_addr + STACK_BYTES;
    /* EL0t with IRQ enabled. */
    f->pstate = 0;
}

int process_spawn(const char *name)
{
    struct process_image *im = image(name);
    if (!im) return -1;
    thread_t *t = allocate_process();
    if (!t) { process_image_put(im); return -1; }
    t->user_image = im;
    setup(t->user_frame, t, im->pages->phy_addr);
    int pid = t->id;
    thread_publish(t);
    return pid;
}

static int process_fork(struct trap_frame *f)
{
    thread_t *parent = get_current();
    uintptr_t base = parent->user_stack->phy_addr;
    if (f->sp < base || f->sp > base + STACK_BYTES) return -1;
    thread_t *child = allocate_process();
    if (!child) return -1;
    uint64_t flags = irq_save();
    child->user_image = parent->user_image;
    ++child->user_image->refs;
    irq_restore(flags);
    copy((void *)child->user_stack->phy_addr, (void *)base, STACK_BYTES);
    struct trap_frame *cf = child->user_frame;
    copy(cf, f, sizeof(*cf));
    uintptr_t delta = child->user_stack->phy_addr - base;
    /* Without VM, stack addresses change. Relocate stack references in saved
     * GPRs and aligned stack slots (including the frame-pointer chain). */
    for (unsigned i = 0; i < 31; ++i)
        if (cf->x[i] >= base && cf->x[i] <= base + STACK_BYTES) cf->x[i] += delta;
    uint64_t *words = (void *)child->user_stack->phy_addr;
    for (unsigned i = (f->sp - base) / 8; i < STACK_BYTES / 8; ++i)
        if (words[i] >= base && words[i] <= base + STACK_BYTES) words[i] += delta;
    cf->sp += delta;
    cf->x[0] = 0;
    int pid = child->id;
    thread_publish(child);
    return pid;
}

void syscall_dispatch(struct trap_frame *f)
{
    uint64_t result = (uint64_t)-1;
    switch (f->x[8]) {
    case 0: result = get_current()->id; break;
    case 1: {
        char *buf = (void *)f->x[0];
        size_t size = f->x[1];
        if (!buf && size) break;
        for (size_t i = 0; i < size; ++i) {
            while (!uart_async_read(buf + i)) {
                if (get_current()->killed) thread_exit();
                schedule();
            }
        }
        result = size;
        break;
    }
    case 2: {
        const char *buf = (void *)f->x[0];
        size_t size = f->x[1];
        if (!buf && size) break;
        for (;;) {
            uint64_t flags = irq_save();
            if (!writer_busy) {
                writer_busy = 1;
                irq_restore(flags);
                break;
            }
            irq_restore(flags);
            if (get_current()->killed) thread_exit();
            schedule();
        }
        size_t written = 0;
        while (written < size && !get_current()->killed)
            uart_write_char(buf[written++]);
        uint64_t flags = irq_save();
        writer_busy = 0;
        irq_restore(flags);
        result = written;
        break;
    }
    case 3: {
        struct process_image *im = image((const char *)f->x[0]);
        if (!im) break;
        thread_t *t = get_current();
        struct process_image *old = t->user_image;
        t->user_image = im;
        setup(f, t, im->pages->phy_addr);
        process_image_put(old);
        return;
    }
    case 4: result = process_fork(f); break;
    case 5: thread_exit();
    case 6: {
        /* Firmware needs a 16-byte-aligned physical buffer. Use a private
         * bounce buffer so a regular unsigned-int array is sufficient. */
        uint32_t *user = (void *)f->x[1];
        result = 0;
        if (!user || (f->x[1] & 3) || f->x[0] >= 16) break;
        size_t size = user[0];
        if (size < 12 || (size & 3) || size > (PAGE_SIZE << MAX_ORDER)) break;
        unsigned order = 0;
        while ((PAGE_SIZE << order) < size) ++order;
        page_t *page = buddy_block_alloc(order);
        if (!page) break;
        void *buf = (void *)page->phy_addr;
        copy(buf, user, size);
        result = mailbox_call(f->x[0], buf);
        copy(user, buf, size);
        buddy_block_free(page);
        break;
    }
    case 7: result = thread_kill(f->x[0]); break;
    }
    f->x[0] = result;
}
