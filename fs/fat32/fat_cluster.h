#ifndef FAT_CLUSTER_H
#define FAT_CLUSTER_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/vbr/vbr.h"

/* Return status codes for cluster-level operations. */
typedef enum
{
    FAT_CLUSTER_SUCCESS = 0,
    FAT_CLUSTER_ERROR_INVALID_PARAM,
    FAT_CLUSTER_ERROR_IO,
    FAT_CLUSTER_ERROR_BAD_CLUSTER,
    FAT_CLUSTER_ERROR_OUT_OF_BOUNDS
} FATClusterStatus;

/*
 * Reads an entire data cluster from disk into Buffer.
 * Buffer must be allocated with at least (Boot->SectorsPerCluster * Boot->BytesPerSector) bytes.
 *
 * @param Boot               Pointer to the validated FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 volume's boot sector.
 * @param Cluster            Target data cluster index (>= 2).
 * @param Buffer             Output memory buffer.
 * @return true on success, false on error.
 */
bool FATReadCluster(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t Cluster, void *Buffer);

/*
 * Writes an entire data cluster from Buffer onto disk.
 * Buffer must contain at least (Boot->SectorsPerCluster * Boot->BytesPerSector) bytes.
 *
 * @param Boot               Pointer to the validated FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 volume's boot sector.
 * @param Cluster            Target data cluster index (>= 2).
 * @param Buffer             Input memory buffer containing sector data.
 * @return true on success, false on error.
 */
bool FATWriteCluster(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                     uint32_t Cluster, const void *Buffer);

/*
 * Clears (zero-fills) all sectors of the specified data cluster on disk.
 *
 * @param Boot               Pointer to the validated FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 volume's boot sector.
 * @param Cluster            Target data cluster index (>= 2).
 * @return true on success, false on error.
 */
bool FATClearCluster(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                     uint32_t Cluster);

#endif /* FAT_CLUSTER_H */
