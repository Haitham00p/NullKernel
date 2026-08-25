#include "heap.h"
#include "kernel/panic/panic.h"
#include "kernel/debug/debug.h"
#include <stddef.h>

static uintptr_t HeapStart;
static uintptr_t HeapCurrent;
static uintptr_t HeapEnd;

typedef struct HeapBlock
{
    uint64_t Size;
    struct HeapBlock *Next;
} HeapBlock;

static HeapBlock *FreeList;

static void HeapSerialNumber(uint64_t Value)
{
    char Buffer[24];
    uint32_t Index = 0;

    if (Value == 0)
    {
        DbgSerialWrite("0");
        return;
    }
    while (Value != 0 && Index + 1U < sizeof(Buffer))
    {
        Buffer[Index++] = (char)('0' + (Value % 10U));
        Value /= 10U;
    }
    while (Index > 0)
    {
        DbgSerialPutChar(Buffer[--Index]);
    }
}

void HeapInitialize(uintptr_t Base, uint64_t Length)
{
    if (Base == 0 || Length == 0) {
        PanicKernel("Heap Base or Length Values is NULL", (uintptr_t)HeapInitialize);
    }

    HeapStart   = Base;
    HeapCurrent = Base;
    HeapEnd     = Base + Length;
    FreeList    = NULL;
}

void *kmalloc(uint64_t Size)
{
    HeapBlock **Walk = &FreeList;
    HeapBlock *Block;

    if (Size == 0)
    {
        Size = 1;
    }
    Size = (Size + 15U) & ~(uint64_t)15U;

    while (*Walk != NULL)
    {
        if ((*Walk)->Size >= Size)
        {
            Block = *Walk;
            *Walk = Block->Next;
            return (void *)(Block + 1);
        }
        Walk = &(*Walk)->Next;
    }

    if (HeapCurrent + Size + sizeof(HeapBlock) > HeapEnd)
    {
        DbgSerialWrite("HEAP OOM: size=");
        HeapSerialNumber(Size);
        DbgSerialWrite(" current=");
        HeapSerialNumber(HeapCurrent);
        DbgSerialWrite(" end=");
        HeapSerialNumber(HeapEnd);
        DbgSerialWrite("\n");
        PanicKernel(
            "Out Of Heap Memory",
            (uintptr_t)kmalloc
        );
    }

    Block = (HeapBlock *)HeapCurrent;
    Block->Size = Size;
    HeapCurrent += Size + sizeof(HeapBlock);

    if (Size >= (1U << 20))
    {
        DbgSerialWrite("HEAP BIG: size=");
        HeapSerialNumber(Size);
        DbgSerialWrite(" current=");
        HeapSerialNumber(HeapCurrent);
        DbgSerialWrite("\n");
    }

    return (void *)(Block + 1);
}

void kfree(void *Ptr)
{
    HeapBlock *Block;

    if (Ptr == NULL)
    {
        return;
    }
    Block = ((HeapBlock *)Ptr) - 1;
    Block->Next = FreeList;
    FreeList = Block;
}