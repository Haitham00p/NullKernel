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

#include "file.h"

#include <stddef.h>

#include "drivers/storage/block/block.h"
#include "fs/fat32/fat/fat.h"
#include "lib/string/string.h"

#define FAT32_FILE_SECTOR_SIZE 512U

static bool FAT32FileBuffersEqual(const uint8_t *Left, const uint8_t *Right,
                                  uint32_t Size)
{
    for (uint32_t Index = 0; Index < Size; Index++)
    {
        if (Left[Index] != Right[Index]) return false;
    }
    return true;
}

static bool FAT32FileClusterLBA(const FAT32Directory *Directory,
                                uint32_t Cluster, uint32_t *LBA)
{
    uint64_t Result;

    if (Directory == NULL || LBA == NULL || Cluster < 2U) return false;
    Result = (uint64_t)FAT32GetFirstDataSector(Directory->Boot,
                                               Directory->PartitionStartLBA) +
             (uint64_t)(Cluster - 2U) * Directory->Boot->SectorsPerCluster;
    if (Result > UINT32_MAX) return false;
    *LBA = (uint32_t)Result;
    return true;
}

bool FAT32FileWriteFromReader(FAT32Directory *Directory, const char *Name,
                              uint32_t Size, FAT32FileReadCallback Reader,
                              void *Context)
{
    uint8_t Sector[FAT32_FILE_SECTOR_SIZE];
    uint32_t PreviousCluster = 0U;
    uint32_t FirstCluster = 0U;
    uint32_t Offset = 0U;

    if (Directory == NULL || Name == NULL || Reader == NULL) return false;
    while (Offset < Size)
    {
        uint32_t Cluster = FATFindFreeCluster(Directory->Boot,
                                              Directory->PartitionStartLBA);
        uint32_t ClusterLBA;

        if (Cluster == 0U ||
            !FAT32FileClusterLBA(Directory, Cluster, &ClusterLBA) ||
            !FATWriteEntry(Directory->Boot, Directory->PartitionStartLBA,
                           Cluster, FAT32_CLUSTER_EOC_LAST) ||
            (PreviousCluster != 0U &&
             !FATWriteEntry(Directory->Boot, Directory->PartitionStartLBA,
                            PreviousCluster, Cluster)))
        {
            return false;
        }
        if (FirstCluster == 0U) FirstCluster = Cluster;

        for (uint32_t SectorIndex = 0;
             SectorIndex < Directory->Boot->SectorsPerCluster && Offset < Size;
             SectorIndex++)
        {
            uint32_t Bytes = Size - Offset;
            if (Bytes > FAT32_FILE_SECTOR_SIZE) Bytes = FAT32_FILE_SECTOR_SIZE;
            memset(Sector, 0, sizeof(Sector));
            if (!Reader(Context, Offset, Sector, Bytes) ||
                !BlockWrite(ClusterLBA + SectorIndex, Sector)) return false;
            Offset += Bytes;
        }
        PreviousCluster = Cluster;
    }

    return FAT32DirectoryCreateEntry(Directory, Name, FAT32_ATTRIBUTE_ARCHIVE,
                                     FirstCluster, Size, 0);
}

bool FAT32FileVerifyFromReader(const FAT32Directory *Directory, const char *Name,
                               uint32_t Size, FAT32FileReadCallback Reader,
                               void *Context)
{
    FAT32DirectoryEntry Entry;
    uint8_t DiskSector[FAT32_FILE_SECTOR_SIZE];
    uint8_t SourceSector[FAT32_FILE_SECTOR_SIZE];
    uint32_t Cluster;
    uint32_t Offset = 0U;

    if (Directory == NULL || Name == NULL || Reader == NULL ||
        !FAT32DirectoryFind(Directory, Name, &Entry) || Entry.FileSize != Size)
    {
        return false;
    }
    if (Size == 0U) return FAT32DirectoryEntryFirstCluster(&Entry) == 0U;

    Cluster = FAT32DirectoryEntryFirstCluster(&Entry);
    while (Offset < Size)
    {
        uint32_t ClusterLBA;
        if (!FAT32FileClusterLBA(Directory, Cluster, &ClusterLBA)) return false;
        for (uint32_t SectorIndex = 0;
             SectorIndex < Directory->Boot->SectorsPerCluster && Offset < Size;
             SectorIndex++)
        {
            uint32_t Bytes = Size - Offset;
            if (Bytes > FAT32_FILE_SECTOR_SIZE) Bytes = FAT32_FILE_SECTOR_SIZE;
            if (!BlockRead(ClusterLBA + SectorIndex, DiskSector) ||
                !Reader(Context, Offset, SourceSector, Bytes) ||
                !FAT32FileBuffersEqual(DiskSector, SourceSector, Bytes))
            {
                return false;
            }
            Offset += Bytes;
        }
        if (Offset < Size &&
            (!FATReadEntry(Directory->Boot, Directory->PartitionStartLBA,
                           Cluster, &Cluster) || FATIsEndOfChain(Cluster) ||
             Cluster < 2U))
        {
            return false;
        }
    }
    return true;
}
