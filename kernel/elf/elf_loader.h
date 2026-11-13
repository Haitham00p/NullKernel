#ifndef NULLOS_ELF_LOADER_H
#define NULLOS_ELF_LOADER_H

#include <stdbool.h>
#include <stdint.h>

#define ELF_PROGRAM_IMAGE_BASE 0x00100000ULL
#define ELF_PROGRAM_IMAGE_END  0x01000000ULL

typedef struct
{
    uint64_t Entry;
    uint64_t LowestVaddr;
    uint64_t HighestEnd;
} ElfLoadInfo;

bool ElfLoadImage(const void *Image, uint64_t ImageSize, ElfLoadInfo *Out);

#endif /* NULLOS_ELF_LOADER_H */
