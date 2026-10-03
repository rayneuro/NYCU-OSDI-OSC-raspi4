#include "cpio.h"
#include "uart.h"
#include "string.h"
#include "utils.h"

static unsigned char user_stack[16384] __attribute__((aligned(16)));
extern void enter_el0(const void *entry, void *stack_top)
    __attribute__((noreturn));

void cpio_exec(char *filename)
{
    if (!cpio_addr) {
        uart_puts("Initramfs is not available\n");
        return;
    }

    char *target = findFile(filename);
    if (!target) {
        uart_puts("Not found the file\n");
        return;
    }

    struct cpio_header *header = (struct cpio_header *)target;
    unsigned long size = utils_atoi(header->c_filesize, 8);
    unsigned long mode = utils_atoi(header->c_mode, 8);
    unsigned long offset = sizeof(*header) + utils_atoi(header->c_namesize, 8);
    utils_align(&offset, 4);
    unsigned char *entry = (unsigned char *)target + offset;

    if ((mode & 0170000) != 0100000 || size < 4 ||
        ((uintptr_t)entry & 3) ||
        (entry[0] == 0x7f && entry[1] == 'E' &&
         entry[2] == 'L' && entry[3] == 'F')) {
        uart_puts("Expected an AArch64 position-independent raw binary\n");
        return;
    }

    uart_puts("Entering EL0\n");
    enter_el0(entry, user_stack + sizeof(user_stack));
}

char *findFile(char *name)
{
    char *addr = (char *)cpio_addr;
    while (strcmp((char *)(addr + sizeof(struct cpio_header)), "TRAILER!!!") != 0)
    {
        if ((strcmp((char *)(addr + sizeof(struct cpio_header)), name) == 0))
        {
            return addr;
        }
        struct cpio_header* header = (struct cpio_header *)addr;
        unsigned long pathname_size = utils_atoi(header->c_namesize,(int)sizeof(header->c_namesize));;
        unsigned long file_size =  utils_atoi(header->c_filesize,(int)sizeof(header->c_filesize));
        unsigned long headerPathname_size = sizeof(struct cpio_header) + pathname_size;

        utils_align(&headerPathname_size,4); 
        utils_align(&file_size,4);           
        addr += (headerPathname_size + file_size);
    }
    return 0;
}

void cpio_ls(){
	char* addr = (char*) cpio_addr;

	while(strcmp((char *)(addr+sizeof(struct cpio_header)),"TRAILER!!!") != 0){
		

		struct cpio_header* header = (struct cpio_header*) addr;
		unsigned long filename_size = utils_atoi(header->c_namesize,(int)sizeof(header->c_namesize));
		unsigned long headerPathname_size = sizeof(struct cpio_header) + filename_size;
		unsigned long file_size = utils_atoi(header->c_filesize,(int)sizeof(header->c_filesize));
	    
		utils_align(&headerPathname_size,4);
		utils_align(&file_size,4);
		
		uart_puts(addr+sizeof(struct cpio_header));
		uart_puts("\n");
		
		addr += (headerPathname_size + file_size);
	}
}


void cpio_cat(char *filename)
{
    char *target = findFile(filename);
    if (target)
    {
        struct cpio_header *header = (struct cpio_header *)target;
        unsigned long pathname_size = utils_atoi(header->c_namesize,(int)sizeof(header->c_namesize));
        unsigned long file_size = utils_atoi(header->c_filesize,(int)sizeof(header->c_filesize));
        unsigned long headerPathname_size = sizeof(struct cpio_header) + pathname_size;

        utils_align(&headerPathname_size,4); 
        utils_align(&file_size,4);           

        char *file_content = target + headerPathname_size;
		for (unsigned int i = 0; i < file_size; i++)
        {
            uart_write_char(file_content[i]);
        }
        uart_puts("\n");
    }
    else
    {
        uart_puts("Not found the file\n");
    }
}
