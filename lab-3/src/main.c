#include "uart.h"
#include "shell.h"
#include "framebuffer.h"
#include "dtb.h"
#include "irq.h"

extern void *_dtb_ptr;
int main()
{
    // set up serial console
    uart_init();

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
