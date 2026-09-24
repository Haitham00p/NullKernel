#ifndef NULLOS_PMM_H
#define NULLOS_PMM_H
#include <stdint.h>
#include <stdbool.h>
void PmmInitialize(void *MemMapResponse);
uint64_t PmmAllocPage(void);
bool PmmFreePage(uint64_t Physical);
void PmmReserveRange(uint64_t Base, uint64_t Length);
uint64_t PmmTotalPages(void);
uint64_t PmmFreePages(void);
#endif
