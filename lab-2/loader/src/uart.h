#ifndef UART_H
#define UART_H

#define PERIPHERAL_BASE = 0xFE000000
// mini UART
// bcm2711-peripherals.pdf
enum {
    AUX_BASE        = PERIPHERAL_BASE + 0x215000,
    AUX_IRQ         = AUX_BASE,
    AUX_ENABLES     = AUX_BASE + 4,
    AUX_MU_IO_REG   = AUX_BASE + 64,
    AUX_MU_IER_REG  = AUX_BASE + 68,
    AUX_MU_IIR_REG  = AUX_BASE + 72,
    AUX_MU_LCR_REG  = AUX_BASE + 76,
    AUX_MU_MCR_REG  = AUX_BASE + 80,
    AUX_MU_LSR_REG  = AUX_BASE + 84,
    AUX_MU_MSR_REG  = AUX_BASE + 88,
    AUX_MU_SCRATCH  = AUX_BASE + 92,
    AUX_MU_CNTL_REG = AUX_BASE + 96,
    AUX_MU_STAT_REG = AUX_BASE + 100,
    AUX_MU_BAUD_REG = AUX_BASE + 104,
    AUX_UART_CLOCK  = 500000000,
    UART_MAX_QUEUE  = 16 * 1024
};

// PL011 UART 0
// bcm2711-peripherals.pdf
enum {
    UART_DR =  PERIPHERAL_BASE + 0x201000,
    UART_RSRECR =  PERIPHERAL_BASE + 0x201004,
    UART_FR =  PERIPHERAL_BASE + 0x201018, // Flag register
    UART_ILPR = PERIPHERAL_BASE + 0x201020, // Not in use
    UART_IBRD = PERIPHERAL_BASE +  0x201024, //Integer Baud rate divisor
    UART_FBRD = PERIPHERAL_BASE + 0x201028 // Fractional Baud rate divisor
    UART_LCRH = PERIPHERAL_BASE +  0x20102C  // Line Control
    UART_CR = PERIPHERAL_BASE + 0x201030 // Control register
    UART_IMSC = PERIPHERAL_BASE +  0x201038 // Interrupt Mask Set Clear Register
    UART_ICR = PERIPHERAL_BASE + 0x201044 // Interrupt Clear Register   
};

void uart_init();
unsigned char uart_readByte();
unsigned int uart_isReadByteReady();
void uart_writeByteBlocking(unsigned char ch);
void uart_write_char(unsigned char ch);
void uart_puts(char *buffer);
void mmio_write(long reg, unsigned int val);
unsigned int mmio_read(long reg);
void uart_CR();

# endif