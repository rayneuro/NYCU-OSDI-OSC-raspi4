#include "uart.h"
#include "irq.h"
#include "timer.h"
#include "tasklist.h"
#include "thread.h"

#include "process.h"

void except_handler_c(struct trap_frame *frame)
{
    uint64_t esr;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    if ((frame->pstate & 31) == 0) {
        /* Return state is already saved; nested timer IRQs are now safe. */
        asm volatile("msr daifclr, #2" ::: "memory");
        if ((esr >> 26) == 0x15 && (esr & 0xffff) == 0)
            syscall_dispatch(frame);
        else {
            uart_puts("[process] user fault ESR=");
            uart_hex(esr);
            uart_puts(" PC=");
            uart_hex(frame->pc);
            uart_puts("\n");
            thread_exit();
        }
        if (get_current()->killed) thread_exit();
        asm volatile("msr daifset, #2" ::: "memory");
        return;
    }
    uart_puts("[kernel] synchronous exception ESR=");
    uart_hex(esr);
    uart_puts("\n");
    for (;;) asm volatile("wfe");
}

void irq_except_handler_c(uint64_t *frame)
{
    thread_irq_enter();
    uint32_t iar = *GICC_IAR;
    uint32_t intid = iar & 0x3ffU;

    switch (intid) {
    case UART0_GIC_IRQ_ID:
        uart_irq_handler();
        break;

    case GIC_CNTNS_IRQ_ID:
        timer_irq_handler();
        break;

    default:
        break;
    }

    /*
     * Keep this interrupt active in the GIC while running its bottom halves.
     * After IRQ is unmasked, the GIC running-priority mechanism only permits
     * a higher-priority interrupt to nest this handler.
    */
    asm volatile(
        "dsb sy\n"
        "msr DAIFClr, #2\n"
        "isb\n"
        ::: "memory"
    );

    execute_tasks();

    /* Protect the final EOIR and exception-frame restore from another IRQ. */
    asm volatile(
        "msr DAIFSet, #2\n"
        "isb\n"
        ::: "memory"
    );

    if (intid < 1020U) {
        *GICC_EOIR = iar;
        asm volatile("dsb sy" ::: "memory");
    }
    thread_irq_exit(frame[33]); /* SPSR_EL1 saved by save_all. */
}

void gic_init(void)
{
    *GICD_CTLR = 0;
    *GICC_CTLR = 0;

    ((volatile uint8_t *)GICD_IPRIORITYR)
        [UART0_GIC_IRQ_ID] = 0x40;

    ((volatile uint8_t *)GICD_ITARGETSR)
        [UART0_GIC_IRQ_ID] = 0x01;

    ((volatile uint8_t *)GICD_IPRIORITYR)
        [GIC_CNTNS_IRQ_ID] = 0x80;

    *GICD_ISENABLER(0) = 1U << GIC_CNTNS_IRQ_ID;

    *GICC_PMR  = 0xff;
    *GICC_CTLR = 1;
    *GICD_CTLR = 1;

    asm volatile(
        "dsb sy\n"
        "isb\n"
        ::: "memory"
    );
}
