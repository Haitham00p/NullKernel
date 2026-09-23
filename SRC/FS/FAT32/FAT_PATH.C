#include "fs/fat32/fat_path.h"

#include <stddef.h>

bool FATFindPath(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                 const char *Path, FAT32DirectoryEntry *OutEntry,
                 uint32_t *OutCluster)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)Path;
    (void)OutEntry;
    (void)OutCluster;

    /*
     * TODO: Implement path lookup and traversal logic:
     * 1. Validate parameters (Boot != NULL, Path != NULL).
     * 2. Start traversal at root cluster (Boot->RootCluster).
     * 3. Tokenize Path using '/' delimiter into sequential component tokens.
     * 4. For each token:
     *    a. Search current directory cluster via FATFindEntry().
     *    b. If token is intermediate path component, verify attribute is directory and retrieve its cluster.
     *    c. Advance to child directory cluster.
     * 5. For final path component:
     *    a. Retrieve FAT32DirectoryEntry and store in *OutEntry if OutEntry != NULL.
     *    b. Retrieve cluster index and store in *OutCluster if OutCluster != NULL.
     * 6. Return true if all path components resolved, false otherwise.
     */

    return false;
}

bool FATSplitPath(const char *Path, char *ParentPath, char *Filename)
{
    (void)Path;
    (void)ParentPath;
    (void)Filename;

    /*
     * TODO: Implement path string splitting:
     * 1. Validate parameters (Path != NULL, ParentPath != NULL, Filename != NULL).
     * 2. Scan Path to find final '/' delimiter.
     * 3. If no '/' delimiter exists:
     *    a. Copy "." or "/" to ParentPath.
     *    b. Copy Path to Filename.
     * 4. If '/' delimiter exists:
     *    a. Copy substring up to last '/' into ParentPath.
     *    b. Copy substring after last '/' into Filename.
     * 5. Return true on success.
     */

    return false;
}

bool FATResolvePath(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    const char *Path, uint32_t *OutCluster)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)Path;
    (void)OutCluster;

    /*
     * TODO: Implement path resolution to cluster index:
     * 1. Validate parameters (Boot != NULL, Path != NULL, OutCluster != NULL).
     * 2. Invoke FATFindPath(Boot, PartitionStartLBA, Path, &Entry, OutCluster).
     * 3. Return status of FATFindPath call.
     */

    return false;
}
