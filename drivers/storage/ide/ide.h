#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stdbool.h>

bool IDEInitialize(void);

/* Switches the active IDE device to the first ATA hard disk. */
bool IDESelectDisk(void);

/* Switches the active IDE device back to the ATAPI CD-ROM. */
bool IDESelectCD(void);

bool IDEReadSector(
    uint32_t LBA,
    void *Buffer
);

bool IDEWriteSector(
    uint32_t LBA,
    const void *Buffer
);

bool IDEIsATAPI(void);

#endif