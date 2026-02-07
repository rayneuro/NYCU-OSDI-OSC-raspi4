#include "shell.h"
#include "command.h"
#include "io.h"


int main()
{
    uart_init();
    
    command_hello();
    //uart_writeText("Hello world!\n");
    //uart_writeText("Test the Mini uart output\n");
    shell_init();
    //while (1) {
    //    uart_update();
    //}
    
}