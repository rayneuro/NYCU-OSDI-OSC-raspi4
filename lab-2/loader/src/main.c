#include "uart.h"

void main()
{
    int size=0;
    char *kernel;
    mmio_write(kernel,(char *)0x80000);

    // set up serial console
    uart_init();

    // say hello. To reduce loader size I removed uart_puts()
again:
    uart_send('R');
    uart_send('B');
    uart_send('I');
    uart_send('N');
    uart_send('6');
    uart_send('4');
    uart_send('\r');
    uart_send('\n');
    // notify raspbootcom to send the kernel
    uart_send(3);
    uart_send(3);
    uart_send(3);

    // read the kernel's size
    size=uart_readByte();
    size|=uart_readByte()<<8;
    size|=uart_readByte()<<16;
    size|=uart_readByte()<<24;

    // send negative or positive acknowledge
    if(size<64 || size>1024*1024) {
        // size error
        uart_send('S');
        uart_send('E');
        goto again;
    }
    uart_send('O');
    uart_send('K');

    // read the kernel
    while(size--) *kernel++ = uart_readByte();

    // restore arguments and jump to the new kernel.
    asm volatile (
        "mov x0, x10;"
        "mov x1, x11;"
        "mov x2, x12;"
        "mov x3, x13;"
        // we must force an absolute address to branch to
        "mov x30, 0x80000; ret"
    );
}