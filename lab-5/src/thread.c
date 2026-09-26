#include "thread.h"
#include "process.h"
#include "irq.h"
#include "print.h"
#include "time.h"
#include "string.h"
#include "timer.h"
#include <stddef.h>

/* One 16 KiB buddy block: TCB at the bottom, downward-growing stack above it. */
#define THREAD_ORDER 2
#define THREAD_BYTES (PAGE_SIZE << THREAD_ORDER)
_Static_assert(offsetof(thread_t, context) == 0, "TPIDR must point to TCB");
_Static_assert(offsetof(struct thread_context, fp) == 80, "switch FP offset");
_Static_assert(offsetof(struct thread_context, lr) == 88, "switch LR offset");
_Static_assert(offsetof(struct thread_context, sp) == 96, "switch SP offset");
_Static_assert(offsetof(struct thread_context, d8_d15) == 104, "switch FP regs offset");
_Static_assert(offsetof(struct thread_context, fpcr) == 168, "switch FPCR offset");
_Static_assert(sizeof(thread_t) < THREAD_BYTES / 2, "room for stack");

static thread_t idle_thread;
static LIST_HEAD(run_queue);
static LIST_HEAD(zombies);
static unsigned int next_id = 1;
static int initialized;
static unsigned int irq_depth;
static unsigned int preempt_count;
static int need_resched;
static unsigned int quantum_ms = THREAD_DEFAULT_QUANTUM_MS;

void thread_request_reschedule(void) { need_resched = 1; }
void thread_irq_enter(void) { ++irq_depth; }

void thread_irq_exit(uint64_t interrupted_spsr)
{
    --irq_depth;
    if (!irq_depth && (interrupted_spsr & 0x1f) == 0 && get_current()->killed)
        thread_exit();
    /* Schedule only after the outermost IRQ has completed its GIC EOI. */
    if (!irq_depth && !preempt_count && need_resched &&
        ((interrupted_spsr & 0x1f) == 5 || (interrupted_spsr & 0x1f) == 0) && !(interrupted_spsr & 0x80))
        schedule();
}

void preempt_disable(void)
{
    uint64_t flags = irq_save();
    ++preempt_count;
    irq_restore(flags);
}

void preempt_enable(void)
{
    uint64_t flags = irq_save();
    if (preempt_count)
        --preempt_count;
    if (!preempt_count && !irq_depth && need_resched && !(flags & 0x80))
        schedule();
    irq_restore(flags);
}

int thread_set_quantum_ms(unsigned int ms)
{
    if (!ms || ms > 1000)
        return 0;
    uint64_t flags = irq_save();
    quantum_ms = ms;
    if (initialized) {
        need_resched = 0;
        timer_set_quantum_ms(ms);
    }
    irq_restore(flags);
    return 1;
}

unsigned int thread_get_quantum_ms(void) { return quantum_ms; }

extern void switch_to(thread_t *prev, thread_t *next);

static thread_t *queue_thread(struct list_head *node)
{
    return (thread_t *)((char *)node - offsetof(thread_t, queue));
}

thread_t *current_thread(void) { return get_current(); }

static void thread_bootstrap(void)
{
    thread_t *self = get_current();
    /* switch_to runs with IRQs masked; first entry has no schedule frame. */
    irq_restore(self->initial_daif);
    self->entry();
    thread_exit();
}

void thread_init(void)
{
    uint64_t flags = irq_save();
    if (!initialized) {
        INIT_LIST_HEAD(&run_queue);
        INIT_LIST_HEAD(&zombies);
        INIT_LIST_HEAD(&idle_thread.queue);
        idle_thread.state = THREAD_RUNNING;
        asm volatile("msr tpidr_el1, %0" :: "r"(&idle_thread) : "memory");
        initialized = 1;
        timer_set_quantum_ms(quantum_ms);
    }
    irq_restore(flags);
}

