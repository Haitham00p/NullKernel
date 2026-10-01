/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "include/diskfs.h"

#include <stddef.h>

#include "drivers/storage/block/block.h"
#include "fs/fat32/fat/fat.h"
#include "fs/fat32/file/file.h"
#include "fs/partition/mbr/mbr.h"
#include "lib/string/string.h"
#include "kernel/terminal/terminal.h"
#include "kernel/debug/debug.h"

#define DISKFS_SECTOR_SIZE 512U

static FAT32BootSector g_Boot;
static FAT32Directory g_Cwd;
static uint32_t g_PartitionStartLBA;
static char g_Path[DISKFS_PATH_MAX];
static bool g_Mounted;

static bool DiskFsWalk(const char *Path, bool CreateMissing);

static bool DiskFsReadChain(const FAT32Directory *Dir,
                            const FAT32DirectoryEntry *Entry,
                            void *Buffer, uint32_t MaxSize,
                            uint32_t *BytesRead);

static bool DiskFsReadSector(uint32_t LBA, void *Buffer)
{
    return BlockRead((uint64_t)LBA, Buffer);
}

static bool DiskFsWriteSector(uint32_t LBA, const void *Buffer)
{
    return BlockWrite((uint64_t)LBA, Buffer);
}

bool DiskFsMount(void)
{
    uint8_t Sector[DISKFS_SECTOR_SIZE];
    const MBR *Table;
    const MBRPartitionEntry *Part;

    if (g_Mounted)
    {
        return true;
    }

    if (!BlockIsReady() || !DiskFsReadSector(0, Sector))
    {
        return false;
    }

    Table = (const MBR *)Sector;
    if (!MBRIsValid(Table) || MBRGetPartitionCount(Table) == 0)
    {
        return false;
    }

    Part = MBRGetPartition(Table, 0);
    if (Part == NULL ||
        !(Part->PartitionType == 0x0BU || Part->PartitionType == 0x0CU))
    {
        return false;
    }

    g_PartitionStartLBA = Part->StartLBA;
    if (!DiskFsReadSector(g_PartitionStartLBA, Sector))
    {
        return false;
    }

    memcpy(&g_Boot, Sector, sizeof(FAT32BootSector));
    if (!FAT32IsValid(&g_Boot) || g_Boot.BytesPerSector != DISKFS_SECTOR_SIZE)
    {
        return false;
    }

    if (!FAT32DirectoryOpenRoot(&g_Cwd, &g_Boot, g_PartitionStartLBA))
    {
        return false;
    }

    g_Path[0] = '/';
    g_Path[1] = '\0';
    g_Mounted = true;
    return true;
}

bool DiskFsIsMounted(void)
{
    return g_Mounted;
}

bool DiskFsCdRoot(void)
{
    if (!g_Mounted)
    {
        return false;
    }
    g_Cwd.FirstCluster = g_Boot.RootCluster;
    g_Path[0] = '/';
    g_Path[1] = '\0';
    return true;
}

static void DiskFsPathPush(const char *Name)
{
    uint32_t Length = strlen(g_Path);

    if (Length > 1U)
    {
        g_Path[Length++] = '/';
    }
    if (Length + strlen(Name) < DISKFS_PATH_MAX)
    {
        strcpy(&g_Path[Length], Name);
    }
}

static void DiskFsPathPop(void)
{
    uint32_t Length = strlen(g_Path);
    int32_t Index;

    if (Length <= 1U)
    {
        return;
    }
    Index = (int32_t)Length - 2;
    while (Index >= 0 && g_Path[Index] != '/')
    {
        Index--;
    }
    if (Index < 0)
    {
        g_Path[1] = '\0';
    }
    else if (Index == 0)
    {
        g_Path[1] = '\0';
    }
    else
    {
        g_Path[Index] = '\0';
    }
}

