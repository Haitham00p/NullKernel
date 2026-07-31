#ifndef FAT32_FILE_H
#define FAT32_FILE_H

#include <stdbool.h>
#include <stdint.h>

#include "fs/fat32/directory/directory.h"

/* Supplies exactly ByteCount bytes from a read-only source at ByteOffset. */
typedef bool (*FAT32FileReadCallback)(void *Context, uint32_t ByteOffset,
                                      void *Buffer, uint32_t ByteCount);

/* Streams a source into a newly created regular file in Directory. */
bool FAT32FileWriteFromReader(FAT32Directory *Directory, const char *Name,
                              uint32_t Size, FAT32FileReadCallback Reader,
                              void *Context);

/* Reads the file back and compares it with the same source callback. */
bool FAT32FileVerifyFromReader(const FAT32Directory *Directory, const char *Name,
                               uint32_t Size, FAT32FileReadCallback Reader,
                               void *Context);

#endif
