
#include "string.h"
#include "gpio.h"
#include "uart.h"
#include "time.h"
#include "command.h"
#include "type.h"


// PM Registers
// using a macro is safer than using an enum
#define PM_BASE             (0xFE000000UL + 0x100000UL)
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
    uart_puts("\tboardvision : Print board vision\n");
    uart_puts("\tVC address  : Print video core address\n");

    
}

void command_reboot()
{
    uart_puts("Rebooting...\n");
    // Pi 4 's PM  module have short timeout will ignore or delay（race condition）。
    mmio_write(PM_WDOG, PM_PASSWORD | 100);    // Set watchdog timer to 100 tick
    
    mmio_write(PM_RSTC, PM_PASSWORD | 0x20); // Write to PM_RSTC to trigger a full reset
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

void command_vc_base_addr()
{
    char str[20];  
    uint64_t vc_base_addr = mbox_get_VC_base_addr ();

    uart_puts("VC Core Memory:\n");
    if ( vc_base_addr )
    {
        uart_puts("    - Base Address: ");
        itohex_str((uint32_t)(vc_base_addr >> 32), sizeof(uint32_t), str);
        uart_puts(str);
        uart_puts(" (in bytes)\n");


        uart_puts("    - Size: ");
        itohex_str((uint32_t)(vc_base_addr & 0xffffffff), sizeof(uint32_t), str);
        uart_puts(str);
        uart_puts(" (in bytes)\n");
    }
    else
    {
        uart_puts("Unable to query serial!\n");
    }
}

void command_load_image()
{
    int32_t size = 0;
    int32_t is_receive_success = 0;
    char output_buffer[20];
    char *load_address;
    char *address_counter;

    uart_puts("Start Loading Kernel Image...\n");
    uart_puts("Please input kernel load address in decimal.(defualt: 0x80000): ");
    load_address = (char *)((unsigned long)uart_getint());
    uart_puts("Please send kernel image from UART now:\n");

    wait_cycles(5000);

    if ( load_address == 0 )
        load_address = (char *)0x80000;

    do {

        // start signal to receive image
        uart_write_char(3);
        uart_write_char(3);
        uart_write_char(3);

        // read the kernel's size
        size  = uart_readByte();
        size |= uart_readByte() << 8;
        size |= uart_readByte() << 16;
        size |= uart_readByte() << 24;

        // send negative or positive acknowledge
        if(size<64 || size>1024*1024)
        {
            // size error
            uart_write_char('S');
            uart_write_char('E');            
            
            continue;
        }
        uart_write_char('O');
        uart_write_char('K');

        address_counter = load_address;
        
        // start from 0x80000
        while ( size-- ) 
        {
            mmio_write(address_counter++, uart_readByte());
        }

        is_receive_success = 1;

        uart_puts("Kernel Loaded address: ");
        itohex_str( (uint64_t)load_address, sizeof(char *), output_buffer );
        uart_puts(output_buffer);
        uart_write_char('\n');

        wait_cycles(5000);

    } while ( !is_receive_success );
   
    // restore arguments and jump to the new kernel.
    asm volatile (
        // we must force an absolute address to branch to
        "mov x30, 0x80000;"
        "ret"
    );
}