bool DiskFsCd(const char *Name)
{
    FAT32DirectoryEntry Entry;
    FAT32Directory Next;

    if (!g_Mounted || Name == NULL || Name[0] == '\0' ||
        strcmp(Name, ".") == 0)
    {
        return g_Mounted;
    }
    if (strcmp(Name, "/") == 0)
    {
        return DiskFsCdRoot();
    }
    if (strcmp(Name, "..") == 0)
    {
        uint32_t Cluster = g_Cwd.FirstCluster;
        uint64_t Steps = 0;

        if (Cluster == g_Boot.RootCluster)
        {
            return true;
        }
        while (Cluster >= 2U && Steps < 65536U)
        {
            uint64_t ClusterLBA =
                (uint64_t)FAT32GetFirstDataSector(g_Cwd.Boot,
                                                  g_Cwd.PartitionStartLBA) +
                (uint64_t)(Cluster - 2U) * g_Cwd.Boot->SectorsPerCluster;

            Steps++;
            if (ClusterLBA > UINT32_MAX)
            {
                return false;
            }
            for (uint32_t SectorIndex = 0;
                 SectorIndex < g_Cwd.Boot->SectorsPerCluster; SectorIndex++)
            {
                uint8_t Sector[DISKFS_SECTOR_SIZE];

                if (!DiskFsReadSector((uint32_t)ClusterLBA + SectorIndex,
                                      Sector))
                {
                    return false;
                }
                for (uint32_t Index = 0; Index < DISKFS_SECTOR_SIZE / 32U;
                     Index++)
                {
                    FAT32DirectoryEntry *DotDot =
                        (FAT32DirectoryEntry *)&Sector[Index * 32U];

                    if (DotDot->Name[0] == FAT32_DIRECTORY_ENTRY_FREE)
                    {
                        return false;
                    }
                    if (DotDot->Name[0] == '.' && DotDot->Name[1] == '.')
                    {
                        uint32_t ParentCluster =
                            FAT32DirectoryEntryFirstCluster(DotDot);
                        DiskFsPathPop();
                        return FAT32DirectoryOpen(&g_Cwd, g_Cwd.Boot,
                                                  g_Cwd.PartitionStartLBA,
                                                  ParentCluster);
                    }
                }
            }

            uint32_t NextCluster;
            if (!FATReadEntry(g_Cwd.Boot, g_Cwd.PartitionStartLBA, Cluster,
                              &NextCluster) ||
                FATIsEndOfChain(NextCluster))
            {
                break;
            }
            Cluster = NextCluster;
        }
        return false;
    }

    if (!FAT32DirectoryFind(&g_Cwd, Name, &Entry) ||
        (Entry.Attributes & FAT32_ATTRIBUTE_DIRECTORY) == 0 ||
        !FAT32DirectoryOpen(&Next, g_Cwd.Boot, g_Cwd.PartitionStartLBA,
                            FAT32DirectoryEntryFirstCluster(&Entry)))
    {
        return false;
    }

    DiskFsPathPush(Name);
    g_Cwd = Next;
    return true;
}

void DiskFsPwd(char *Out, uint32_t OutSize)
{
    if (Out == NULL || OutSize == 0)
    {
        return;
    }
    if (!g_Mounted)
    {
        Out[0] = '/';
        Out[1] = '\0';
        return;
    }
    if (strlen(g_Path) + 1U > OutSize)
    {
        strncpy(Out, g_Path, OutSize - 1U);
        Out[OutSize - 1U] = '\0';
        return;
    }
    strcpy(Out, g_Path);
}

const FAT32Directory *DiskFsCwd(void)
{
    return g_Mounted ? &g_Cwd : NULL;
}

const FAT32BootSector *DiskFsBoot(void)
{
    return g_Mounted ? &g_Boot : NULL;
}

static bool DiskFsRamReader(void *Context, uint32_t Offset, void *Buffer,
                            uint32_t ByteCount)
{
    const uint8_t *Data = (const uint8_t *)Context;

    if (Data == NULL || Buffer == NULL)
    {
        return false;
    }
    memcpy(Buffer, Data + Offset, ByteCount);
    return true;
}

