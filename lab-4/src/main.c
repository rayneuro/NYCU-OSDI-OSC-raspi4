#include "uart.h"
#include "shell.h"
#include "framebuffer.h"
#include "dtb.h"
#include "irq.h"
#include "printf.h"

extern void *_dtb_ptr;
static void uart_printf_putc(void *arg, char ch)
{
    (void)arg;

    if (ch == '\n')
        uart_write_char('\r');

    uart_write_char((unsigned char)ch);
}

int main()
{
    // set up serial console
    uart_init();
    init_printf(NULL, uart_printf_putc);
    // set up framebuffer
    framebuffer_init();
    framebuffer_show_pic();
    // say hello
    fdt_traverse(get_cpio_addr,_dtb_ptr);
    uart_puts("Hlelo World!\n");
    gic_init();
    uart_enable_interrupt();

    asm volatile(
        "dsb sy\n"
        "msr DAIFClr, #2\n"
        "isb\n"
        ::: "memory"
    );

    // start shell
    shell_init();
    
    return 0;
}