thread_t *thread_alloc(void (*entry)(void))
{
    uint64_t flags = irq_save();
    if (!initialized || !entry) {
        irq_restore(flags);
        return NULL;
    }
    page_t *block = buddy_block_alloc(THREAD_ORDER);
    if (!block) {
        irq_restore(flags);
        return NULL;
    }
    thread_t *t = (thread_t *)(uintptr_t)block->phy_addr;
    strset((char *)t, 0, sizeof(*t));
    t->allocation = block;
    t->id = next_id++;
    t->entry = entry;
    t->initial_daif = flags;
    t->state = THREAD_RUNNABLE;
    t->context.sp = ((uintptr_t)t + THREAD_BYTES) & ~(uintptr_t)15;
    t->context.lr = (uintptr_t)thread_bootstrap;
    irq_restore(flags);
    return t;
}

void thread_publish(thread_t *t)
{
    uint64_t flags = irq_save();
    list_add_tail(&t->queue, &run_queue);
    irq_restore(flags);
}

thread_t *thread_create(void (*entry)(void))
{
    thread_t *t = thread_alloc(entry);
    if (t) thread_publish(t);
    return t;
}

int thread_alive(unsigned int pid)
{
    uint64_t flags = irq_save();
    int found = get_current()->id == pid;
    for (struct list_head *n = run_queue.next; n != &run_queue; n = n->next)
        if (queue_thread(n)->id == pid) found = 1;
    irq_restore(flags);
    return found;
}

int thread_kill(unsigned int pid)
{
    uint64_t flags = irq_save();
    thread_t *t = get_current();
    if (t->id == pid && t->user_stack) {
        irq_restore(flags);
        thread_exit();
    }
    for (struct list_head *n = run_queue.next; n != &run_queue; n = n->next) {
        t = queue_thread(n);
        if (t->id == pid && t->user_stack) {
            /* Defer until user return so an in-flight syscall releases locks. */
            t->killed = 1;
            irq_restore(flags);
            return 0;
        }
    }
    irq_restore(flags);
    return -1;
}

void schedule(void)
{
    uint64_t flags = irq_save();
    if (!initialized || irq_depth || preempt_count) {
        irq_restore(flags);
        return;
    }
    thread_t *prev = get_current();
    /* The running thread is not queued. Idle participates so it can reap
     * zombies even when another thread (e.g. the shell) is always runnable. */
    if (prev->state == THREAD_RUNNING) {
        prev->state = THREAD_RUNNABLE;
        list_add_tail(&prev->queue, &run_queue);
    }
    thread_t *next = &idle_thread;
    if (!list_empty(&run_queue)) {
        next = queue_thread(run_queue.next);
        list_del(&next->queue);
    }
    next->state = THREAD_RUNNING;
    need_resched = 0;
    timer_reset_slice();
    if (next != prev)
        switch_to(prev, next);
    /* Each resumed thread restores the DAIF saved on its own stack. */
    irq_restore(flags);
}

void thread_exit(void)
{
    uint64_t flags = irq_save();
    thread_t *self = get_current();
    if (self == &idle_thread) {
        irq_restore(flags);
        idle();
    }
    self->state = THREAD_DEAD;
    list_add_tail(&self->queue, &zombies);
    schedule();
    for (;;)
        asm volatile("wfe");
}

void kill_zombies(void)
{
    uint64_t flags = irq_save();
    if (get_current() == &idle_thread) {
        while (!list_empty(&zombies)) {
            thread_t *t = queue_thread(zombies.next);
            list_del(&t->queue);
            /* The dead thread's stack is no longer active. */
            process_image_put(t->user_image);
            if (t->user_stack) buddy_block_free(t->user_stack);
            buddy_block_free(t->allocation);
        }
    }
    irq_restore(flags);
}

void idle(void)
{
    for (;;) {
        kill_zombies();
        schedule();
    }
}

static void thread_demo_worker(void)
{
    for (int i = 0; i < 10; ++i) {
        printf("Thread id: %d %d\n", (int)current_thread()->id, i);
        wait_cycles(1000000);
        schedule();
    }
    /* Exercise both explicit exit and return-through-trampoline cleanup. */
    if (current_thread()->id % 3 == 0)
        thread_exit();
}

void thread_test(void)
{
    for (int i = 0; i < 3; ++i) {
        if (!thread_create(thread_demo_worker)) {
            printf("[thread] Unable to allocate demo thread\n");
            return;
        }
    }
}
