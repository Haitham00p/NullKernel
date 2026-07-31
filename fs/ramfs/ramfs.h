#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>
#include <stdbool.h>

#define RAMFS_MAX_FILES 128
#define RAMFS_MAX_FILE_LENGTH 32

typedef struct ramfs
{
    char Name[RAMFS_MAX_FILE_LENGTH];
    void *Data;
    uint64_t Size;
    bool Used;
} RamFile;

typedef struct
{
    RamFile Files[RAMFS_MAX_FILES];
    uint32_t FileCount;
} RamFs;

void RamFsInitialize(void);

RamFile *RamFsCreate(const char *Name);

RamFile *RamFsFind(const char *Name);  

RamFile *RamFsGetByIndex(uint32_t Index);

void RamFsWrite(RamFile *File, const void *Data, uint64_t Size);

void RamFsRead(RamFile *File);

void RamFsDelete(RamFile *File);

void RamFsListFiles(void);

RamFile *RamFsFind(const char *Name);

#endif
