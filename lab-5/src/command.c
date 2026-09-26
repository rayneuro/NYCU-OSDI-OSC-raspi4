
#include "string.h"
#include "gpio.h"
#include "uart.h"
#include "time.h"
#include "command.h"
#include "cpio.h"
#include "mailbox.h"
#include "dtb.h"
#include "allocator.h"
#include "mm.h"
#include "timer.h"

extern void *_dtb_ptr;
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
    uart_async_send("[");
    uart_async_send(str);
    uart_async_send("]\n");
    
}

void command_hello()
{
    uart_async_send("Hello world!\n");
}

void command_timeout(const char *message, uint64_t seconds)
{
    if (setTimeout(message, seconds))
        uart_async_send("Timer scheduled.\n");
    else
        uart_async_send("Unable to schedule timer.\n");
}


void command_help()
{
    uart_async_send("Supported commands:\n");
    uart_async_send("\tthread_test : Run three round-robin threads\n");
    uart_async_send("\tpreempt_test: Run CPU-bound threads without yielding\n");
    uart_async_send("\tquantum [ms]: Show/set time slice (1..1000 ms)\n");
    uart_async_send("\thello       : Print Hello world\n");
    uart_async_send("\thelp        : Show this help message\n");
    uart_async_send("\treboot  : Reboot the rpi4 \n");
    uart_async_send("\ttimestamp   : Show current timestamp\n");
    uart_async_send("\tSetTimeout  : Schedule a timer callback\n");
    uart_async_send("\tboardvision : Print board vision\n");
    uart_async_send("\tVC address  : Print video core address\n");
    uart_async_send("\tloadimg     : Load the kernel image to target address\n");
    uart_async_send("\texec        : Run an initramfs raw binary in EL0\n");
    uart_async_send("\tls          : list the file \n");
    uart_async_send("\tcat         : list the file \n");
    uart_async_send("\tdtb	     : print device tree\n");
    uart_async_send("\tmalloc	     : give dynamic memory space\n");
    uart_async_send("\tma          : Initialize system of memory management\n");
}

void command_reboot()
{
    uart_async_send("Rebooting...\n");
    // Pi 4 's PM  module have short timeout will ignore or delay（race condition）。
    mmio_write(PM_WDOG, PM_PASSWORD | 100);    // Set watchdog timer to 100 tick
    
    mmio_write(PM_RSTC, PM_PASSWORD | 0x20); // Write to PM_RSTC to trigger a full reset
    while (1) asm("wfe");
}

void command_not_found(char * buf)
{
    uart_async_send("Command ");
    uart_async_send(buf);
    uart_async_send("not found\n");
    
}


void command_board_revision()
{
    char str[20];  
    uint32_t board_revision = mbox_get_board_revision();

    uart_async_send("Board Revision: ");
    if (board_revision)
    {
        itohex_str(board_revision, sizeof(uint32_t), str);
        uart_async_send(str);
        uart_async_send("\n");
    }
    else
    {
        uart_async_send("Unable to query serial!\n");
    }
}

void command_vc_base_addr()
{
    char str[20];  
    uint64_t vc_base_addr = mbox_get_VC_base_addr ();

    uart_async_send("VC Core Memory:\n");
    if ( vc_base_addr )
    {
        uart_async_send("    - Base Address: ");
        itohex_str((uint32_t)(vc_base_addr >> 32), sizeof(uint32_t), str);
        uart_async_send(str);
        uart_async_send(" (in bytes)\n");


        uart_async_send("    - Size: ");
        itohex_str((uint32_t)(vc_base_addr & 0xffffffff), sizeof(uint32_t), str);
        uart_async_send(str);
        uart_async_send(" (in bytes)\n");
    }
    else
    {
        uart_async_send("Unable to query serial!\n");
    }
}

void command_load_image()
{
    int32_t size = 0;
    int32_t is_receive_success = 0;
    char output_buffer[20];
    char *load_address;
    char *address_counter;

    uart_async_send("Start Loading Kernel Image...\n");
    uart_async_send("Please input kernel load address in decimal.(defualt: 0x80000): ");
    load_address = (char *)((unsigned long)uart_getint());
    uart_async_send("Please send kernel image from UART now:\n");

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

        uart_async_send("Kernel Loaded address: ");
        itohex_str( (uint64_t)load_address, sizeof(char *), output_buffer );
        uart_async_send(output_buffer);
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

void command_list_file(){
    cpio_ls();
}

void command_dtb(){
    fdt_traverse(print_dtb,_dtb_ptr);
}

void command_malloc(){
    char *a = kmalloc(sizeof("1234"));
    char *b = kmalloc(sizeof("567"));
    a[0] = '1';
	a[1] = '2';
	a[2] = '3';
	a[3] = '4';
	a[4] = '\0';
	b[0] = '5';
	b[1] = '6';
	b[2] = '7';
	b[3] = '\0';
	uart_async_send(a);
    uart_async_send("\n");
	uart_async_send(b);
	uart_async_send("\n");
	kfree(a);
	kfree(b);
}