bool DiskFsTouch(const char *Name)
{
    FAT32DirectoryEntry Existing;

    if (!g_Mounted || Name == NULL || Name[0] == '\0')
    {
        return false;
    }
    if (FAT32DirectoryFind(&g_Cwd, Name, &Existing))
    {
        return true;
    }
    return FAT32DirectoryCreateEntry(&g_Cwd, Name, FAT32_ATTRIBUTE_ARCHIVE,
                                     0, 0, NULL);
}

bool DiskFsWrite(const char *Name, const void *Data, uint32_t Size)
{
    FAT32DirectoryEntry Existing;

    if (!g_Mounted || Name == NULL || Name[0] == '\0')
    {
        return false;
    }
    if (FAT32DirectoryFind(&g_Cwd, Name, &Existing))
    {
        DiskFsRemove(Name);
    }
    return FAT32FileWriteFromReader(&g_Cwd, Name, Size, DiskFsRamReader,
                                    (void *)Data);
}
bool DiskFsFileSizeInPath(const char *DirPath, const char *Name,
                          uint32_t *Size)
{
    FAT32DirectoryEntry Entry;

    if (!g_Mounted || DirPath == NULL || Name == NULL || Size == NULL)
    {
        return false;
    }
    if (!DiskFsFindInPath(DirPath, Name, &Entry))
    {
        return false;
    }
    *Size = Entry.FileSize;
    return true;
}

bool DiskFsWriteInPath(const char *DirPath, const char *Name,
                       const void *Data, uint32_t Size)
{
    FAT32Directory Saved;
    char SavedPath[DISKFS_PATH_MAX];
    bool Ok;

    if (DirPath == NULL || Name == NULL || Data == NULL)
    {
        return false;
    }
    memcpy(&Saved, &g_Cwd, sizeof(Saved));
    strncpy(SavedPath, g_Path, sizeof(SavedPath) - 1U);
    SavedPath[sizeof(SavedPath) - 1U] = '\0';

    if (!DiskFsWalk(DirPath, false))
    {
        return false;
    }
    Ok = DiskFsWrite(Name, Data, Size);
    memcpy(&g_Cwd, &Saved, sizeof(g_Cwd));
    strncpy(g_Path, SavedPath, sizeof(g_Path) - 1U);
    g_Path[sizeof(g_Path) - 1U] = '\0';
    return Ok;
}

bool DiskFsRead(const char *Name, void *Buffer, uint32_t MaxSize,
                uint32_t *BytesRead)
{
    FAT32DirectoryEntry Entry;

    if (!g_Mounted || Name == NULL || Buffer == NULL || BytesRead == NULL ||
        !FAT32DirectoryFind(&g_Cwd, Name, &Entry))
    {
        return false;
    }
    return DiskFsReadChain(&g_Cwd, &Entry, Buffer, MaxSize, BytesRead);
}

bool DiskFsFileSize(const char *Name, uint32_t *Size)
{
    FAT32DirectoryEntry Entry;

    if (!g_Mounted || Name == NULL || Size == NULL ||
        !FAT32DirectoryFind(&g_Cwd, Name, &Entry))
    {
        return false;
    }
    *Size = Entry.FileSize;
    return true;
}

