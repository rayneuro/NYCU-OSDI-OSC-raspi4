
#include "io.h"
#include "string.h"
#include "command.h"


// PM Registers UL / ULL postifx is safe
#define PERIPHERAL_BASE     0xFE000000UL
#define PM_BASE             (PERIPHERAL_BASE + 0x100000UL)
#define PM_PASSWORD         0x5A000000UL
#define PM_RSTC             (PM_BASE + 0x1c)
#define PM_WDOG             (PM_BASE + 0x24)


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
    // Pi 4 's PM  module have short timeout will ignore or delay（race condition）。
    mmio_write(PM_WDOG, PM_PASSWORD | 100);    // Set watchdog timer to 100 tick
    
    mmio_write(PM_RSTC, PM_PASSWORD | 0x20); // Write to PM_RSTC to trigger a full reset
   
    //asm volatile ("dmb sy" ::: "memory");
    //for (volatile int i = 0; i < 100000; i++) asm("nop");  
    
    while (1) asm("wfe");
}

void command_not_found(char * buf)
{
    uart_puts("Command ");
    uart_puts(buf);
    uart_puts(" not found\n");
    
}





