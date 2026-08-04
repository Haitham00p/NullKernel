#ifndef NULLOS_DISKFS_H
#define NULLOS_DISKFS_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/directory/directory.h"
#include "fs/fat32/vbr/vbr.h"

/* Mounts the first FAT32 partition of the hard disk as the working volume.
   Safe to call repeatedly; returns true when already mounted. */
bool DiskFsMount(void);

bool DiskFsIsMounted(void);

/* Changes the working directory. "." stays, ".." goes up, "/" goes to root. */
bool DiskFsCd(const char *Name);

bool DiskFsCdRoot(void);

/* Writes the full working directory path into Out. */
void DiskFsPwd(char *Out, uint32_t OutSize);

/* Directory handle of the current working directory. */
const FAT32Directory *DiskFsCwd(void);

/* Boot sector of the mounted FAT32 volume. */
const FAT32BootSector *DiskFsBoot(void);

/* Creates an empty file (no-op when the file already exists). */
bool DiskFsTouch(const char *Name);

/* Creates or replaces a regular file with the given contents. */
bool DiskFsWrite(const char *Name, const void *Data, uint32_t Size);

/* Reads a regular file into Buffer (truncated to MaxSize). */
bool DiskFsRead(const char *Name, void *Buffer, uint32_t MaxSize,
                uint32_t *BytesRead);

/* Deletes a file or directory entry from the working directory. */
bool DiskFsRemove(const char *Name);

/* Creates a subdirectory in the working directory. */
bool DiskFsMkdir(const char *Name);

#endif