static bool DiskFsReadChain(const FAT32Directory *Dir,
                            const FAT32DirectoryEntry *Entry,
                            void *Buffer, uint32_t MaxSize,
                            uint32_t *BytesRead)
{
    uint8_t Sector[DISKFS_SECTOR_SIZE];
    uint32_t Cluster;
    uint32_t Offset = 0;
    uint32_t Size;

    if (Dir == NULL || Entry == NULL || Buffer == NULL ||
        BytesRead == NULL)
    {
        return false;
    }

    Size = Entry->FileSize;
    if (Size > MaxSize)
    {
        Size = MaxSize;
    }
    *BytesRead = Size;
    if (Size == 0)
    {
        return true;
    }

    Cluster = FAT32DirectoryEntryFirstCluster(Entry);
    while (Offset < Size)
    {
        uint64_t ClusterLBA =
            (uint64_t)FAT32GetFirstDataSector(Dir->Boot,
                                              Dir->PartitionStartLBA) +
            (uint64_t)(Cluster - 2U) * Dir->Boot->SectorsPerCluster;

        if (Cluster < 2U || ClusterLBA > UINT32_MAX)
        {
            return false;
        }
        for (uint32_t SectorIndex = 0;
             SectorIndex < Dir->Boot->SectorsPerCluster && Offset < Size;
             SectorIndex++)
        {
            uint32_t Bytes = Size - Offset;

            if (Bytes > DISKFS_SECTOR_SIZE)
            {
                Bytes = DISKFS_SECTOR_SIZE;
            }
            if (!DiskFsReadSector((uint32_t)ClusterLBA + SectorIndex, Sector))
            {
                return false;
            }
            memcpy((uint8_t *)Buffer + Offset, Sector, Bytes);
            Offset += Bytes;
        }
        if (Offset < Size &&
            (!FATReadEntry(Dir->Boot, Dir->PartitionStartLBA, Cluster,
                           &Cluster) ||
             FATIsEndOfChain(Cluster) || Cluster < 2U))
        {
            return false;
        }
    }
    return true;
}

static bool DiskFsEntryMatches(const FAT32DirectoryEntry *Entry,
                               const char *Name)
{
    uint8_t Encoded[FAT32_SHORT_NAME_LENGTH];
    char ShortName[FAT32_SHORT_NAME_MAX + 1U];

    if (!FAT32DirectoryEncodeShortName(Name, Encoded))
    {
        return false;
    }
    FAT32DirectoryDecodeShortName(Entry, ShortName);
    (void)Encoded;
    return strcasecmp(ShortName, Name) == 0;
}

bool DiskFsRemove(const char *Name)
{
    uint8_t Sector[DISKFS_SECTOR_SIZE];
    uint32_t Cluster;
    uint64_t Steps = 0;

    if (!g_Mounted || Name == NULL || Name[0] == '\0')
    {
        return false;
    }

    Cluster = g_Cwd.FirstCluster;
    while (Cluster >= 2U && Steps < 65536U)
    {
        uint64_t ClusterLBA =
            (uint64_t)FAT32GetFirstDataSector(g_Cwd.Boot,
                                              g_Cwd.PartitionStartLBA) +
            (uint64_t)(Cluster - 2U) * g_Cwd.Boot->SectorsPerCluster;

        Steps++;
        if (ClusterLBA > UINT32_MAX)
        {
            return false;
        }
        for (uint32_t SectorIndex = 0;
             SectorIndex < g_Cwd.Boot->SectorsPerCluster; SectorIndex++)
        {
            if (!DiskFsReadSector((uint32_t)ClusterLBA + SectorIndex, Sector))
            {
                return false;
            }
            for (uint32_t Index = 0; Index < DISKFS_SECTOR_SIZE / 32U;
                 Index++)
            {
                FAT32DirectoryEntry *Entry =
                    (FAT32DirectoryEntry *)&Sector[Index * 32U];

                if (Entry->Name[0] == FAT32_DIRECTORY_ENTRY_FREE)
                {
                    return false;
                }
                if (Entry->Name[0] == FAT32_DIRECTORY_ENTRY_DELETED ||
                    (Entry->Attributes & FAT32_ATTRIBUTE_VOLUME_ID) != 0 ||
                    (Entry->Attributes & FAT32_ATTRIBUTE_LONG_NAME) != 0)
                {
                    continue;
                }
                if (DiskFsEntryMatches(Entry, Name))
                {
                    Entry->Name[0] = FAT32_DIRECTORY_ENTRY_DELETED;
                    return DiskFsWriteSector((uint32_t)ClusterLBA + SectorIndex,
                                             Sector);
                }
            }
        }

        uint32_t NextCluster;
        if (!FATReadEntry(g_Cwd.Boot, g_Cwd.PartitionStartLBA, Cluster,
                          &NextCluster) ||
            FATIsEndOfChain(NextCluster))
        {
            break;
        }
        Cluster = NextCluster;
    }
    return false;
}

