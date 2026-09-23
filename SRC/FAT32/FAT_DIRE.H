#ifndef FAT_DIRECTORY_H
#define FAT_DIRECTORY_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/directory/directory.h"
#include "fs/fat32/vbr/vbr.h"

/* Search and directory entry status codes. */
typedef enum
{
    FAT_DIR_STATUS_SUCCESS = 0,
    FAT_DIR_STATUS_NOT_FOUND,
    FAT_DIR_STATUS_ALREADY_EXISTS,
    FAT_DIR_STATUS_NO_SPACE,
    FAT_DIR_STATUS_INVALID_PARAM,
    FAT_DIR_STATUS_IO_ERROR
} FATDirEntryStatus;

/*
 * Reads directory entries from a directory starting at DirCluster.
 * Parses 32-byte entries across the cluster chain into EntryBuffer up to MaxEntries.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of the target directory.
 * @param EntryBuffer        Array of FAT32DirectoryEntry to populate.
 * @param MaxEntries         Maximum capacity of EntryBuffer.
 * @param OutEntriesRead     Pointer to store actual count of entries read.
 * @return true on success, false on failure.
 */
bool FATReadDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                      uint32_t DirCluster, FAT32DirectoryEntry *EntryBuffer,
                      uint32_t MaxEntries, uint32_t *OutEntriesRead);

/*
 * Searches for a directory entry matching Name within directory starting at DirCluster.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of the target directory.
 * @param Name               Name of the file or directory to find.
 * @param OutEntry           Pointer to store the retrieved directory entry.
 * @return true if entry found, false if not found or on error.
 */
bool FATFindEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                  uint32_t DirCluster, const char *Name,
                  FAT32DirectoryEntry *OutEntry);

/*
 * Creates a raw directory entry inside directory starting at DirCluster.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of target parent directory.
 * @param Name               Name for the new entry.
 * @param Attributes         FAT directory entry attribute flags.
 * @param FirstCluster       Initial data cluster assigned to entry.
 * @param FileSize           Initial size of file in bytes (0 for directories).
 * @param OutEntry           Optional pointer to receive created entry structure.
 * @return true on success, false on error.
 */
bool FATCreateEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t DirCluster, const char *Name, uint8_t Attributes,
                    uint32_t FirstCluster, uint32_t FileSize,
                    FAT32DirectoryEntry *OutEntry);

/*
 * Marks a directory entry matching Name in DirCluster as deleted (0xE5).
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of target directory.
 * @param Name               Name of the entry to delete.
 * @return true on success, false if entry not found or IO fails.
 */
bool FATDeleteEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t DirCluster, const char *Name);

/*
 * Updates an existing directory entry matching Name in DirCluster with NewEntry contents.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of target directory.
 * @param Name               Name of existing entry to update.
 * @param NewEntry           Pointer to updated entry data.
 * @return true on success, false on error.
 */
bool FATUpdateEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t DirCluster, const char *Name,
                    const FAT32DirectoryEntry *NewEntry);

#endif /* FAT_DIRECTORY_H */
