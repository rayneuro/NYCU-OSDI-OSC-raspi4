#include "uart.h"
#include "shell.h"
#include "framebuffer.h"
#include "dtb.h"

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

    // start shell
    shell_init();
    
    return 0;
}