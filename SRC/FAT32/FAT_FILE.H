#ifndef FAT_FILE_H
#define FAT_FILE_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/directory/directory.h"
#include "fs/fat32/vbr/vbr.h"

/* Mode flags for file opening and creation. */
typedef enum
{
    FAT_FILE_MODE_READ   = 0x01,
    FAT_FILE_MODE_WRITE  = 0x02,
    FAT_FILE_MODE_APPEND = 0x04,
    FAT_FILE_MODE_CREATE = 0x08
} FATFileMode;

/* High-level FAT32 file structure handle. */
typedef struct
{
    const FAT32BootSector *Boot;
    uint32_t PartitionStartLBA;
    uint32_t DirCluster;        /* Cluster of parent directory */
    uint32_t FirstCluster;      /* First data cluster of the file */
    uint32_t CurrentCluster;    /* Current seek position cluster */
    uint32_t FileSize;          /* File size in bytes */
    uint32_t Position;          /* Read/write byte offset */
    char Name[FAT32_SHORT_NAME_MAX + 1U]; /* Short filename (8.3 format + NUL) */
} FATFile;

/*
 * High-level creation of a file named Filename in directory DirCluster.
 * Allocates initial data cluster and adds directory entry.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of parent directory.
 * @param Filename           Name of file to create.
 * @param Attributes         File attribute flags (e.g. archive, read-only).
 * @return true on success, false on failure.
 */
bool FATCreateFile(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                   uint32_t DirCluster, const char *Filename, uint8_t Attributes);

/*
 * High-level deletion of file Filename from directory DirCluster.
 * Traverses file cluster chain, frees all clusters in FAT, and removes directory entry.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of parent directory.
 * @param Filename           Name of file to delete.
 * @return true on success, false on failure.
 */
bool FATDeleteFile(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                   uint32_t DirCluster, const char *Filename);

/*
 * High-level reading from a file.
 * Reads BytesToRead bytes starting from Offset into Buffer.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param FirstCluster       Starting cluster of the file.
 * @param FileSize           Total size of file in bytes.
 * @param Offset             Byte offset within file to begin reading.
 * @param Buffer             Output buffer to receive byte data.
 * @param BytesToRead        Number of bytes requested to read.
 * @param BytesRead          Pointer to receive actual count of bytes read.
 * @return true on success, false on error.
 */
bool FATReadFile(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                 uint32_t FirstCluster, uint32_t FileSize, uint32_t Offset,
                 void *Buffer, uint32_t BytesToRead, uint32_t *BytesRead);

/*
 * High-level writing to a file.
 * Writes BytesToWrite bytes from Buffer into file at Offset.
 * Extends cluster chain if writing past current EOF and updates directory entry.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of parent directory.
 * @param Filename           Name of file to write.
 * @param Offset             Byte offset within file to begin writing.
 * @param Buffer             Input buffer containing bytes to write.
 * @param BytesToWrite       Number of bytes to write.
 * @param BytesWritten       Pointer to receive actual count of bytes written.
 * @return true on success, false on error.
 */
bool FATWriteFile(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                  uint32_t DirCluster, const char *Filename, uint32_t Offset,
                  const void *Buffer, uint32_t BytesToWrite, uint32_t *BytesWritten);

/*
 * High-level appending to a file.
 * Writes BytesToWrite bytes from Buffer to the end of file Filename.
 * Automatically allocates new clusters as required and updates file size in directory.
 *
 * @param Boot               Pointer to the FAT32 boot sector.
 * @param PartitionStartLBA  LBA offset of the FAT32 partition.
 * @param DirCluster         Starting cluster of parent directory.
 * @param Filename           Name of file to append to.
 * @param Buffer             Input buffer containing bytes to append.
 * @param BytesToWrite       Number of bytes to append.
 * @param BytesWritten       Pointer to receive actual count of bytes appended.
 * @return true on success, false on error.
 */
bool FATAppendFile(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                   uint32_t DirCluster, const char *Filename,
                   const void *Buffer, uint32_t BytesToWrite, uint32_t *BytesWritten);

#endif /* FAT_FILE_H */
