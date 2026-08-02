#include "irq.h"
uint64_t irq_save(void)
{
    uint64_t flags;

    asm volatile(
        "mrs %0, daif\n"
        "msr daifset, #2"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

void irq_restore(uint64_t flags)
{
    asm volatile(
        "msr daif, %0"
        :
        : "r"(flags)
        : "memory"
    );
}