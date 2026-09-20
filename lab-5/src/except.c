#include "uart.h"
#include "irq.h"
#include "timer.h"
#include "tasklist.h"

void except_handler_c() {
	uart_puts("In Exception handle\n");

	//read spsr_el1
	unsigned long long spsr_el1 = 0;
	asm volatile("mrs %0, spsr_el1":"=r"(spsr_el1));
	uart_puts("spsr_el1: ");
	uart_hex(spsr_el1);
	uart_puts("\n");

	//read elr_el1
	unsigned long long elr_el1 = 0;
	asm volatile("mrs %0, elr_el1":"=r"(elr_el1));
	uart_puts("elr_el1: ");
	uart_hex(elr_el1);
	uart_puts("\n");
	
	//esr_el1
	unsigned long long esr_el1 = 0;
	asm volatile("mrs %0, esr_el1":"=r"(esr_el1));
	uart_hex(esr_el1);
	uart_puts("\n");

	//ec
	unsigned ec = (esr_el1 >> 26) & 0x3F; //0x3F = 0b111111(6)
	uart_puts("ec: ");
	uart_hex(ec);
	uart_puts("\n");

	while(1){

	}
}

void irq_except_handler_c(void)
{
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
