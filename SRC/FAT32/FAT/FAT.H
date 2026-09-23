#ifndef FAT32_FAT_H
#define FAT32_FAT_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/vbr/vbr.h"

/* FAT32 entries use only their low 28 bits. */
#define FAT32_CLUSTER_MASK       0x0FFFFFFFU
#define FAT32_CLUSTER_FREE       0x00000000U
#define FAT32_CLUSTER_BAD        0x0FFFFFF7U
#define FAT32_CLUSTER_EOC        0x0FFFFFF8U
#define FAT32_CLUSTER_EOC_LAST   0x0FFFFFFFU

/*
 * Read or update an entry in the file allocation table.  PartitionStartLBA
 * is the LBA of the FAT32 volume's boot sector, not the disk's MBR LBA.
 *
 * FATFindFreeCluster returns 0 when no free data cluster is available.
 */
bool FATReadEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                  uint32_t Cluster, uint32_t *NextCluster);

bool FATWriteEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                   uint32_t Cluster, uint32_t Value);

bool FATIsEndOfChain(uint32_t Cluster);
bool FATIsFreeCluster(uint32_t Cluster);
bool FATIsBadCluster(uint32_t Cluster);

uint32_t FATFindFreeCluster(const FAT32BootSector *Boot,
                            uint32_t PartitionStartLBA);

#endif
