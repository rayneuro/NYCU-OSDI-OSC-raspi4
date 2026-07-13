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

void timer_irq_handler() {
	//enable core_0_timer
	unsigned int* address = (unsigned int*) CORE0_TIMER_IRQ_CTRL;
	*address = 2;

	asm volatile("msr cntp_ctl_el0,%0"::"r"(0));
	// Disable interrupts to protect critical section
	asm volatile("msr DAIFSet, 0xf");

	uint64_t current_time;
	asm volatile("mrs %0, cntpct_el0":"=r"(current_time));

	while(timer_head && timer_head->expiry <= current_time) {
		timer_t *timer = timer_head;

		//Execute the callback
		timer->callback(timer->data);
 
		// Remove timer from the list
        timer_head = timer->next;
        if (timer_head) {
            timer_head->prev = NULL;
        }
		
		//free timer
		
		// Reprogram the hardware timer if there are still timers left
		if(timer_head) {
			asm volatile("msr cntp_cval_el0, %0"::"r"(timer_head->expiry));
			asm volatile("msr cntp_ctl_el0,%0"::"r"(1));
		} else {
			asm volatile("msr cntp_ctl_el0,%0"::"r"(0));
		}
	

		//enable interrupt
		asm volatile("msr DAIFClr,0xf");
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

void irq_except_handler_c() {


	asm volatile("msr DAIFSet, 0xf"); // Disable interrupts
									  
	uint32_t irq_pending1 = mmio_read(IRQ_PENDING_1);
	uint32_t core0_interrupt_source = mmio_read(CORE0_INTERRUPT_SOURCE);	
	uint32_t iir = mmio_read(AUX_MU_IIR);

	if (core0_interrupt_source & CNTPSIRQ_BIT_POSITION) {
		
		//djsable core 0 timer
		unsigned int* address = (unsigned int*) CORE0_TIMER_IRQ_CTRL;
		*address = 0;

		create_task(timer_irq_handler,3);
    }

    // Handle UART interrupt
    if (irq_pending1 & AUXINIT_BIT_POSTION) {
         if ((iir & 0x06) == 0x04) {
			 //Disable receive interrupt
			 mmio_write(AUX_MU_IER, mmio_read(AUX_MU_IER) & ~(0x01));
			 create_task(uart_receive_handler,1);
		 }

		if ((iir & 0x06) == 0x02) {
			//Disable transmit interrupt
			//mmio_write(AUX_MU_IER, mmio_read(AUX_MU_IER) & ~(0x02));
			//create_task(uart_transmit_handler,2);
		}
    }	
	
	asm volatile("msr DAIFClr, 0xf"); // Enable interrupts
	
	execute_tasks();
	//asm volatile("msr DAIFClr, 0xf"); // Enable interrupts
	
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

    asm volatile("dsb sy");
    asm volatile("isb");
}
