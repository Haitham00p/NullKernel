#include "heap.h"
#include "kernel/panic/panic.h"
#include "kernel/debug/debug.h"
#include <stddef.h>

/*
 * Overflow-resistant heap.
 *
 * Every allocation carries a header (magic + size + next) and a tail guard
 * word immediately after the user region.  kfree() validates both the header
 * magic and the tail guard before threading a block back onto the free list.
 * If either is corrupted (i.e. a caller wrote past its buffer), the block is
 * quietly quarantined (leaked) instead of being chained with a smashed Next
 * pointer.  That is what previously produced the bogus kfree(0x28) fault and,
 * downstream, the #UD in the kernel when a corrupted free-list node was
 * reused as a return/jump target.  Guests therefore survive a stray overflow
 * instead of taking the whole OS down -- which is what lets Ctrl+C interrupt
 * them reliably.
 */

#define HEAP_MAGIC     0x4E554C4C4F534058ULL  /* "NULLOS@" */
#define HEAP_TAIL      0x58404F534C4F4E55ULL  /* reverse */

typedef struct HeapBlock
{
    uint64_t            Magic;
    uint64_t            Size;     /* user bytes (excludes header + tail) */
    struct HeapBlock   *Next;
    uint64_t            Pad;      /* keep header 16-byte aligned (32 bytes) */
} HeapBlock;

#define HEAP_BLOCK_BYTES  ((uint64_t)sizeof(HeapBlock))   /* 32, 16-aligned */
#define HEAP_TAIL_BYTES   ((uint64_t)sizeof(uint64_t))    /* 8 */

static uintptr_t HeapStart;
static uintptr_t HeapCurrent;
static uintptr_t HeapEnd;

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

static inline void HeapSetTail(HeapBlock *Block)
{
    uint64_t *Tail = (uint64_t *)((uint8_t *)(Block + 1) + Block->Size);
    *Tail = HEAP_TAIL;
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
    HeapBlock  *Block;

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
            Block->Size = Size;
            HeapSetTail(Block);
            return (void *)(Block + 1);
        }
        Walk = &(*Walk)->Next;
    }

    if (HeapCurrent + HEAP_BLOCK_BYTES + Size + HEAP_TAIL_BYTES > HeapEnd)
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
    Block->Magic = HEAP_MAGIC;
    Block->Size  = Size;
    Block->Next  = NULL;
    HeapCurrent += HEAP_BLOCK_BYTES + Size + HEAP_TAIL_BYTES;
    HeapSetTail(Block);

    return (void *)(Block + 1);
}

void kfree(void *Ptr)
{
    HeapBlock *Block;

    if (Ptr == NULL)
    {
        return;
    }

    /* Reject obviously-bogus pointers (e.g. a near-NULL dangling free). */
    if ((uintptr_t)Ptr < HeapStart + HEAP_BLOCK_BYTES ||
        (uintptr_t)Ptr >= HeapEnd)
    {
        return;
    }

    Block = ((HeapBlock *)Ptr) - 1;

    if ((uintptr_t)Block < HeapStart ||
        (uintptr_t)Block + HEAP_BLOCK_BYTES > HeapEnd)
    {
        return;
    }

    /* Corrupted or non-heap block: quarantine instead of chaining a smashed
       header into the free list. */
    if (Block->Magic != HEAP_MAGIC)
    {
        return;
    }

    if (Block->Size == 0 ||
        Block->Size > (HeapEnd - HeapStart))
    {
        return;
    }

    /* Detect an overrun past the user region by checking the tail guard. */
    {
        uint64_t *Tail = (uint64_t *)((uint8_t *)(Block + 1) + Block->Size);
        if (*Tail != HEAP_TAIL)
        {
            /* A caller overflowed this buffer.  Drop the block rather than
               let the corruption propagate through the allocator. */
            return;
        }
    }

    Block->Next = FreeList;
    FreeList = Block;
}