/* ------------------------------------------------------------------ */
/* Duplicate-folder lock                                               */
/*                                                                    */
/* Refuses to create a directory whose name is a numbered / case       */
/* variant of an already-existing directory in the current folder      */
/* (e.g. "HOME2" when "HOME" already exists).  This prevents the       */
/* shell from ever spawning duplicate folders like HOME0..HOME5.       */
/* ------------------------------------------------------------------ */

static void DiskFsStripTrailingDigits(const char *Name, char *Out, uint32_t OutMax)
{
    uint32_t n = (uint32_t)strlen(Name);
    while (n > 0U && Name[n - 1U] >= '0' && Name[n - 1U] <= '9')
    {
        n--;
    }
    uint32_t i = 0;
    while (i < n && i + 1U < OutMax)
    {
        char c = Name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        Out[i] = c;
        i++;
    }
    Out[i] = '\0';
}

typedef struct
{
    char  Base[64];
    bool  Found;
} DiskFsDupCtx;

static bool DiskFsDupVisitor(const FAT32DirectoryEntry *Entry,
                             const char *ShortName, void *Context)
{
    DiskFsDupCtx *ctx = (DiskFsDupCtx *)Context;
    char existing[64];
    (void)Entry;
    DiskFsStripTrailingDigits(ShortName, existing, sizeof(existing));
    if (existing[0] != '\0' && strcmp(existing, ctx->Base) == 0)
    {
        ctx->Found = true;
        return false; /* stop walking */
    }
    return true;
}

bool DiskFsMkdir(const char *Name)
{
    FAT32Directory Child;
    char newbase[64];

    if (!g_Mounted || Name == NULL || Name[0] == '\0')
    {
        return false;
    }

    /* Lock: deny directories that duplicate (by base, case- and digit-
       insensitively) an existing directory in the current folder. */
    DiskFsStripTrailingDigits(Name, newbase, sizeof(newbase));
    if (newbase[0] != '\0')
    {
        DiskFsDupCtx ctx;
        ctx.Found = false;
        memcpy(ctx.Base, newbase, strlen(newbase) + 1U);
        FAT32DirectoryForEach(&g_Cwd, DiskFsDupVisitor, &ctx);
        if (ctx.Found)
        {
            TerminalPrintLine32("mkdir: denied: duplicate folder '",
                                0x00FFA500U);
            TerminalWrite32(Name, 0x00FFA500U);
            TerminalPrintLine32("' (a folder with that base already exists)",
                                0x00FFA500U);
            return false;
        }
    }

    return FAT32DirectoryCreateDirectory(&g_Cwd, Name, &Child);
}

bool DiskFsFindInRoot(const char *Name, FAT32DirectoryEntry *Entry)
{
    FAT32Directory Root;

    if (!g_Mounted || Name == NULL || Entry == NULL)
    {
        return false;
    }
    if (!FAT32DirectoryOpen(&Root, &g_Boot, g_PartitionStartLBA,
                            g_Boot.RootCluster))
    {
        return false;
    }
    return FAT32DirectoryFind(&Root, Name, Entry);
}

bool DiskFsReadFromRoot(const char *Name, void *Buffer, uint32_t MaxSize,
                        uint32_t *BytesRead)
{
    FAT32Directory Root;
    FAT32DirectoryEntry Entry;

    if (!g_Mounted || Name == NULL || Buffer == NULL || BytesRead == NULL)
    {
        return false;
    }
    if (!FAT32DirectoryOpen(&Root, &g_Boot, g_PartitionStartLBA,
                            g_Boot.RootCluster))
    {
        return false;
    }
    if (!FAT32DirectoryFind(&Root, Name, &Entry))
    {
        return false;
    }
    return DiskFsReadChain(&Root, &Entry, Buffer, MaxSize, BytesRead);
}

