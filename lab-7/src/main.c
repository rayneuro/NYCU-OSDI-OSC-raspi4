#include "uart.h"
#include "shell.h"
#include "framebuffer.h"
#include "dtb.h"
#include "irq.h"
#include "print.h"
#include "mm.h"
#include "thread.h"
#include "vfs.h"

extern void *_dtb_ptr;
static void uart_printf_putc(void *arg, char ch)
{
	(void)arg;

	if (ch == '\n')
		uart_async_write_char('\r');

	uart_async_write_char((unsigned char)ch);
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
	mm_init();
	uart_puts("[vm] MMU enabled: kernel at ");
	uart_hex((uintptr_t)main);
	uart_puts("; empty TTBR0, 4 KiB user pages\n");
	uart_async_send("Hello World!\n");
	gic_init();
	uart_enable_interrupt();

	asm volatile(
		"dsb sy\n"
		"msr DAIFClr, #2\n"
		"isb\n"
		::: "memory"
	);

	// Keep the bootstrap stack for idle and run the shell as a thread.
	thread_init();
	if (vfs_init() < 0) { uart_puts("[vfs] initialization failed\n"); for (;;) asm volatile("wfe"); }
	uart_puts("[vfs] tmpfs /, initramfs /initramfs, devfs /dev ready\n");
	if (!thread_create(shell_init))
		printf("[thread] Unable to create shell thread\n");
	idle();
}
