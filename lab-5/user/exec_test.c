#include "syscall.h"
__attribute__((section(".text.entry"), noreturn)) void _start(void)
{
    static const char text[] = "PASS exec replacement\n";
    uart_write(text, sizeof(text)-1);
    exit(0);
}
