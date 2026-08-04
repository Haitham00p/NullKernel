#include "include/diskfs.h"

#include <stddef.h>

#include "drivers/storage/ide/ide.h"
#include "fs/fat32/fat/fat.h"
#include "fs/fat32/file/file.h"
#include "fs/partition/mbr/mbr.h"
#include "lib/string/string.h"

#define DISKFS_SECTOR_SIZE 512U
#define DISKFS_PATH_MAX    256U

static FAT32BootSector g_Boot;
static FAT32Directory g_Cwd;
static uint32_t g_PartitionStartLBA;
static char g_Path[DISKFS_PATH_MAX];
static bool g_Mounted;

static bool DiskFsReadSector(uint32_t LBA, void *Buffer)
{
    return IDEReadSector(LBA, Buffer);
}

static bool DiskFsWriteSector(uint32_t LBA, const void *Buffer)
{
    return IDEWriteSector(LBA, Buffer);
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

    if (!IDESelectDisk() || !DiskFsReadSector(0, Sector))
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
                    if (DotDot->Name[0] == '.' &&
                        DotDot->Name[1] == '.')
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

bool DiskFsRead(const char *Name, void *Buffer, uint32_t MaxSize,
                uint32_t *BytesRead)
{
    FAT32DirectoryEntry Entry;
    uint8_t Sector[DISKFS_SECTOR_SIZE];
    uint32_t Cluster;
    uint32_t Offset = 0;
    uint32_t Size;

    if (!g_Mounted || Name == NULL || Buffer == NULL || BytesRead == NULL ||
        !FAT32DirectoryFind(&g_Cwd, Name, &Entry))
    {
        return false;
    }

    Size = Entry.FileSize;
    if (Size > MaxSize)
    {
        Size = MaxSize;
    }
    *BytesRead = Size;
    if (Size == 0)
    {
        return true;
    }

    Cluster = FAT32DirectoryEntryFirstCluster(&Entry);
    while (Offset < Size)
    {
        uint64_t ClusterLBA =
            (uint64_t)FAT32GetFirstDataSector(g_Cwd.Boot,
                                              g_Cwd.PartitionStartLBA) +
            (uint64_t)(Cluster - 2U) * g_Cwd.Boot->SectorsPerCluster;

        if (Cluster < 2U || ClusterLBA > UINT32_MAX)
        {
            return false;
        }
        for (uint32_t SectorIndex = 0;
             SectorIndex < g_Cwd.Boot->SectorsPerCluster && Offset < Size;
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
            (!FATReadEntry(g_Cwd.Boot, g_Cwd.PartitionStartLBA, Cluster,
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

bool DiskFsMkdir(const char *Name)
{
    FAT32Directory Child;

    if (!g_Mounted || Name == NULL || Name[0] == '\0')
    {
        return false;
    }
    return FAT32DirectoryCreateDirectory(&g_Cwd, Name, &Child);
}
