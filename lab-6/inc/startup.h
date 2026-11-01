#ifndef STARTUP_H
#define STARTUP_H

#include <stddef.h>
#include <stdint.h>

#define STARTUP_MAX_BLOCKS 128

typedef struct {
    uintptr_t start;
    size_t size;
} startup_block_t;

extern startup_block_t startup_blocks[STARTUP_MAX_BLOCKS];
extern unsigned int startup_block_count;

/* Boot-time only; intervals are [start, end). No page allocator is needed. */
void startup_init(uintptr_t start, uintptr_t end);
int startup_reserve(uintptr_t start, size_t size);
void *startup_alloc(size_t size, size_t alignment);
/* Permanently disable allocations after the buddy handoff. */
void startup_finish(void);

#endif
