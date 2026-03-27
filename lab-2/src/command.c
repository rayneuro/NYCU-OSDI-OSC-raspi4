
#include "string.h"
#include "uart.h"
#include "time.h"
#include "command.h"


// PM Registers
enum{
    PERIPHERAL_BASE = 0xFE000000,
    PM_BASE = PERIPHERAL_BASE + 0x001000,
    PM_PASSWORD = 0x5A000000,
    PM_RSTC = PM_BASE + 0x1c,
    PM_WDOG = PM_BASE + 0x24,

};


void command_timestamp()
{
    unsigned long int cnt_frq, cnt_pct;
    char str[20];

    asm volatile(
        "mrs %0, cntfrq_el0 \n\t"
        "mrs %1, cntpct_el0 \n\t"
        : "=r" (cnt_frq),  "=r" (cnt_pct)
        :
    );

    float time =  ((float)cnt_pct) / cnt_frq ; 

    ftoa(time, str,6);
    uart_puts("[");
    uart_puts(str);
    uart_puts("]\n");
    
}

void command_hello()
{
    uart_puts("Hello world!\n");
}


void command_help()
{
    uart_puts("Supported commands:\n");
    
    uart_puts("\thelp        : Show this help message\n");
    
    uart_puts("\ttimestamp   : Show current timestamp\n");

    uart_puts("\thello       : Print Hello world\n");
    
}

void command_reboot()
{
    uart_puts("Rebooting...\n");
    
    mmio_write(PM_RSTC, PM_PASSWORD | 0x20); // Write to PM_RSTC to trigger a full reset
    mmio_write(PM_WDOG, PM_PASSWORD | 1);    // Set watchdog timer to 1 tick
    while (1) asm("wfe");
}

void command_not_found(char * buf)
{
    uart_puts("Command ");
    uart_puts(buf);
    uart_puts(" not found\n");
    
}


void command_board_revision()
{
    char str[20];  
    uint32_t board_revision = mbox_get_board_revision();

    uart_puts("Board Revision: ");
    if (board_revision)
    {
        itohex_str(board_revision, sizeof(uint32_t), str);
        uart_puts(str);
        uart_puts("\n");
    }
    else
    {
        uart_puts("Unable to query serial!\n");
    }
}





