#include "tasklist.h"
#include "allocator.h"
#include "uart.h"
#include "thread.h"

task_t *task_head = NULL;
/* A smaller number means a higher priority.  UINT64_MAX is the idle level. */
static uint64_t current_task_priority = UINT64_MAX;

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

int create_task(task_callback callback, uint64_t priority) {

	task_t* task = simple_malloc(sizeof(task_t));
	if(!task) {
		return 0;
	}

	task->callback = callback;
	task->priority = priority;
		
	enqueue_task(task);
    return 1;
}

void execute_tasks(void) {
    preempt_disable();
    uint64_t flags = irq_save();
    uint64_t preempted_priority = current_task_priority;

    irq_restore(flags);

    while (1) {
        uint64_t previous_priority;
        flags = irq_save();
        task_t *task = task_head;

        /*
         * The queue is sorted by priority.  A task may preempt only when it
         * has a strictly higher priority than the task interrupted by this
         * invocation of execute_tasks().  Equal/lower-priority work remains
         * queued until that task completes.
         */
        if (!task || task->priority >= preempted_priority) {
            irq_restore(flags);
            preempt_enable();
            return;
        }

        task_head = task->next;
        if (task_head)
            task_head->prev = NULL;

        previous_priority = current_task_priority;
        current_task_priority = task->priority;
        irq_restore(flags);

        task->callback();

        /*
         * Restore the preempted task's priority as a stack would.  Keep this
         * update and the task-node reclamation atomic with respect to IRQs.
         */
        flags = irq_save();
        current_task_priority = previous_priority;
        simple_free(task);
        irq_restore(flags);
    }
}
