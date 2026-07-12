#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>

void utils_align(void *size, unsigned int s);
uint32_t utils_align_up(uint32_t size, int alignment);
#endif