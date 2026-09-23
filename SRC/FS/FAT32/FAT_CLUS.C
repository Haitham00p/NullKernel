#include "fs/fat32/fat_cluster.h"

#include <stddef.h>

bool FATReadCluster(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t Cluster, void *Buffer)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)Cluster;
    (void)Buffer;

    /*
     * TODO: Implement raw cluster read logic:
     * 1. Validate parameters (Boot != NULL, Buffer != NULL, Cluster >= 2).
     * 2. Calculate starting sector LBA using FAT32ClusterToLBA(Boot, PartitionStartLBA, Cluster).
     * 3. Iterate from 0 to Boot->SectorsPerCluster - 1.
     * 4. Call IDEReadSector(LBA + sector_offset, Buffer + sector_offset * Boot->BytesPerSector).
     * 5. Return true if all sector reads succeed, false otherwise.
     */

    return false;
}

bool FATWriteCluster(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                     uint32_t Cluster, const void *Buffer)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)Cluster;
    (void)Buffer;

    /*
     * TODO: Implement raw cluster write logic:
     * 1. Validate parameters (Boot != NULL, Buffer != NULL, Cluster >= 2).
     * 2. Calculate starting sector LBA using FAT32ClusterToLBA(Boot, PartitionStartLBA, Cluster).
     * 3. Iterate from 0 to Boot->SectorsPerCluster - 1.
     * 4. Call IDEWriteSector(LBA + sector_offset, Buffer + sector_offset * Boot->BytesPerSector).
     * 5. Return true if all sector writes succeed, false otherwise.
     */

    return false;
}

bool FATClearCluster(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                     uint32_t Cluster)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)Cluster;

    /*
     * TODO: Implement raw cluster zero-fill logic:
     * 1. Validate parameters (Boot != NULL, Cluster >= 2).
     * 2. Allocate a zeroed sector buffer (size = Boot->BytesPerSector).
     * 3. Calculate starting sector LBA using FAT32ClusterToLBA(Boot, PartitionStartLBA, Cluster).
     * 4. Iterate over Boot->SectorsPerCluster, writing zeroed sectors using IDEWriteSector().
     * 5. Return true if all sector writes succeed, false otherwise.
     */

    return false;
}
