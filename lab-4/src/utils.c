#include "utils.h"
void utils_align(void *size, unsigned int s) {
	unsigned long* x = (unsigned long*) size;
	unsigned long mask = s-1;
	*x = ((*x) + mask) & (~mask);
}

uint32_t utils_align_up(uint32_t size, int alignment) {
  return (size + alignment - 1) & ~(alignment-1);
}