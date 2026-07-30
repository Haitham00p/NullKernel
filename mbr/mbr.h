#ifndef MBR_H
#define MBR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t  BootIndicator;
    uint8_t  StartCHS[3];

    uint8_t  PartitionType;
    uint8_t  EndCHS[3];

    uint32_t StartLBA;
    uint32_t SectorCount;

} MBRPartitionEntry;

typedef struct
{
    uint8_t BootCode[446];

    MBRPartitionEntry Partitions[4];

    uint16_t Signature;

} MBR;

bool MBRIsValid(
    const MBR *Table
);

const MBRPartitionEntry *
MBRGetPartition(
    const MBR *Table,
    uint8_t Index
);

uint8_t MBRGetPartitionCount(
    const MBR *Table
);

#endif