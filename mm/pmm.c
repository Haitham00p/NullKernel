#include "pmm.h"
#include "include/nullboot.h"
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096ULL
#define MAX_PHYS  (1ULL * 1024ULL * 1024ULL * 1024ULL) /* Current Limine compatibility mapping covers the first 1 GiB */
#define MAX_PAGES (MAX_PHYS / PAGE_SIZE)
#define BITMAP_BYTES (MAX_PAGES / 8ULL)

static uint8_t Bitmap[BITMAP_BYTES];
static uint64_t Total, Free;

static inline bool bit(uint64_t p)
{
    return (Bitmap[p >> 3] >> (p & 7U)) & 1U;
}

static inline void set(uint64_t p)
{
    Bitmap[p >> 3] |= (uint8_t)(1U << (p & 7U));
}

static inline void clr(uint64_t p)
{
    Bitmap[p >> 3] &= (uint8_t)~(1U << (p & 7U));
}

void PmmInitialize(void *Raw)
{
    const NullBootInfo *Boot = (const NullBootInfo *)Raw;
    const NullBootMemoryEntry *Map;

    for (uint64_t i = 0; i < BITMAP_BYTES; i++)
        Bitmap[i] = 0xFF;

    Total = 0;
    Free = 0;

    if (!Boot || Boot->Magic != NULLBOOT_MAGIC ||
        Boot->Version != NULLBOOT_VERSION ||
        Boot->MemoryMap == 0 || Boot->MemoryMapCount == 0)
        return;

    Map = (const NullBootMemoryEntry *)(uintptr_t)Boot->MemoryMap;

    for (uint32_t i = 0; i < Boot->MemoryMapCount; i++)
    {
        const NullBootMemoryEntry *Entry = &Map[i];

        if (Entry->Type != NULLBOOT_MEM_USABLE)
            continue;

        uint64_t a = (Entry->Base + PAGE_SIZE - 1ULL) &
                     ~(PAGE_SIZE - 1ULL);
        uint64_t end = (Entry->Base + Entry->Length) &
                       ~(PAGE_SIZE - 1ULL);

        if (end <= a || a >= MAX_PHYS)
            continue;
        if (end > MAX_PHYS)
            end = MAX_PHYS;

        for (uint64_t p = a; p < end; p += PAGE_SIZE)
        {
            uint64_t n = p / PAGE_SIZE;
            if (bit(n))
            {
                clr(n);
                Total++;
                Free++;
            }
        }
    }

    PmmReserveRange(0, 0x100000);
}

void PmmReserveRange(uint64_t Base, uint64_t Length)
{
    uint64_t a = Base & ~(PAGE_SIZE - 1ULL);
    uint64_t end = Base + Length;

    if (end < a)
        return;

    end = (end + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);

    for (uint64_t p = a; p < end && p < MAX_PHYS; p += PAGE_SIZE)
    {
        uint64_t n = p / PAGE_SIZE;
        if (!bit(n))
        {
            set(n);
            if (Free)
                Free--;
        }
    }
}

uint64_t PmmAllocPage(void)
{
    for (uint64_t n = 256; n < MAX_PAGES; n++)
    {
        if (!bit(n))
        {
            set(n);
            if (Free)
                Free--;

            uint64_t p = n * PAGE_SIZE;
            for (uint64_t i = 0; i < 512; i++)
                ((volatile uint64_t *)(uintptr_t)p)[i] = 0;

            return p;
        }
    }

    return 0;
}

bool PmmFreePage(uint64_t Physical)
{
    if ((Physical & (PAGE_SIZE - 1ULL)) != 0 ||
        Physical >= MAX_PHYS)
        return false;

    uint64_t n = Physical / PAGE_SIZE;

    if (!bit(n))
        return false;

    clr(n);
    Free++;
    return true;
}

uint64_t PmmTotalPages(void)
{
    return Total;
}

uint64_t PmmFreePages(void)
{
    return Free;
}
