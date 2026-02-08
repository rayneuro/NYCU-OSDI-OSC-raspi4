#include "shell.h"
#include "command.h"
#include "io.h"


int main()
{
    uart_init();
    
    command_hello();
    
    shell_init();
   
    
}