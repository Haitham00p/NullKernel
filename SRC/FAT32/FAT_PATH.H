#ifndef FAT_PATH_H
#define FAT_PATH_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/directory/directory.h"
#include "fs/fat32/vbr/vbr.h"

#define FAT_PATH_MAX 256U

/*
 * Searches for a file or directory at Path starting from root directory.
 * If found, populates OutEntry with the target directory entry and OutCluster with its starting cluster.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param Path               Absolute or relative path string (e.g. "/EFI/BOOT/BOOTX64.EFI").
 * @param OutEntry           Pointer to store final target directory entry.
 * @param OutCluster         Pointer to store final target starting cluster.
 * @return true if path resolved, false if any component not found or invalid.
 */
bool FATFindPath(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                 const char *Path, FAT32DirectoryEntry *OutEntry,
                 uint32_t *OutCluster);

/*
 * Splits a full path string into parent directory path and trailing filename/dirname.
 * Example: "/EFI/BOOT/BOOTX64.EFI" -> ParentPath: "/EFI/BOOT", Filename: "BOOTX64.EFI".
 *
 * @param Path               Full path string to split.
 * @param ParentPath         Buffer to receive parent path component (size >= FAT_PATH_MAX).
 * @param Filename           Buffer to receive filename component (size >= FAT32_SHORT_NAME_MAX + 1).
 * @return true on success, false on invalid path.
 */
bool FATSplitPath(const char *Path, char *ParentPath, char *Filename);

/*
 * Resolves Path to its target starting cluster.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param Path               Path string to resolve.
 * @param OutCluster         Pointer to receive starting cluster index.
 * @return true on success, false if resolution fails.
 */
bool FATResolvePath(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    const char *Path, uint32_t *OutCluster);

#endif /* FAT_PATH_H */
