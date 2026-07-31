#include "heap.h"
#include "kernel/panic/panic.h"

static uintptr_t HeapStart;
static uintptr_t HeapCurrent;
static uintptr_t HeapEnd;

void HeapInitialize(uintptr_t Base, uint64_t Length)
{   
    if (Base == 0 || Length == 0) {
        PanicKernel("Heap Base or Length Values is NULL", (uintptr_t)HeapInitialize);
    }

    HeapStart   = Base;
    HeapCurrent = Base;
    HeapEnd     = Base + Length;
}

void *kmalloc(uint64_t Size)
{
    if (HeapCurrent + Size > HeapEnd)
    {
        PanicKernel(
            "Out Of Heap Memory",
            (uintptr_t)kmalloc
        );
    }

    uintptr_t Allocation = HeapCurrent;

    HeapCurrent += Size;

    return (void *)Allocation;
}
