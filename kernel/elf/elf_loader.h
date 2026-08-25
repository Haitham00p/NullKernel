#ifndef NULLOS_ELF_LOADER_H
#define NULLOS_ELF_LOADER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint64_t Entry;
    uint64_t LowestVaddr;
    uint64_t HighestEnd;
} ElfLoadInfo;

bool ElfLoadImage(const void *Image, uint64_t ImageSize, ElfLoadInfo *Out);

#endif /* NULLOS_ELF_LOADER_H */
