#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

void HeapInitialize(uintptr_t Base, uint64_t Length);

void *kmalloc(uint64_t Size);

#endif