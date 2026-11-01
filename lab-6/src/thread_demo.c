#include "thread.h"
#include "irq.h"
#include "print.h"

static volatile unsigned int started;
static volatile unsigned int remaining;
static unsigned int target;
static volatile unsigned int guard_progress;

/* No function calls/yields while these caller-saved values are live. */
static int register_probe(uint64_t deadline, uint64_t token)
{
    uint64_t result;
    asm volatile(
        "mov x10, %2\n"
        "fmov d0, %2\n"
        "fmov d1, %2\n"
        "fmov d31, %2\n"
        "1: mrs x11, cntpct_el0\n"
        "cmp x11, %1\n"
        "b.lo 1b\n"
        "eor %0, x10, %2\n"
        "fmov x11, d0\n"
        "eor x11, x11, %2\n"
        "orr %0, %0, x11\n"
        "fmov x11, d1\n"
        "eor x11, x11, %2\n"
        "orr %0, %0, x11\n"
        "fmov x11, d31\n"
        "eor x11, x11, %2\n"
        "orr %0, %0, x11\n"
        : "=&r"(result) : "r"(deadline), "r"(token)
        : "x10", "x11", "v0", "v1", "v31", "cc", "memory");
    return result == 0;
}

static void preempt_worker(void)
{
    uint64_t flags = irq_save();
    ++started;
    irq_restore(flags);
    /* Without timer preemption, the first worker can never leave this barrier. */
    while (started < target)
        asm volatile("nop" ::: "memory");

    uint64_t frequency;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    unsigned int id = current_thread()->id;
    for (int i = 0; i < 10; ++i) {
        uint64_t now;
        asm volatile("mrs %0, cntpct_el0" : "=r"(now));
        uint64_t interval = frequency * thread_get_quantum_ms() * 2 / 1000;
        if (i == 0) {
            preempt_disable();
            unsigned int before = guard_progress;
            if (!register_probe(now + interval, id) || guard_progress != before)
                printf("[preempt] critical-section corruption\n");
            ++guard_progress;
            preempt_enable();
            asm volatile("mrs %0, cntpct_el0" : "=r"(now));
        }
        if (!register_probe(now + interval, ((uint64_t)id << 32) | (unsigned int)i))
            printf("[preempt] register corruption in thread %d\n", (int)id);
        printf("Preempt id: %d %d\n", (int)id, i);
    }
    flags = irq_save();
    --remaining;
    irq_restore(flags);
    if (id % 3 == 0)
        thread_exit();
}

void thread_preempt_test(void)
{
    preempt_disable();
    if (remaining) {
        printf("[preempt] Test already running\n");
        preempt_enable();
        return;
    }
    started = 0;
    target = 0;
    for (int i = 0; i < 3; ++i) {
        if (!thread_create(preempt_worker))
            break;
        ++target;
    }
    remaining = target;
    printf("[preempt] %d workers, quantum %d ms, no schedule() calls\n",
           (int)target, (int)thread_get_quantum_ms());
    preempt_enable();
}
