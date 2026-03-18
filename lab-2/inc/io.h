#ifndef IO_H
#define IO_H
void uart_init();
void uart_writeText(char *buffer);
unsigned char uart_readByte();
unsigned int uart_isReadByteReady();
void uart_writeByteBlocking(unsigned char ch);
void uart_write_char(unsigned char ch);
void uart_puts(char *buffer);
void mmio_write(long reg, unsigned int val);
unsigned int mmio_read(long reg);
void uart_CR();
# endif