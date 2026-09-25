#ifndef NULLOS_BLOCK_H
#define NULLOS_BLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define BLOCK_SECTOR_SIZE 512U

bool BlockInitialize(void);
bool BlockIsReady(void);
bool BlockRead(uint64_t LBA, void *Buffer);
bool BlockWrite(uint64_t LBA, const void *Buffer);

#endif
