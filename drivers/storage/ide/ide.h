#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stdbool.h>

bool IDEInitialize(void);

bool IDEReadSector(
    uint32_t LBA,
    void *Buffer
);

bool IDEWriteSector(
    uint32_t LBA,
    const void *Buffer
);

#endif