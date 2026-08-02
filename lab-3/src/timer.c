#include "timer.h"
#include "allocator.h"
#include "uart.h"
#include "irq.h"
#include "utils.h"

timer_t *timer_head = NULL;

void add_timer(timer_t *new_timer) {
    uint64_t flags = irq_save();

    /* 修改 timer linked list */
    irq_restore(flags);

}


void create_timer(timer_callback callback, void* data, uint64_t after) {
	//Allocate memory for the timer
	timer_t* timer = simple_malloc(sizeof(timer_t));
	if(!timer) {
		return;
	}

	//Set the callback and data
	timer->callback = callback;
	timer->data = data;

	//Calculate the expiry time
	uint64_t current_time, cntfrq;
	asm volatile("mrs %0, cntpct_el0":"=r"(current_time));
	asm volatile("mrs %0, cntfrq_el0":"=r"(cntfrq));
	timer->expiry = current_time + after * cntfrq;
	//Add the time to the list
	add_timer(timer);
}


void print_message(void *data) {
	char *message = data;
	uint64_t current_time, cntfrq;
    asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));
    uint64_t seconds = current_time / cntfrq;

	uart_puts("Timeout message: ");
	uart_puts(message);
	uart_puts(" occurs at ");
	uart_hex(seconds);
	uart_puts("\n");
}

void setTimeout(char *message,uint64_t seconds) {
	
	char *message_copy = utils_strdup(message);

	if(!message_copy){
		return;
	}
	
	create_timer(print_message,message_copy,seconds);
}
