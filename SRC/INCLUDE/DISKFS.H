#ifndef NULLOS_DISKFS_H
#define NULLOS_DISKFS_H

#include <stdbool.h>
#include <stdint.h>

/* Maximum length of a filesystem path (including the terminating NUL). */
#define DISKFS_PATH_MAX 256U

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

/* Looks up a file in the root directory without changing the working
   directory. Returns false when the entry is not present. */
bool DiskFsFindInRoot(const char *Name, FAT32DirectoryEntry *Entry);

/* Navigates to an absolute path like "/BOOT/LIMINE". */
bool DiskFsChdirPath(const char *Path);

/* Creates an empty file (no-op when the file already exists). */
bool DiskFsTouch(const char *Name);

/* Creates or replaces a regular file with the given contents. */
bool DiskFsWrite(const char *Name, const void *Data, uint32_t Size);

/* Path-aware variant of DiskFsWrite: navigates DirPath (absolute or
   relative) and writes Name inside it, restoring the working directory
   afterwards. Creates or replaces the file. */
bool DiskFsWriteInPath(const char *DirPath, const char *Name,
                       const void *Data, uint32_t Size);

/* Path-aware file size lookup (absolute or relative DirPath). */
bool DiskFsFileSizeInPath(const char *DirPath, const char *Name,
                          uint32_t *Size);

/* Returns the size in bytes of a regular file in the working directory.
   Returns false when the file does not exist. */
bool DiskFsFileSize(const char *Name, uint32_t *Size);

/* Reads a regular file into Buffer (truncated to MaxSize). */
bool DiskFsRead(const char *Name, void *Buffer, uint32_t MaxSize,
                uint32_t *BytesRead);
bool DiskFsReadFromRoot(const char *Name, void *Buffer, uint32_t MaxSize,
                        uint32_t *BytesRead);

/* Deletes a file or directory entry from the working directory. */
bool DiskFsRemove(const char *Name);

/* Creates a subdirectory in the working directory. */
bool DiskFsMkdir(const char *Name);

/* Fills Out with the short names of every visible entry of the working
   directory, one per line.  Returns the number of entries listed. */
uint32_t DiskFsListNames(char *Out, uint32_t OutSize);

/* Returns true when Name (a name in the working directory, or an
   absolute "/path/") refers to a directory. */
bool DiskFsIsDirectory(const char *Name);

/* mkdir -p on the disk: creates the path components under the working
   directory, entering each one. For absolute paths from "/" pass a path
   starting with '/'. Returns false on failure. */
bool DiskFsMkdirAll(const char *Path);

/* Looks up Name in the absolute directory DirPath (e.g. "/bin") without
   changing the working directory. */
bool DiskFsFindInPath(const char *DirPath, const char *Name,
                      FAT32DirectoryEntry *Out);

/* Reads Name from the absolute directory DirPath without changing the
   working directory. */
bool DiskFsReadInPath(const char *DirPath, const char *Name, void *Buffer,
                      uint32_t MaxSize, uint32_t *BytesRead);

#endif