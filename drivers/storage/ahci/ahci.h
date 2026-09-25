#ifndef NULLOS_AHCI_H
#define NULLOS_AHCI_H

#include <stdbool.h>
#include <stdint.h>

bool AHCIInitialize(void);
bool AHCIIsReady(void);
bool AHCIReadSector(uint64_t LBA, void *Buffer);
bool AHCIWriteSector(uint64_t LBA, const void *Buffer);

#endif
