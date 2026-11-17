#include "timer.h"
#include "allocator.h"
#include "uart.h"
#include "irq.h"
#include "utils.h"
#include "string.h"
#include "tasklist.h"
#include "thread.h"

timer_t_p *timer_head = NULL;
static int timer_task_pending = 0;
static uint64_t slice_ticks;
static uint64_t slice_deadline;

static uint64_t counter_now(void)
{
    uint64_t now;
    asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    return now;
}

static void timer_program_next_locked(void)
{
    uint64_t deadline = slice_deadline;
    int enabled = slice_ticks != 0;
    /* An outstanding bottom half owns expired callbacks, not the slice. */
    if (timer_head && !timer_task_pending &&
        (!enabled || timer_head->expiry < deadline)) {
        deadline = timer_head->expiry;
        enabled = 1;
    }
	if (enabled) {
		asm volatile("msr cntp_cval_el0, %0"
				     :
				     : "r"(deadline)
				     : "memory");
		asm volatile("msr cntp_ctl_el0, %0"
				     :
				     : "r"(1UL)
				     : "memory");
	} else {
		asm volatile("msr cntp_ctl_el0, %0"
				     :
				     : "r"(0UL)
				     : "memory");
	}

	asm volatile("isb" ::: "memory");
}

void timer_reset_slice(void)
{
    uint64_t flags = irq_save();
    if (slice_ticks)
        slice_deadline = counter_now() + slice_ticks;
    timer_program_next_locked();
    irq_restore(flags);
}

void timer_set_quantum_ms(unsigned int ms)
{
    uint64_t flags = irq_save();
    uint64_t frequency;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    slice_ticks = (frequency * ms + 999) / 1000;
    if (!slice_ticks)
        slice_ticks = 1;
    timer_reset_slice();
    irq_restore(flags);
}

void add_timer(timer_t_p *new_timer)
{
	timer_t_p *current;
	uint64_t flags;
	int became_head = 0;

	if (!new_timer)
		return;

	flags = irq_save();
	new_timer->prev = NULL;
	new_timer->next = NULL;

	if (!timer_head || new_timer->expiry < timer_head->expiry) {
		new_timer->next = timer_head;
		if (timer_head)
			timer_head->prev = new_timer;
		timer_head = new_timer;
		became_head = 1;
	} else {
		current = timer_head;
		while (current->next &&
		       current->next->expiry <= new_timer->expiry)
			current = current->next;

		new_timer->next = current->next;
		new_timer->prev = current;
		if (current->next)
			current->next->prev = new_timer;
		current->next = new_timer;
	}

	/* A pending bottom-half owns the timer until it programs the next one. */
	if (became_head && !timer_task_pending)
		timer_program_next_locked();

	irq_restore(flags);
}

int create_timer(timer_callback callback, void *data, uint64_t after)
{
	uint64_t current_time;
	uint64_t cntfrq;
	timer_t_p *timer;

	if (!callback)
		return 0;

	asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));
	asm volatile("mrs %0, cntfrq_el0" : "=r"(cntfrq));

	/* Reject a delay whose conversion to counter ticks would overflow. */
	if (cntfrq && after > (~(uint64_t)0 - current_time) / cntfrq)
		return 0;

	timer = simple_malloc(sizeof(timer_t_p));
	if (!timer)
		return 0;

	//Set the callback and data
	timer->callback = callback;
	timer->data = data;

	timer->expiry = current_time + after * cntfrq;
	add_timer(timer);
	return 1;
}

static void timer_expiry_task(void)
{
	for (;;) {
		timer_t_p *expired;
		uint64_t current_time;
		uint64_t flags = irq_save();

		asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));

		if (!timer_head || timer_head->expiry > current_time) {
			timer_task_pending = 0;
			timer_program_next_locked();
			irq_restore(flags);
			return;
		}

		expired = timer_head;
		timer_head = expired->next;
		if (timer_head)
			timer_head->prev = NULL;

		irq_restore(flags);
		expired->callback(expired->data);
		simple_free(expired);
	}
}

void timer_irq_handler(void)
{
	/* Top-half: deassert the level-triggered PPI before GICC_EOIR. */
	asm volatile("msr cntp_ctl_el0, %0"
			     :
			     : "r"(0UL)
			     : "memory");
	asm volatile("isb" ::: "memory");

    uint64_t now = counter_now();
    if (slice_ticks && now >= slice_deadline) {
        thread_request_reschedule();
        /* Rearm even if a critical section defers the actual switch. */
        slice_deadline = now + slice_ticks;
    }
    if (timer_head && timer_head->expiry <= now && !timer_task_pending) {
        timer_task_pending = 1;
        if (!create_task(timer_expiry_task, 3))
            timer_task_pending = 0;
    }
    timer_program_next_locked();
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
	simple_free(message);
}

int setTimeout(const char *message, uint64_t seconds)
{
	char *message_copy;

	if (!message || !*message)
		return 0;

	message_copy = utils_strdup(message);

	if (!message_copy)
		return 0;

	if (!create_timer(print_message, message_copy, seconds)) {
		simple_free(message_copy);
		return 0;
	}

	return 1;
}
