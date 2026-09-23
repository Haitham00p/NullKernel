#ifndef FAT_DIR_H
#define FAT_DIR_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/directory/directory.h"
#include "fs/fat32/vbr/vbr.h"

/* Structure representing an active directory handle. */
typedef struct
{
    const FAT32BootSector *Boot;
    uint32_t PartitionStartLBA;
    uint32_t FirstCluster;      /* First cluster of this directory */
    uint32_t CurrentCluster;    /* Cluster currently being read */
    uint32_t CurrentIndex;      /* Current entry index within directory */
} FATDir;

/*
 * Creates a new directory named DirName inside ParentCluster directory.
 * Allocates a cluster, formats "." and ".." entries, and registers the entry in ParentCluster.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param ParentCluster      Cluster of the parent directory.
 * @param DirName            Name of the directory to create.
 * @return true on success, false on failure.
 */
bool FATCreateDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                        uint32_t ParentCluster, const char *DirName);

/*
 * Deletes an empty directory named DirName from ParentCluster directory.
 * Ensures target directory contains no entries other than "." and "..",
 * frees its cluster chain, and removes its entry from ParentCluster.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param ParentCluster      Cluster of the parent directory.
 * @param DirName            Name of directory to delete.
 * @return true on success, false on failure.
 */
bool FATDeleteDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                        uint32_t ParentCluster, const char *DirName);

/*
 * Opens a directory starting at Cluster and initializes the FATDir handle.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param Cluster            Target directory starting cluster.
 * @param OutDir             Pointer to FATDir structure to initialize.
 * @return true on success, false on failure.
 */
bool FATOpenDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                      uint32_t Cluster, FATDir *OutDir);

#endif /* FAT_DIR_H */
