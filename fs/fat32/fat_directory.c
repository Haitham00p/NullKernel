#include "fs/fat32/fat_directory.h"

#include <stddef.h>

bool FATReadDirectory(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                      uint32_t DirCluster, FAT32DirectoryEntry *EntryBuffer,
                      uint32_t MaxEntries, uint32_t *OutEntriesRead)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)DirCluster;
    (void)EntryBuffer;
    (void)MaxEntries;
    (void)OutEntriesRead;

    /*
     * TODO: Implement directory entry reading logic:
     * 1. Validate parameters (Boot != NULL, EntryBuffer != NULL, OutEntriesRead != NULL).
     * 2. Traverse directory cluster chain starting at DirCluster using FATReadEntry().
     * 3. Read cluster sectors using FATReadCluster() or IDEReadSector().
     * 4. Iterate over 32-byte chunks in each sector.
     * 5. Stop if entry first byte is 0x00 (end of entries).
     * 6. Skip deleted entries (0xE5) and LFN entries (Attribute 0x0F) if short-name only.
     * 7. Copy valid entries into EntryBuffer up to MaxEntries.
     * 8. Store count in *OutEntriesRead and return true.
     */

    return false;
}

bool FATFindEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                  uint32_t DirCluster, const char *Name,
                  FAT32DirectoryEntry *OutEntry)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)DirCluster;
    (void)Name;
    (void)OutEntry;

    /*
     * TODO: Implement directory entry lookup logic:
     * 1. Validate parameters (Boot != NULL, Name != NULL, OutEntry != NULL).
     * 2. Convert/encode input Name into FAT 8.3 short filename representation.
     * 3. Traverse cluster chain for DirCluster.
     * 4. For each directory sector, scan 32-byte entry structures.
     * 5. Compare 11-byte short name against encoded target name.
     * 6. On match, copy directory entry to OutEntry and return true.
     * 7. Return false if end of directory (0x00) reached or not found.
     */

    return false;
}

bool FATCreateEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t DirCluster, const char *Name, uint8_t Attributes,
                    uint32_t FirstCluster, uint32_t FileSize,
                    FAT32DirectoryEntry *OutEntry)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)DirCluster;
    (void)Name;
    (void)Attributes;
    (void)FirstCluster;
    (void)FileSize;
    (void)OutEntry;

    /*
     * TODO: Implement directory entry creation logic:
     * 1. Validate input parameters and name format.
     * 2. Ensure Name does not already exist in DirCluster using FATFindEntry().
     * 3. Search DirCluster chain for an available 32-byte slot (marked 0x00 or 0xE5).
     * 4. If directory is full, allocate new cluster via FATFindFreeCluster() and append to cluster chain.
     * 5. Populate FAT32DirectoryEntry: Name (8.3 format), Attributes, FirstCluster (High/Low words), FileSize, timestamps.
     * 6. Write sector back to disk via IDEWriteSector().
     * 7. If OutEntry != NULL, copy created entry to OutEntry.
     */

    return false;
}

bool FATDeleteEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t DirCluster, const char *Name)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)DirCluster;
    (void)Name;

    /*
     * TODO: Implement directory entry deletion logic:
     * 1. Validate parameters (Boot != NULL, Name != NULL).
     * 2. Scan DirCluster chain to locate entry matching Name.
     * 3. Mark the first byte of entry Name as 0xE5 (FAT32_DIRECTORY_ENTRY_DELETED).
     * 4. Write modified sector back to disk via IDEWriteSector().
     * 5. Return true on success.
     */

    return false;
}

bool FATUpdateEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                    uint32_t DirCluster, const char *Name,
                    const FAT32DirectoryEntry *NewEntry)
{
    (void)Boot;
    (void)PartitionStartLBA;
    (void)DirCluster;
    (void)Name;
    (void)NewEntry;

    /*
     * TODO: Implement directory entry update logic:
     * 1. Validate parameters (Boot != NULL, Name != NULL, NewEntry != NULL).
     * 2. Scan DirCluster chain to find existing entry matching Name.
     * 3. Copy contents of NewEntry over existing entry at sector offset.
     * 4. Write updated sector back to disk via IDEWriteSector().
     * 5. Return true on success.
     */

    return false;
}
