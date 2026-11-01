#include "process.h"
#include "thread.h"
#include "cpio.h"
#include "utils.h"
#include "string.h"
#include "uart.h"
#include "mailbox.h"
#include "irq.h"
#include "vm.h"

static int writer_busy;

static struct vm_space *load_image(const char *name)
{
    if (!name || !cpio_addr) return NULL;
    char *file = findFile((char *)name);
    if (!file) return NULL;
    struct cpio_header *h = (void *)file;
    size_t size = utils_atoi(h->c_filesize, 8);
    unsigned long offset = sizeof(*h) + utils_atoi(h->c_namesize, 8);
    utils_align(&offset, 4);
    const unsigned char *entry = (void *)(file + offset);
    if ((utils_atoi(h->c_mode, 8) & 0170000) != 0100000 || size < 4 ||
        entry > (const unsigned char *)cpio_end || size > (size_t)((unsigned char *)cpio_end - entry) ||
        (entry[0] == 0x7f && entry[1] == 'E' && entry[2] == 'L' && entry[3] == 'F'))
        return NULL;
    struct vm_space *vm = vm_create();
    if (vm && vm_add_image(vm, entry, size) < 0) { vm_destroy(vm); vm = NULL; }
    return vm;
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
    t->context.sp -= sizeof(struct trap_frame);
    t->user_frame = (void *)t->context.sp;
    return t;
}

static void setup(struct trap_frame *f)
{
    strset((char *)f, 0, sizeof(*f));
    f->pc = 0;
    f->sp = USER_STACK_TOP;
    f->pstate = 0; /* EL0t, IRQ enabled */
}

int process_spawn(const char *name, void (*start)(void))
{
    if (!start) return -1;
    struct vm_space *vm = load_image(name);
    if (!vm) return -1;
    thread_t *t = allocate_process();
    if (!t) { vm_destroy(vm); return -1; }
    t->vm = vm;
    setup(t->user_frame);
    t->entry = start;
    int pid = t->id;
    thread_publish(t);
    return pid;
}

static int process_fork(struct trap_frame *f)
{
    thread_t *child = allocate_process();
    if (!child) return -1;
    child->vm = vm_fork(get_current()->vm);
    if (!child->vm) { buddy_block_free(child->allocation); return -1; }
    struct trap_frame *cf = child->user_frame;
    *cf = *f; /* Identical virtual addresses; no pointer or stack relocation. */
    cf->x[0] = 0;
    int pid = child->id;
    thread_publish(child);
    return pid;
}

/* Firmware returns a bus address. Expose only the allocated framebuffer to
 * EL0, with shared external PTEs that fork never copies or returns to buddy. */
static int map_mailbox_framebuffer(struct vm_space *vm, uint32_t *box, size_t bytes)
{
    size_t words = bytes / 4;
    for (size_t i = 2; i < words && box[i];) {
        if (words - i < 3 || box[i + 1] > (words - i - 3) * 4) return -1;
        size_t count = (box[i + 1] + 3UL) / 4;
        if (box[i] == FB_ALLOC_BUFFER && count >= 2 && (box[i + 2] & 0x80000000)) {
            uintptr_t pa = box[i + 3] & 0x3fffffffU;
            size_t size = box[i + 4];
            uint64_t vc = mbox_get_VC_base_addr();
            uintptr_t vc_base = vc >> 32;
            size_t vc_size = (uint32_t)vc;
            if (!size || pa < vc_base || size > vc_size || pa - vc_base > vc_size - size ||
                vm_map_framebuffer(vm, pa, size) < 0) return -1;
        }
        i += 3 + count;
    }
    return 0;
}

static int process_mailbox(unsigned channel, uintptr_t user)
{
    struct vm_space *vm = get_current()->vm;
    uint32_t size;
    if (channel != MBOX_CH_PROP || (user & 3) || vm_copyin(vm, &size, user, 4) < 0 ||
        size < 12 || (size & 3) || size > (PAGE_SIZE << MAX_ORDER) ||
        vm_user_check(vm, user, size, PROT_READ) < 0 ||
        vm_user_check(vm, user, size, PROT_WRITE) < 0) return 0;
    unsigned order = 0;
    while (((size_t)PAGE_SIZE << order) < size) ++order;
    page_t *page = buddy_block_alloc(order);
    if (!page) return 0;
    uint32_t *box = phys_to_virt(page->phy_addr);
    int result = 0;
    if (vm_copyin(vm, box, user, size) == 0) {
        /* Validate the complete tag list before giving firmware the buffer. */
        size_t i = 2, words = size / 4;
        while (i < words && box[i]) {
            if (words - i < 3 || box[i + 1] > (words - i - 3) * 4)
                goto done;
            i += 3 + (box[i + 1] + 3UL) / 4;
        }
        if (i >= words || box[0] != size) goto done;
        /* The GPU never sees a user virtual address or a shared COW page. */
        result = mailbox_call(channel, box);
        if (result && map_mailbox_framebuffer(vm, box, size) < 0) result = 0;
        if (vm_copyout(vm, user, box, size) < 0) result = 0;
    }
done:
    buddy_block_free(page);
    return result;
}

void syscall_dispatch(struct trap_frame *f)
{
    uint64_t result = (uint64_t)-1;
    thread_t *t = get_current();
    switch (f->x[8]) {
    case 0: result = t->id; break;
    case 1: {
        uintptr_t buf = f->x[0];
        size_t size = f->x[1];
        if (vm_user_check(t->vm, buf, size, PROT_WRITE) < 0) break;
        size_t done = 0;
        while (done < size) {
            char c;
            while (!uart_async_read(&c)) {
                if (t->killed) thread_exit();
                schedule();
            }
            if (vm_copyout(t->vm, buf + done, &c, 1) < 0) break;
            ++done;
        }
        result = done;
        break;
    }
    case 2: {
        uintptr_t buf = f->x[0];
        size_t size = f->x[1];
        if (vm_user_check(t->vm, buf, size, PROT_READ) < 0) break;
        for (;;) {
            uint64_t flags = irq_save();
            if (!writer_busy) { writer_busy = 1; irq_restore(flags); break; }
            irq_restore(flags);
            if (t->killed) thread_exit();
            schedule();
        }
        size_t written = 0;
        char chunk[128];
        while (written < size && !t->killed) {
            size_t n = size - written;
            if (n > sizeof(chunk)) n = sizeof(chunk);
            if (vm_copyin(t->vm, chunk, buf + written, n) < 0) break;
            for (size_t i = 0; i < n; ++i) uart_write_char(chunk[i]);
            written += n;
        }
        uint64_t flags = irq_save();
        writer_busy = 0;
        irq_restore(flags);
        result = written;
        break;
    }
    case 3: {
        char name[256];
        if (vm_copystr(t->vm, name, f->x[0], sizeof(name)) < 0) break;
        struct vm_space *vm = load_image(name);
        if (!vm) break;
        /* Keep the PGD and TCB coherent against timer context switches. */
        preempt_disable();
        struct vm_space *old = t->vm;
        t->vm = vm;
        vm_switch(vm);
        setup(f);
        vm_destroy(old);
        preempt_enable();
        return;
    }
    case 4: result = process_fork(f); break;
    case 5: thread_exit();
    case 6: result = process_mailbox(f->x[0], f->x[1]); break;
    case 7: result = thread_kill(f->x[0]); break;
    case 10: result = vm_mmap(t->vm, f->x[0], f->x[1], f->x[2], f->x[3]); break;
    }
    f->x[0] = result;
}
