#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stdbool.h>

bool IDEInitialize(void);

/* Switches the active IDE device to the first ATA hard disk. */
bool IDESelectDisk(void);

/* Switches the active IDE device back to the ATAPI CD-ROM. */
bool IDESelectCD(void);

/* Block-layer API: these ALWAYS target the hard disk, even when the CD-ROM
   is the current "active" device (the installer switches between them). */
bool IDEReadDiskSector(
    uint32_t LBA,
    void *Buffer
);

bool IDEWriteDiskSector(
    uint32_t LBA,
    const void *Buffer
);

/* ISO9660 API: ALWAYS targets the ATAPI CD-ROM. */
bool IDEReadCDSector(
    uint32_t LBA,
    void *Buffer
);

/* Whether an ATAPI CD-ROM / ATA hard disk was probed by the driver. */
bool IDEIsReadyCD(void);
bool IDEIsReadyDisk(void);

/* Capacity of the ATA hard disk in 512-byte sectors (0 if none). */
uint64_t IDEGetSectorCount(void);

/* "Active device" API used by the shell / debug paths.  Routes to the
   currently selected IDE device (disk or CD-ROM). */
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