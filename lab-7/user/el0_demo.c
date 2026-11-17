#include "syscall.h"
/* Flat binary linked at VA zero; all I/O goes through the syscall ABI. */
__attribute__((section(".text.entry"), noreturn)) void _start(void)
{
    static const char message[] = "Hello from EL0 with virtual memory!\n";
    uart_write(message, sizeof(message) - 1);
    exit(0);
}
