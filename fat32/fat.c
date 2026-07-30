#include "fat.h"
#include "../DRIVERS/IDE/ide.h"

bool FATIsEndOfChain(uint32_t Cluster)
{
    return Cluster >= FAT32_CLUSTER_EOC;
}

bool FATIsFreeCluster(uint32_t Cluster)
{
    // ...
}

bool FATIsBadCluster(uint32_t Cluster)
{
    // ...
}

bool FATReadEntry(
    const FAT32BootSector *Boot,
    uint32_t PartitionStartLBA,
    uint32_t Cluster,
    uint32_t *NextCluster
)
{
    // Step 1:
    // حساب FAT Offset

    // Step 2:
    // حساب Sector

    // Step 3:
    // قراءة Sector

    // Step 4:
    // استخراج الـ Entry

    // Step 5:
    // إرجاع الـ Next Cluster
}

bool FATWriteEntry(...)
{
    // ...
}

uint32_t FATFindFreeCluster(...)
{
    // ...
}