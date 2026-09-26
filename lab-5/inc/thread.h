#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include "list.h"
#include "mm.h"

/* Keep this layout synchronized with switch.S. */
struct thread_context {
    uint64_t x19_x28[10];
    uint64_t fp, lr, sp;
    uint64_t d8_d15[8];
    uint64_t fpcr, fpsr;
};

enum thread_state { THREAD_RUNNABLE, THREAD_RUNNING, THREAD_DEAD };

typedef struct thread {
    struct thread_context context;
    struct list_head queue;
    unsigned int id;
    enum thread_state state;
    void (*entry)(void);
    page_t *allocation;
    uint64_t initial_daif;
    page_t *user_stack;
    struct process_image *user_image;
    void *user_frame;
    int killed;
} thread_t;

/* Call once after mm_init(), on the bootstrap stack (which becomes idle). */
void thread_init(void);
/* NULL on invalid entry or allocation failure. Pointer expires after reaping. */
thread_t *thread_create(void (*entry)(void));
thread_t *thread_alloc(void (*entry)(void));
void thread_publish(thread_t *t);
int thread_kill(unsigned int pid);
int thread_alive(unsigned int pid);
thread_t *get_current(void);
thread_t *current_thread(void);
#define THREAD_DEFAULT_QUANTUM_MS 10U
/* 1..1000 ms. Returns 0 on invalid input, 1 on success. */
int thread_set_quantum_ms(unsigned int ms);
unsigned int thread_get_quantum_ms(void);
/* Single-core nesting; protects thread-context bottom halves from scheduling. */
void preempt_disable(void);
void preempt_enable(void);
/* IRQ plumbing: exit is called with IRQs masked, after GIC EOI. */
void thread_irq_enter(void);
void thread_irq_exit(uint64_t interrupted_spsr);
void thread_request_reschedule(void);
/* Explicit yield remains available; never call from IRQ/bottom-half callbacks. */
void schedule(void);
void thread_exit(void) __attribute__((noreturn));
void kill_zombies(void);
void idle(void) __attribute__((noreturn));
void thread_test(void);
void thread_preempt_test(void);

#endif
