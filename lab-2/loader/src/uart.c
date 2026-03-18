#include "gpio.h"
#include "uart.h"
#include "mailbox.h"

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

unsigned char uart_output_queue[UART_MAX_QUEUE];
unsigned int uart_output_queue_write = 0;
unsigned int uart_output_queue_read = 0;

void uart_init() {
    register unsigned int r;

    /* initialize UART */
    mmio_write(UART0_CR , 0);         // turn off UART0

    /* set up clock for consistent divisor values */
    mbox[0] = 9*4;
    mbox[1] = MBOX_REQUEST;
    mbox[2] = MBOX_TAG_SETCLKRATE; // set clock rate
    mbox[3] = 12;
    mbox[4] = 8;
    mbox[5] = 2;           // UART clock
    mbox[6] = 4000000;     // 4Mhz
    mbox[7] = 0;           // clear turbo
    mbox[8] = MBOX_TAG_LAST;
    mbox_call(MBOX_CH_PROP);

    /* map UART0 to GPIO pins */
    r =*GPFSEL1;
    r &= ~((7<<12)|(7<<15)); // gpio14, gpio15
    r |= (4<<12)|(4<<15);    // alt0
    *GPFSEL1 = r;
    *GPPUD = 0;            // enable pins 14 and 15
    r=150; while(r--) { asm volatile("nop"); }
    mmio_write(GPPUDCLK0 , (1<<14)|(1<<15));
    r=150; while(r--) { asm volatile("nop"); }
    mmio_write(GPPUDCLK0 , 0);        // flush GPIO setup

    mmio_write(UART0_ICR , 0x7FF);    // clear interrupts
    mmio_write(UART0_IBRD , 2);       // 115200 baud
    mmio_write(UART0_FBRD , 0xB);
    mmio_write(UART0_LCRH , 0b11<<5); // 8n1
    mmio_write(UART0_CR , 0x301);     // enable Tx, Rx, FIFO
}

unsigned int uart_isReadByteReady()  { return mmio_read(AUX_MU_LSR_REG) & 0x01; }
unsigned int uart_isWriteByteReady() { return mmio_read(AUX_MU_LSR_REG) & 0x20; }

unsigned char uart_readByte() {
    while (!uart_isReadByteReady());
    return (unsigned char)mmio_read(AUX_MU_IO_REG);
}

void uart_writeByteBlockingActual(unsigned char ch) {
    while (!uart_isWriteByteReady()); 
    mmio_write(AUX_MU_IO_REG, (unsigned int)ch);
}

void uart_write_char(unsigned char ch){
    uart_writeByteBlockingActual(ch);
}

void uart_puts(char * buffer){
    while (*buffer) {
       
       if (*buffer == '\n') uart_write_char('\r');
       uart_write_char(*buffer++);
       
    }
}

void uart_CR(){
    uart_write_char('\r');
}