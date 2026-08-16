#include "tasklist.h"
#include "allocator.h"
#include "uart.h"

task_t *task_head = NULL;

void enqueue_task(task_t *new_task) {
    uint64_t flags = irq_save();

    // Special case: the list is empty or the new task has higher priority
    if (!task_head || new_task->priority < task_head->priority) {
		new_task->next = task_head;
        new_task->prev = NULL;
        if (task_head) {
            task_head->prev = new_task;
        }
        task_head = new_task;
    } else {
        // Find the correct position in the list
        task_t *current = task_head;
        while (current->next && current->next->priority <= new_task->priority) {
            current = current->next;
        }

        // Insert the new task
        new_task->next = current->next;
        new_task->prev = current;
        if (current->next) {
            current->next->prev = new_task;
        }
        current->next = new_task;
    }

    irq_restore(flags);
}

void create_task(task_callback callback, uint64_t priority) {

	task_t* task = simple_malloc(sizeof(task_t));
	if(!task) {
		return;
	}

	task->callback = callback;
	task->priority = priority;
		
	enqueue_task(task);
}

void execute_tasks(void) {
    while (1) {
        uint64_t flags = irq_save();
        task_t *task = task_head;

        if (!task) {
            irq_restore(flags);
            return;
        }

        task_head = task->next;
        if (task_head)
            task_head->prev = NULL;

        irq_restore(flags);

        task->callback();
        /* simple_free(task) is still needed when the allocator supports it. */
    }
}
