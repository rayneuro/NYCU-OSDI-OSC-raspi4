#include "cpio.h"
#include "uart.h"
#include "string.h"
#include "utils.h"

#include "process.h"
#include "thread.h"

void cpio_exec(char *filename)
{
    int pid = process_spawn(filename);
    if (pid < 0) {
        uart_async_send("Unable to load raw user program\n");
        return;
    }
    /* Foreground process owns UART input until exit; shell remains scheduled. */
    while (thread_alive((unsigned int)pid)) schedule();
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
		
		uart_async_send(addr+sizeof(struct cpio_header));
		uart_async_send("\n");
		
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
        uart_async_write(file_content,file_size);
        /*
		for (unsigned int i = 0; i < file_size; i++)
        {
            uart_async_write(file_content[i]);
        }*/
        uart_async_send("\n");
    }
    else
    {
        uart_async_send("Not found the file\n");
    }
}