static bool DiskFsEntryExists(const char *Name, FAT32DirectoryEntry *Entry)
{
    return FAT32DirectoryFind(&g_Cwd, Name, Entry);
}

/* Enters a single path component.  "." stays put, ".." walks to the parent
   via DiskFsCd, and any other name is looked up (created when CreateMissing
   is set) and entered.  Returns false when the component cannot be entered. */
static bool DiskFsEnterComponent(const char *Component, bool CreateMissing)
{
    FAT32DirectoryEntry Entry;

    if (strcmp(Component, ".") == 0)
    {
        return true;
    }
    if (strcmp(Component, "..") == 0)
    {
        return DiskFsCd("..");
    }
    if (!DiskFsEntryExists(Component, &Entry))
    {
        if (!CreateMissing || !DiskFsMkdir(Component))
        {
            return false;
        }
    }
    else if ((Entry.Attributes & FAT32_ATTRIBUTE_DIRECTORY) == 0)
    {
        return false;
    }
    return DiskFsCd(Component);
}

/* Walks every '/'-separated component of Path, entering existing
   directories. When CreateMissing is true, missing components are created
   with DiskFsMkdir first. An absolute Path (leading '/') starts from the
   root. Trailing slashes and empty components are ignored. */
static bool DiskFsWalk(const char *Path, bool CreateMissing)
{
    char Copy[DISKFS_PATH_MAX];
    char Component[FAT32_SHORT_NAME_MAX + 1U];
    uint32_t Start = 0;
    uint32_t Index = 0;

    if (Path == NULL || Path[0] == '\0' || strlen(Path) >= DISKFS_PATH_MAX)
    {
        return false;
    }
    if (Path[0] == '/' && !DiskFsCdRoot())
    {
        return false;
    }

    strcpy(Copy, Path);
    while (Copy[Index] != '\0')
    {
        if (Copy[Index] == '/')
        {
            Copy[Index] = '\0';
            if (Index > Start)
            {
                if (strlen(&Copy[Start]) > FAT32_SHORT_NAME_MAX)
                {
                    return false;
                }
                strcpy(Component, &Copy[Start]);
                if (!DiskFsEnterComponent(Component, CreateMissing))
                {
                    return false;
                }
            }
            Start = Index + 1U;
        }
        Index++;
    }
    if (Index > Start)
    {
        if (strlen(&Copy[Start]) > FAT32_SHORT_NAME_MAX)
        {
            return false;
        }
        strcpy(Component, &Copy[Start]);
        if (!DiskFsEnterComponent(Component, CreateMissing))
        {
            return false;
        }
    }
    return true;
}

bool DiskFsChdirPath(const char *Path)
{
    if (!g_Mounted)
    {
        return false;
    }
    return DiskFsWalk(Path, false);
}

bool DiskFsMkdirAll(const char *Path)
{
    FAT32Directory Saved;
    char SavedPath[DISKFS_PATH_MAX];
    bool Result;

    if (!g_Mounted)
    {
        return false;
    }
    memcpy(&Saved, &g_Cwd, sizeof(Saved));
    strncpy(SavedPath, g_Path, sizeof(SavedPath) - 1U);
    SavedPath[sizeof(SavedPath) - 1U] = '\0';
    Result = DiskFsWalk(Path, true);
    memcpy(&g_Cwd, &Saved, sizeof(g_Cwd));
    strncpy(g_Path, SavedPath, sizeof(g_Path) - 1U);
    g_Path[sizeof(g_Path) - 1U] = '\0';
    return Result;
}

