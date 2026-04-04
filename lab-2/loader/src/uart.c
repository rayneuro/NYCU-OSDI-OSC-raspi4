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
    volatile unsigned int  __attribute__((aligned(16))) mbox[36];
    
    mbox[0] = 9*4;
    mbox[1] = TAGS_REQ_CODE;

    // tags begin
    mbox[2] = MBOX_TAGS_SET_CLOCK;   // set clock rate
    mbox[3] = 12;               // maximum of request and response value buffer's length.
    mbox[4] = TAGS_REQ_CODE;       
    mbox[5] = CLOCK_ID_UART;    // clock id: UART clock
    mbox[6] = 4000000;          // rate: 4Mhz
    mbox[7] = 0;                // clear turbo
    mbox[8] = TAGS_REQ_END;
    
    mbox_call( MBOX_CH_PROP, &mbox);

    /* map UART0 to GPIO pins */
    //r =*GPFSEL1;
    //r &= ~((7<<12)|(7<<15)); // gpio14, gpio15
    //r |= (4<<12)|(4<<15);    // alt0
    gpio_useAsAlt0(14);
    gpio_useAsAlt0(15);


    mmio_write(UART0_ICR , 0x7FF);    // clear interrupts
    // baud rate = UART_CLK / (16 × (IBRD + FBRD / 64))
    mmio_write(UART0_IBRD , 2);       // 115200 baud
    mmio_write(UART0_FBRD , 0xB);
    mmio_write(UART0_LCRH, (3 << 5) | (1 << 4)); // Set up WLEN（Word Length）3 -> 8 bits and FEN（FIFO Enable）= 1 → open TX/RX FIFO
    mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9) | (1 << 4));    // enable Tx, Rx, FIFO
}

unsigned int uart_isReadByteNotReady()  { return mmio_read(UART0_FR) & (1 << 4); }
unsigned int uart_isWriteByteNotReady() {  return mmio_read(UART0_FR) & (1 << 5); }

unsigned char uart_readByte() {
    while (uart_isReadByteNotReady());
    return (unsigned char)mmio_read(UART0_DR);
}

void uart_writeByteBlockingActual(unsigned char ch) {
    while (uart_isWriteByteNotReady()); 
    mmio_write(UART0_DR, (unsigned int)ch);
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