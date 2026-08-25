#include "allocator.h"
#include "irq.h"

#define SIMPLE_MALLOC_BUFFER_SIZE 8192UL
#define SIMPLE_MALLOC_ALIGNMENT   8UL

typedef struct simple_block {
	unsigned long size;
	struct simple_block *next;
	unsigned long free;
} simple_block_t;

/* Ensure the first block header is naturally aligned. */
static union {
	unsigned long alignment;
	unsigned char bytes[SIMPLE_MALLOC_BUFFER_SIZE];
} simple_heap;

static simple_block_t *simple_block_head;

static void simple_allocator_init(void)
{
	simple_block_head = (simple_block_t *)simple_heap.bytes;
	simple_block_head->size = SIMPLE_MALLOC_BUFFER_SIZE
				       - sizeof(simple_block_t);
	simple_block_head->next = 0;
	simple_block_head->free = 1;
}

static void simple_merge_next(simple_block_t *block)
{
	while (block->next && block->next->free) {
		block->size += sizeof(simple_block_t) + block->next->size;
		block->next = block->next->next;
	}
}

void *simple_malloc(unsigned long size)
{
	simple_block_t *block;
	uint64_t flags = irq_save();

	if (!simple_block_head)
		simple_allocator_init();

	if (!size || size > ~0UL - (SIMPLE_MALLOC_ALIGNMENT - 1)) {
		irq_restore(flags);
		return 0;
	}

	size = (size + SIMPLE_MALLOC_ALIGNMENT - 1)
	       & ~(SIMPLE_MALLOC_ALIGNMENT - 1);

	for (block = simple_block_head; block; block = block->next) {
		if (!block->free || block->size < size)
			continue;

		/* Split only when the remainder can hold a useful block. */
		if (block->size >= size + sizeof(simple_block_t)
				  + SIMPLE_MALLOC_ALIGNMENT) {
			simple_block_t *remainder =
				(simple_block_t *)((unsigned char *)(block + 1)
						   + size);

			remainder->size = block->size - size
					  - sizeof(simple_block_t);
			remainder->next = block->next;
			remainder->free = 1;
			block->size = size;
			block->next = remainder;
		}

		block->free = 0;
		irq_restore(flags);
		return (void *)(block + 1);
	}

	irq_restore(flags);
	return 0;
}

void simple_free(void *object)
{
	simple_block_t *block;
	simple_block_t *previous = 0;
	uint64_t flags;

	if (!object)
		return;

	flags = irq_save();

	/* Find an exact allocation boundary instead of trusting any pointer. */
	for (block = simple_block_head; block; block = block->next) {
		if ((void *)(block + 1) == object)
			break;
		previous = block;
	}

	if (!block || block->free) {
		irq_restore(flags);
		return;
	}

	block->free = 1;
	simple_merge_next(block);

	if (previous && previous->free)
		simple_merge_next(previous);

	irq_restore(flags);
}