bool DiskFsFindInPath(const char *DirPath, const char *Name,
                      FAT32DirectoryEntry *Out)
{
    FAT32Directory Saved;
    char SavedPath[DISKFS_PATH_MAX];
    bool Found;

    if (!g_Mounted || DirPath == NULL || Name == NULL || Out == NULL)
    {
        return false;
    }

    Saved = g_Cwd;
    strcpy(SavedPath, g_Path);
    if (!DiskFsWalk(DirPath, false))
    {
        return false;
    }
    Found = FAT32DirectoryFind(&g_Cwd, Name, Out);
    g_Cwd = Saved;
    strcpy(g_Path, SavedPath);
    return Found;
}

bool DiskFsReadInPath(const char *DirPath, const char *Name, void *Buffer,
                      uint32_t MaxSize, uint32_t *BytesRead)
{
    FAT32Directory Saved;
    char SavedPath[DISKFS_PATH_MAX];
    FAT32DirectoryEntry Entry;
    bool Ok;

    if (!g_Mounted || DirPath == NULL || Name == NULL || Buffer == NULL ||
        BytesRead == NULL)
    {
        return false;
    }
    if (!DiskFsFindInPath(DirPath, Name, &Entry))
    {
        return false;
    }

    Saved = g_Cwd;
    strcpy(SavedPath, g_Path);
    if (!DiskFsWalk(DirPath, false))
    {
        return false;
    }
    Ok = DiskFsReadChain(&g_Cwd, &Entry, Buffer, MaxSize, BytesRead);
    g_Cwd = Saved;
    strcpy(g_Path, SavedPath);
    return Ok;
}

typedef struct
{
    uint32_t Used;
    uint32_t OutSize;
    uintptr_t Out;
    uint32_t Count;
} DiskFsListState;

static bool DiskFsListVisitor(const FAT32DirectoryEntry *Entry,
                              const char *ShortName, void *Context)
{
    DiskFsListState *State = (DiskFsListState *)Context;
    uint32_t Length = strlen(ShortName) + 1U;

    (void)Entry;
    if (State->Used + Length + 1U > State->OutSize)
    {
        return false;
    }
    strcpy((char *)State->Out + State->Used, ShortName);
    *(char *)(State->Out + State->Used + Length - 1U) = '\n';
    *(char *)(State->Out + State->Used + Length) = '\0';
    State->Used += Length;
    State->Count++;
    return true;
}

uint32_t DiskFsListNames(char *Out, uint32_t OutSize)
{
    DiskFsListState State;

    if (!g_Mounted || Out == NULL || OutSize == 0)
    {
        return 0;
    }
    State.Used = 0;
    State.OutSize = OutSize;
    State.Out = (uintptr_t)Out;
    State.Count = 0;
    FAT32DirectoryForEach(&g_Cwd, DiskFsListVisitor, &State);
    if (State.Used == 0)
    {
        Out[0] = '\0';
    }
    return State.Count;
}

bool DiskFsIsDirectory(const char *Name)
{
    FAT32DirectoryEntry Entry;
    char Copy[DISKFS_PATH_MAX];
    char *Slash;

    if (!g_Mounted || Name == NULL || Name[0] == '\0')
    {
        return false;
    }

    if (Name[0] == '/')
    {
        if (strlen(Name) >= DISKFS_PATH_MAX)
        {
            return false;
        }
        strcpy(Copy, Name);
        Slash = Copy + strlen(Copy) - 1U;
        while (Slash > Copy && *Slash == '/')
        {
            *Slash-- = '\0';
        }
        if (*Slash == '/')
        {
            return true;
        }
        Slash = Copy + 1U;
        while (*Slash != '\0' && *Slash != '/')
        {
            Slash++;
        }
        if (*Slash == '\0')
        {
            return DiskFsFindInRoot(Copy + 1U, &Entry) &&
                   (Entry.Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0;
        }
        *Slash = '\0';
        return DiskFsFindInPath(Copy, Slash + 1U, &Entry) &&
               (Entry.Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0;
    }

    return FAT32DirectoryFind(&g_Cwd, Name, &Entry) &&
           (Entry.Attributes & FAT32_ATTRIBUTE_DIRECTORY) != 0;
}
