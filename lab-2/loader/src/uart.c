#include "gpio.h"
#include "uart.h"

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

unsigned char uart_output_queue[UART_MAX_QUEUE];
unsigned int uart_output_queue_write = 0;
unsigned int uart_output_queue_read = 0;

void uart_init() {
    mmio_write(AUX_ENABLES, 1); //enable UART1
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3); //8 bits
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xC6); //disable interrupts
    mmio_write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    gpio_useAsAlt5(14);
    gpio_useAsAlt5(15);
    mmio_write(AUX_MU_CNTL_REG, 3); //enable RX/TX
}

unsigned int uart_isOutputQueueEmpty() {
    return uart_output_queue_read == uart_output_queue_write;
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

void uart_loadOutputFifo() {
    while (!uart_isOutputQueueEmpty() && uart_isWriteByteReady()) {
        uart_writeByteBlockingActual(uart_output_queue[uart_output_queue_read]);
        uart_output_queue_read = (uart_output_queue_read + 1) & (UART_MAX_QUEUE - 1); // Don't overrun
    }
}

void uart_writeByteBlocking(unsigned char ch) {
    unsigned int next = (uart_output_queue_write + 1) & (UART_MAX_QUEUE - 1); // Don't overrun

    while (next == uart_output_queue_read) uart_loadOutputFifo();

    uart_output_queue[uart_output_queue_write] = ch;
    uart_output_queue_write = next;
}

void uart_writeText(char *buffer) {
    while (*buffer) {
       if (*buffer == '\n') uart_writeByteBlocking('\r');
       uart_writeByteBlocking(*buffer++);
    }
}

void uart_drainOutputQueue() {
    while (!uart_isOutputQueueEmpty()) uart_loadOutputFifo();
}

void uart_update() {
    uart_loadOutputFifo();
    
    if (uart_isReadByteReady()) {
       unsigned char ch = uart_readByte();
       if (ch == '\r') uart_writeText("\n"); else uart_writeByteBlocking(ch);
    }
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