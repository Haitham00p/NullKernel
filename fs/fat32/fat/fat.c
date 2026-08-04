#include "fat.h"

#include "drivers/storage/ide/ide.h"

#define FAT32_IDE_SECTOR_SIZE       512U
#define FAT32_ENTRY_SIZE             4U
#define FAT32_ENTRIES_PER_SECTOR \
    (FAT32_IDE_SECTOR_SIZE / FAT32_ENTRY_SIZE)

/* This layer uses the current IDE driver's fixed 512-byte sector interface. */
static bool FATCanAccess(const FAT32BootSector *Boot, uint32_t Cluster)
{
    return FAT32IsValid(Boot) &&
           Boot->BytesPerSector == FAT32_IDE_SECTOR_SIZE &&
           Cluster >= 2U;
}

static uint32_t FATReadLittleEndian32(const uint8_t *Bytes)
{
    return (uint32_t)Bytes[0] |
           ((uint32_t)Bytes[1] << 8) |
           ((uint32_t)Bytes[2] << 16) |
           ((uint32_t)Bytes[3] << 24);
}

static void FATWriteLittleEndian32(uint8_t *Bytes, uint32_t Value)
{
    Bytes[0] = (uint8_t)Value;
    Bytes[1] = (uint8_t)(Value >> 8);
    Bytes[2] = (uint8_t)(Value >> 16);
    Bytes[3] = (uint8_t)(Value >> 24);
}

static bool FATGetEntryLocation(const FAT32BootSector *Boot,
                                uint32_t PartitionStartLBA,
                                uint32_t Cluster,
                                uint32_t *FatSector,
                                uint32_t *EntryOffset)
{
    uint64_t FatOffset;
    uint64_t Sector;

    if (!FATCanAccess(Boot, Cluster) || FatSector == 0 || EntryOffset == 0)
    {
        return false;
    }

    FatOffset = (uint64_t)Cluster * FAT32_ENTRY_SIZE;
    if (FatOffset >= (uint64_t)Boot->FATSize32 * FAT32_IDE_SECTOR_SIZE)
    {
        return false;
    }

    Sector = (uint64_t)PartitionStartLBA + Boot->ReservedSectorCount +
             (FatOffset / FAT32_IDE_SECTOR_SIZE);
    if (Sector > UINT32_MAX)
    {
        return false;
    }

    *FatSector = (uint32_t)Sector;
    *EntryOffset = (uint32_t)(FatOffset % FAT32_IDE_SECTOR_SIZE);
    return true;
}

bool FATIsEndOfChain(uint32_t Cluster)
{
    Cluster &= FAT32_CLUSTER_MASK;
    return Cluster >= FAT32_CLUSTER_EOC && Cluster <= FAT32_CLUSTER_EOC_LAST;
}

bool FATIsFreeCluster(uint32_t Cluster)
{
    return (Cluster & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_FREE;
}

bool FATIsBadCluster(uint32_t Cluster)
{
    return (Cluster & FAT32_CLUSTER_MASK) == FAT32_CLUSTER_BAD;
}

bool FATReadEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                  uint32_t Cluster, uint32_t *NextCluster)
{
    uint8_t Sector[FAT32_IDE_SECTOR_SIZE];
    uint32_t FatSector;
    uint32_t EntryOffset;

    if (NextCluster == 0 ||
        !FATGetEntryLocation(Boot, PartitionStartLBA, Cluster,
                             &FatSector, &EntryOffset) ||
        !IDEReadSector(FatSector, Sector))
    {
        return false;
    }

    *NextCluster = FATReadLittleEndian32(&Sector[EntryOffset]) &
                   FAT32_CLUSTER_MASK;
    return true;
}

bool FATWriteEntry(const FAT32BootSector *Boot, uint32_t PartitionStartLBA,
                   uint32_t Cluster, uint32_t Value)
{
    uint8_t Sector[FAT32_IDE_SECTOR_SIZE];
    uint32_t FirstFatSector;
    uint32_t EntryOffset;
    uint32_t FatIndex;

    if (!FATGetEntryLocation(Boot, PartitionStartLBA, Cluster,
                             &FirstFatSector, &EntryOffset))
    {
        return false;
    }

    Value &= FAT32_CLUSTER_MASK;

    /* Keep the high four reserved bits of this FAT32 entry intact. */
    for (FatIndex = 0; FatIndex < Boot->NumberOfFATs; FatIndex++)
    {
        uint64_t SectorLBA = (uint64_t)FirstFatSector +
                             (uint64_t)FatIndex * Boot->FATSize32;
        uint32_t Existing;

        if (SectorLBA > UINT32_MAX ||
            !IDEReadSector((uint32_t)SectorLBA, Sector))
        {
            return false;
        }

        Existing = FATReadLittleEndian32(&Sector[EntryOffset]);
        FATWriteLittleEndian32(&Sector[EntryOffset],
                               (Existing & ~FAT32_CLUSTER_MASK) | Value);

        if (!IDEWriteSector((uint32_t)SectorLBA, Sector))
        {
            return false;
        }
    }

    return true;
}

uint32_t FATFindFreeCluster(const FAT32BootSector *Boot,
                            uint32_t PartitionStartLBA)
{
    uint8_t Sector[FAT32_IDE_SECTOR_SIZE];
    uint64_t EntryCount;
    uint64_t FatSector;
    uint64_t ClusterNumber;
    uint64_t TotalSectors;
    uint64_t FirstDataSector;
    uint32_t Cluster;

    if (!FAT32IsValid(Boot) ||
        Boot->BytesPerSector != FAT32_IDE_SECTOR_SIZE)
    {
        return 0;
    }

    TotalSectors = Boot->TotalSectors16 != 0 ?
                   Boot->TotalSectors16 : Boot->TotalSectors32;
    FirstDataSector = (uint64_t)Boot->ReservedSectorCount +
                      (uint64_t)Boot->NumberOfFATs * Boot->FATSize32;
    if (FirstDataSector >= TotalSectors)
    {
        return 0;
    }

    /* Include entries 0 and 1, which are reserved by FAT32. */
    EntryCount = ((TotalSectors - FirstDataSector) /
                  Boot->SectorsPerCluster) + 2U;
    if (EntryCount > (uint64_t)Boot->FATSize32 * FAT32_ENTRIES_PER_SECTOR)
    {
        EntryCount = (uint64_t)Boot->FATSize32 * FAT32_ENTRIES_PER_SECTOR;
    }
    if (EntryCount > FAT32_CLUSTER_EOC)
    {
        EntryCount = FAT32_CLUSTER_EOC;
    }

    for (FatSector = 0; FatSector < Boot->FATSize32; FatSector++)
    {
        uint64_t SectorLBA = (uint64_t)PartitionStartLBA +
                             Boot->ReservedSectorCount + FatSector;

        if (SectorLBA > UINT32_MAX ||
            !IDEReadSector((uint32_t)SectorLBA, Sector))
        {
            return 0;
        }

        for (uint32_t Entry = 0; Entry < FAT32_ENTRIES_PER_SECTOR; Entry++)
        {
            ClusterNumber = FatSector * FAT32_ENTRIES_PER_SECTOR + Entry;
            if (ClusterNumber >= EntryCount)
            {
                return 0;
            }

            Cluster = (uint32_t)ClusterNumber;

            if (Cluster >= 2U && Cluster != Boot->RootCluster &&
                FATIsFreeCluster(
                    FATReadLittleEndian32(&Sector[Entry * FAT32_ENTRY_SIZE])))
            {
                return Cluster;
            }
        }
    }

    return 0;
}
