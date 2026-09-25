#include "vbr.h"

#include <stddef.h>
#include <stdint.h>

/* Accepts only a real FAT32 geometry:
   - 512-byte sectors (the block layer is 512-byte only)
   - power-of-two sectors-per-cluster in 1..128
   - a reserved area, sane FAT count, nonzero FAT size
   - nonzero total sectors and a valid root cluster
   - no integer overflow when expanding geometry to byte/entry counts */
static bool FAT32IsPowerOfTwo(uint8_t Value)
{
    return Value != 0 && (Value & (Value - 1U)) == 0;
}

bool FAT32IsValid(const FAT32BootSector *Boot)
{
    uint32_t TotalSectors;
    uint64_t ResumeBytes;
    uint64_t DataSectors;
    uint64_t ClusterBitmap; /* bits required to index clusters */

    if (Boot == NULL)
    {
        return false;
    }

    if (Boot->BytesPerSector != 512U)
    {
        return false;
    }

    if (Boot->SectorsPerCluster == 0 ||
        !FAT32IsPowerOfTwo(Boot->SectorsPerCluster) ||
        Boot->SectorsPerCluster > 128U)
    {
        return false;
    }

    /* FAT32 uses reserved >= 1; a zero reserved area means no BPB. */
    if (Boot->ReservedSectorCount == 0)
    {
        return false;
    }

    if (Boot->NumberOfFATs == 0 || Boot->NumberOfFATs > 16U)
    {
        return false;
    }

    if (Boot->FATSize32 == 0)
    {
        return false;
    }

    TotalSectors = Boot->TotalSectors32 ? Boot->TotalSectors32
                                        : Boot->TotalSectors16;
    if (TotalSectors == 0)
    {
        return false;
    }

    /* FAT12/16 signposts: FAT16 uses RootEntryCount and FATSize16, FAT32
       keeps both zero.  Reject formats that cannot be FAT32. */
    if (Boot->RootEntryCount != 0 || Boot->FATSize16 != 0)
    {
        return false;
    }

    /* Root directory must live in an allocated first cluster. */
    if (Boot->RootCluster == 0 || Boot->RootCluster == 1)
    {
        return false;
    }

    /* No overflow of cluster size or of the FAT-entry bitmap coverage. */
    if ((uint64_t)Boot->SectorsPerCluster * 512ULL > 0x40000000ULL)
    {
        return false;
    }

    /* The two FATs must fit inside the volume before the data area. */
    ResumeBytes = (uint64_t)Boot->ReservedSectorCount +
                  (uint64_t)Boot->NumberOfFATs * Boot->FATSize32;
    if (ResumeBytes >= TotalSectors)
    {
        return false;
    }

    /* Data area must be able to hold at least cluster 2 (already above
       the reserved+FAT region). */
    DataSectors = (uint64_t)TotalSectors - ResumeBytes;
    if (DataSectors < (uint64_t)Boot->SectorsPerCluster)
    {
        return false;
    }

    /* Each cluster index needs a 32-bit FAT entry; make sure a single
       declared FAT copy could hold entries for every data cluster and stay
       below the reserved FAT-32 cluster-count upper bound. */
    ClusterBitmap = (((DataSectors / Boot->SectorsPerCluster) + 2ULL) * 4ULL +
                     511ULL) / 512ULL;
    if ((uint64_t)Boot->FATSize32 < ClusterBitmap)
    {
        return false;
    }

    return true;
}

uint32_t FAT32GetFirstFATSector(const FAT32BootSector *Boot,
                                 uint32_t PartitionStartLBA)
{
    uint64_t LBA = (uint64_t)PartitionStartLBA +
                   (uint64_t)Boot->ReservedSectorCount;
    if (LBA > UINT32_MAX)
    {
        return 0;
    }
    return (uint32_t)LBA;
}

uint32_t FAT32GetFirstDataSector(const FAT32BootSector *Boot,
                                  uint32_t PartitionStartLBA)
{
    uint64_t LBA = (uint64_t)FAT32GetFirstFATSector(Boot, PartitionStartLBA) +
                   (uint64_t)Boot->NumberOfFATs * Boot->FATSize32;
    if (LBA > UINT32_MAX)
    {
        return 0;
    }
    return (uint32_t)LBA;
}

uint32_t FAT32ClusterToLBA(const FAT32BootSector *Boot,
                           uint32_t PartitionStartLBA,
                           uint32_t Cluster)
{
    uint64_t LBA;

    if (Cluster < 2U)
    {
        return 0;
    }
    LBA = (uint64_t)FAT32GetFirstDataSector(Boot, PartitionStartLBA) +
          ((uint64_t)Cluster - 2U) * Boot->SectorsPerCluster;
    if (LBA > UINT32_MAX)
    {
        return 0;
    }
    return (uint32_t)LBA;
}