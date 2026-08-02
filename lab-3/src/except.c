#include "uart.h"
#include "irq.h"
#include "shell.h"
#include "timer.h"
#include "tasklist.h"

// 針對 GIC-400 的通用定義 (Raspberry Pi 4B 專用)
#define GIC_CNTPS_IRQ_ID       29    // Secure Physical Timer 在 GIC 中的中斷號
#define GIC_CNTNS_IRQ_ID       30    // Non-secure Physical Timer 
#define AUXINIT_BIT_POSTION 1<<29

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

void timer_irq_handler(void)
{
    uint64_t current_time;

    /* Pause halt the timer，避免持續觸發 */
    asm volatile(
        "msr cntp_ctl_el0, %0"
        :
        : "r"(0UL)
    );

    asm volatile(
        "mrs %0, cntpct_el0"
        : "=r"(current_time)
    );

    while (timer_head && timer_head->expiry <= current_time) {
        timer_t *expired = timer_head;

        timer_head = expired->next;
        if (timer_head)
            timer_head->prev = NULL;

        expired->callback(expired->data);

        /* This can express expired */
    }

    if (timer_head) {
        asm volatile(
            "msr cntp_cval_el0, %0"
            :
            : "r"(timer_head->expiry)
        );

        asm volatile(
            "msr cntp_ctl_el0, %0"
            :
            : "r"(1UL)
        );
    }
}

static void uart0_irq_handler(void)
{
    uint32_t status = mmio_read(UART0_MIS);

    /*
     * RX interrupt or receive-timeout interrupt。
     * Keep reading, until  RX FIFO empty。
     */
    if (status & (UART_RXIM | UART_RTIM)) {
        while (!(mmio_read(UART0_FR) & UART_RXFE)) {
            unsigned char ch =
                (unsigned char)(mmio_read(UART0_DR) & 0xff);

            uart_write_char(ch);
        }

        /*
         * RX interrupt 通常會因 FIFO 被讀空而解除；
         * receive-timeout interrupt 必須透過 ICR 清除。
         */
        mmio_write(UART0_ICR, UART_RXIM | UART_RTIM);
    }
}

void irq_except_handler_c(void)
{
    uint32_t iar = *GICC_IAR;
    uint32_t intid = iar & 0x3ffU;

    switch (intid) {
    case 153:
        uart0_irq_handler();
        break;

    case GIC_CNTNS_IRQ_ID:
        timer_irq_handler();
        break;

    default:
        break;
    }

    if (intid < 1020U) {
        *GICC_EOIR = iar;
        asm volatile("dsb sy" ::: "memory");
    }
}
void gic_init(void)
{
    *GICD_CTLR = 1;
    *GICC_PMR  = 0xff;
    *GICC_CTLR = 1;

    /* UART0 priority = 0x80 */
    ((volatile uint8_t *)GICD_IPRIORITYR)[153] = 0x80;

    /* 將 UART0 interrupt routing 到 CPU0 */
    ((volatile uint8_t *)GICD_ITARGETSR)[153] = 0x01;

	/* Non-secure physical timer PPI INTID 30 */
    ((volatile uint8_t *)GICD_IPRIORITYR)[GIC_CNTNS_IRQ_ID] =
        0x40;

	/*
     * INTID 30 must be PPI，each CPU has own enable bit。
     * CPU0 must activate it by itself。 
     */
    *GICD_ISENABLER(0) =
        1U << GIC_CNTNS_IRQ_ID;
	
	*GICD_CTLR = 1U;
    *GICC_PMR  = 0xffU;
    *GICC_CTLR = 1U;

    asm volatile(
        "dsb sy\n"
        "isb\n"
        ::: "memory"
    );
}
