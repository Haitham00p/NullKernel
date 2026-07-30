#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stdbool.h>

#include "vbr.h"

#define FAT32_CLUSTER_FREE       0x00000000
#define FAT32_CLUSTER_BAD        0x0FFFFFF7
#define FAT32_CLUSTER_EOC        0x0FFFFFF8

bool FATReadEntry(
    const FAT32BootSector *Boot,
    uint32_t PartitionStartLBA,
    uint32_t Cluster,
    uint32_t *NextCluster
);

bool FATWriteEntry(
    const FAT32BootSector *Boot,
    uint32_t PartitionStartLBA,
    uint32_t Cluster,
    uint32_t Value
);

bool FATIsEndOfChain(
    uint32_t Cluster
);

bool FATIsFreeCluster(
    uint32_t Cluster
);

bool FATIsBadCluster(
    uint32_t Cluster
);

uint32_t FATFindFreeCluster(
    const FAT32BootSector *Boot,
    uint32_t PartitionStartLBA
);

#endif