#ifndef MBR_H
#define MBR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct __attribute__((packed))
{
    uint8_t  BootIndicator;
    uint8_t  StartCHS[3];

    uint8_t  PartitionType;
    uint8_t  EndCHS[3];

    uint32_t StartLBA;
    uint32_t SectorCount;

} MBRPartitionEntry;

typedef struct __attribute__((packed))
{
    uint8_t BootCode[446];

    MBRPartitionEntry Partitions[4];

    uint16_t Signature;

} MBR;

_Static_assert(sizeof(MBRPartitionEntry) == 16,
               "An MBR partition entry must occupy exactly 16 bytes");
_Static_assert(sizeof(MBR) == 512,
               "An MBR sector must occupy exactly 512 bytes");

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