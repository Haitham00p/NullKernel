#include "fs/fat32/fat_dir.h"

#include <stddef.h>

bool FATCreateDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                        uint32_t ParentCluster, const char *DirName)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)ParentCluster;
    (void)DirName;

    /*
     * TODO: Implement directory creation logic:
     * 1. Validate parameters (Boot != NULL, DirName != NULL).
     * 2. Ensure DirName does not exist in ParentCluster using FATFindEntry().
     * 3. Allocate a free cluster for new directory via FATFindFreeCluster().
     * 4. Zero-fill allocated cluster via FATClearCluster().
     * 5. Initialize "." entry pointing to allocated cluster and ".." entry pointing to ParentCluster.
     * 6. Add directory entry for DirName (attribute 0x10) in ParentCluster via FATCreateEntry().
     */

    return false;
}

bool FATDeleteDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                        uint32_t ParentCluster, const char *DirName)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)ParentCluster;
    (void)DirName;

    /*
     * TODO: Implement directory deletion logic:
     * 1. Locate directory entry for DirName in ParentCluster via FATFindEntry().
     * 2. Verify attribute contains FAT32_ATTRIBUTE_DIRECTORY (0x10).
     * 3. Retrieve target directory cluster index.
     * 4. Read target directory entries using FATReadDirectory() to verify directory is empty (only "." and "..").
     * 5. Free directory cluster chain in FAT table using FATWriteEntry(..., FAT32_CLUSTER_FREE).
     * 6. Delete directory entry from ParentCluster using FATDeleteEntry().
     */

    return false;
}

bool FATOpenDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                      uint32_t Cluster, FATDir *OutDir)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)Cluster;
    (void)OutDir;

    /*
     * TODO: Implement directory opening logic:
     * 1. Validate parameters (Boot != NULL, OutDir != NULL, Cluster >= 2).
     * 2. Initialize OutDir fields:
     *    - OutDir->Boot = Boot;
     *    - OutDir->PartitionStartLBA = PartitionStartLBA;
     *    - OutDir->FirstCluster = Cluster;
     *    - OutDir->CurrentCluster = Cluster;
     *    - OutDir->CurrentIndex = 0;
     * 3. Return true on success.
     */

    return false;
}
