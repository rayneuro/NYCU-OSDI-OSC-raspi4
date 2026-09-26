#include "startup.h"

/* This small bootstrap table itself lives in the reserved kernel BSS. */
startup_block_t startup_blocks[STARTUP_MAX_BLOCKS];
unsigned int startup_block_count;
static uintptr_t cursor, limit;
static int active;

void startup_init(uintptr_t start, uintptr_t end)
{
    cursor = start;
    limit = end;
    startup_block_count = 0;
    active = start < end;
}

int startup_reserve(uintptr_t start, size_t size)
{
    if (!active || !size || size > UINTPTR_MAX - start ||
        startup_block_count == STARTUP_MAX_BLOCKS)
        return -1;

    startup_blocks[startup_block_count].start = start;
    startup_blocks[startup_block_count++].size = size;
    return 0;
}

void *startup_alloc(size_t size, size_t alignment)
{
    uintptr_t candidate = cursor;

    if (!active || !size || !alignment || (alignment & (alignment - 1)))
        return NULL;

    for (;;) {
        unsigned int i;

        if (candidate > UINTPTR_MAX - (alignment - 1))
            return NULL;
        candidate = (candidate + alignment - 1) & ~(uintptr_t)(alignment - 1);
        if (candidate >= limit || size > limit - candidate)
            return NULL;

        /* Restart the scan after each conflict: reservations may be unsorted. */
        for (i = 0; i < startup_block_count; i++) {
            uintptr_t end = startup_blocks[i].start + startup_blocks[i].size;

            if (candidate < end && candidate + size > startup_blocks[i].start) {
                candidate = end;
                break;
            }
        }
        if (i == startup_block_count)
            break;
    }

    if (!candidate || startup_reserve(candidate, size) < 0)
        return NULL;
    cursor = candidate + size;
    return (void *)candidate;
}

void startup_finish(void)
{
    active = 0;
}
