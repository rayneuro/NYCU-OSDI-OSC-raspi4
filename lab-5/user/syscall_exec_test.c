#include "syscall.h"

__attribute__((section(".text.entry"), noreturn))
void _start(void)
{
    static const char text[] = "[PASS] 3 exec: replacement program running\r\n"
                               "[EXIT] 5 exit: replacement process\r\n";
    uart_write(text, sizeof(text) - 1);
    exit(0);
}
