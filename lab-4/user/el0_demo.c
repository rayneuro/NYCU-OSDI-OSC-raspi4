/* No absolute addresses: this raw binary runs directly inside initramfs. */
__attribute__((section(".text.entry"), noreturn))
void _start(void)
{
    volatile unsigned long stack_value = 42;
    __asm__ volatile("mov x0, %0\nsvc #0" :: "r"(stack_value) : "x0", "memory");
    for (;;)
        __asm__ volatile("nop");
}