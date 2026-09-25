#ifndef NULLOS_NVME_H
#define NULLOS_NVME_H

#include <stdbool.h>
#include <stdint.h>

bool NVMeInitialize(void);
bool NVMeIsReady(void);
bool NVMeReadSector(uint64_t LBA, void *Buffer);
bool NVMeWriteSector(uint64_t LBA, const void *Buffer);
uint64_t NVMeGetSectorCount(void);

#endif
