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

#include "directory.h"

#include <stddef.h>

#include "drivers/storage/block/block.h"
#include "fs/fat32/fat/fat.h"
#include "lib/string/string.h"

#define FAT32_SECTOR_SIZE 512U
#define FAT32_ENTRIES_PER_SECTOR \
    (FAT32_SECTOR_SIZE / FAT32_DIRECTORY_ENTRY_SIZE)

typedef struct
{
    uint32_t Cluster;
    uint32_t SectorIndex;
    uint32_t EntryIndex;
    uint32_t LBA;
    bool IsEndMarker;
} FAT32DirectorySlot;

static uint8_t DirectoryLFNChecksum(const uint8_t Short[FAT32_SHORT_NAME_LENGTH]);
static bool DirectoryMakeUniqueAlias(const FAT32Directory *Directory,
                                     const char *Name,
                                     char Alias[FAT32_SHORT_NAME_MAX + 1U + 8U]);

static bool DirectoryIsUsable(const FAT32Directory *Directory)
{
    return Directory != 0 && FAT32IsValid(Directory->Boot) &&
           Directory->Boot->BytesPerSector == FAT32_SECTOR_SIZE &&
           Directory->FirstCluster >= 2U;
}

static uint64_t DirectoryClusterLimit(const FAT32BootSector *Boot)
{
    uint64_t TotalSectors = Boot->TotalSectors16 != 0 ?
                            Boot->TotalSectors16 : Boot->TotalSectors32;
    uint64_t FirstDataSector = (uint64_t)Boot->ReservedSectorCount +
                               (uint64_t)Boot->NumberOfFATs * Boot->FATSize32;

    if (FirstDataSector >= TotalSectors)
    {
        return 0;
    }

    return (TotalSectors - FirstDataSector) / Boot->SectorsPerCluster;
}

static bool DirectoryClusterSectorLBA(const FAT32Directory *Directory,
                                      uint32_t Cluster, uint32_t SectorIndex,
                                      uint32_t *LBA)
{
    uint64_t FirstDataSector;
    uint64_t Result;

    if (!DirectoryIsUsable(Directory) || LBA == 0 ||
        Cluster < 2U || SectorIndex >= Directory->Boot->SectorsPerCluster)
    {
        return false;
    }

    FirstDataSector = (uint64_t)Directory->PartitionStartLBA +
                      Directory->Boot->ReservedSectorCount +
                      (uint64_t)Directory->Boot->NumberOfFATs *
                      Directory->Boot->FATSize32;
    Result = FirstDataSector +
             (uint64_t)(Cluster - 2U) * Directory->Boot->SectorsPerCluster +
             SectorIndex;
    if (Result > UINT32_MAX)
    {
        return false;
    }

    *LBA = (uint32_t)Result;
    return true;
}

static bool DirectoryIsVisible(const FAT32DirectoryEntry *Entry)
{
    return Entry->Name[0] != FAT32_DIRECTORY_ENTRY_FREE &&
           Entry->Name[0] != FAT32_DIRECTORY_ENTRY_DELETED &&
           Entry->Attributes != FAT32_ATTRIBUTE_LONG_NAME &&
           (Entry->Attributes & FAT32_ATTRIBUTE_VOLUME_ID) == 0;
}

static bool DirectoryNamesEqual(const uint8_t *Left, const uint8_t *Right)
{
    for (uint32_t Index = 0; Index < FAT32_SHORT_NAME_LENGTH; Index++)
    {
        if (Left[Index] != Right[Index]) return false;
    }
    return true;
}

static bool DirectoryIsValidShortCharacter(char Character)
{
    if (Character >= 'A' && Character <= 'Z') return true;
    if (Character >= '0' && Character <= '9') return true;

    switch (Character)
    {
        case '$': case '%': case '\'': case '-': case '_': case '@':
        case '~': case '`': case '!': case '(': case ')': case '{':
        case '}': case '^': case '#': case '&':
            return true;
        default:
            return false;
    }
}

static char DirectoryToUpper(char Character)
{
    if (Character >= 'a' && Character <= 'z')
    {
        return (char)(Character - ('a' - 'A'));
    }
    return Character;
}

static bool DirectoryNextCluster(const FAT32Directory *Directory,
                                 uint32_t Cluster, uint32_t *NextCluster)
{
    if (!FATReadEntry(Directory->Boot, Directory->PartitionStartLBA,
                      Cluster, NextCluster) || FATIsBadCluster(*NextCluster))
    {
        return false;
    }
    return true;
}

bool FAT32DirectoryOpen(FAT32Directory *Directory,
                        const FAT32BootSector *Boot,
                        uint32_t PartitionStartLBA, uint32_t FirstCluster)
{
    if (Directory == 0 || !FAT32IsValid(Boot) ||
        Boot->BytesPerSector != FAT32_SECTOR_SIZE || FirstCluster < 2U)
    {
        return false;
    }

    Directory->Boot = Boot;
    Directory->PartitionStartLBA = PartitionStartLBA;
    Directory->FirstCluster = FirstCluster;
    return true;
}

bool FAT32DirectoryOpenRoot(FAT32Directory *Directory,
                            const FAT32BootSector *Boot,
                            uint32_t PartitionStartLBA)
{
    if (Boot == 0)
    {
        return false;
    }
    return FAT32DirectoryOpen(Directory, Boot, PartitionStartLBA,
                              Boot->RootCluster);
}

bool FAT32DirectoryEncodeShortName(const char *Name,
                                   uint8_t Encoded[FAT32_SHORT_NAME_LENGTH])
{
    uint32_t Position = 0;
    uint32_t BaseLength = 0;
    uint32_t ExtensionLength = 0;
    bool HasExtension = false;

    if (Name == 0 || Encoded == 0 || Name[0] == '\0')
    {
        return false;
    }

    memset(Encoded, ' ', FAT32_SHORT_NAME_LENGTH);
    while (Name[Position] != '\0')
    {
        char Character = Name[Position++];

        if (Character == '.')
        {
            if (HasExtension || BaseLength == 0)
            {
                return false;
            }
            HasExtension = true;
            continue;
        }

        Character = DirectoryToUpper(Character);
        if (!DirectoryIsValidShortCharacter(Character))
        {
            return false;
        }

        if (!HasExtension)
        {
            if (BaseLength == 8U) return false;
            Encoded[BaseLength++] = (uint8_t)Character;
        }
        else
        {
            if (ExtensionLength == 3U) return false;
            Encoded[8U + ExtensionLength++] = (uint8_t)Character;
        }
    }

    return BaseLength != 0 && (!HasExtension || ExtensionLength != 0);
}

void FAT32DirectoryDecodeShortName(const FAT32DirectoryEntry *Entry,
                                   char Output[FAT32_SHORT_NAME_MAX + 1U])
{
    uint32_t Position = 0;
    uint32_t Index;
    bool HasExtension = false;

    if (Entry == 0 || Output == 0)
    {
        return;
    }

    for (Index = 0; Index < 8U && Entry->Name[Index] != ' '; Index++)
    {
        Output[Position++] = (char)Entry->Name[Index];
    }
    for (Index = 8U; Index < FAT32_SHORT_NAME_LENGTH; Index++)
    {
        if (Entry->Name[Index] != ' ')
        {
            HasExtension = true;
            break;
        }
    }
    if (HasExtension)
    {
        Output[Position++] = '.';
        for (Index = 8U; Index < FAT32_SHORT_NAME_LENGTH &&
             Entry->Name[Index] != ' '; Index++)
        {
            Output[Position++] = (char)Entry->Name[Index];
        }
    }
    Output[Position] = '\0';
}

uint32_t FAT32DirectoryEntryFirstCluster(const FAT32DirectoryEntry *Entry)
{
    if (Entry == 0)
    {
        return 0;
    }
    return ((uint32_t)Entry->FirstClusterHigh << 16) | Entry->FirstClusterLow;
}

static void DirectoryCollectLFNField(const uint8_t *Raw, uint8_t Field,
                                     uint8_t Out[13])
{
    const uint32_t Offsets[3] = { 1, 14, 28 };
    const uint32_t Counts[3] = { 5, 6, 2 };
    uint32_t Pos = 0;

    for (uint32_t i = 0; i < Counts[Field]; i++)
    {
        uint8_t Lo = Raw[Offsets[Field] + 2U * i];
        uint8_t Hi = Raw[Offsets[Field] + 2U * i + 1U];

        if (Hi != 0x00U && Hi != 0xFFU)
        {
            Lo = 0x00;
        }
        Out[Pos++] = Lo;
    }
}

static void DirectoryDecodeLFNChars(const uint8_t *Raw, uint8_t Out[13])
{
    for (uint32_t Field = 0; Field < 3; Field++)
    {
        DirectoryCollectLFNField(Raw, Field, Out + (Field == 0U ? 0U :
                                                     (Field == 1U ? 5U : 11U)));
    }
}

bool FAT32DirectoryForEach(const FAT32Directory *Directory,
                           FAT32DirectoryVisitor Visitor, void *Context)
{
    uint8_t Sector[FAT32_SECTOR_SIZE];
    uint32_t Cluster;
    uint64_t Steps;
    uint64_t Limit;
    char LongName[255U + 1U];
    uint8_t LFNChunks[255U / 13U + 1U][13];
    bool LFNChunkUsed[255U / 13U + 1U];
    uint32_t LFNCount = 0;
    uint8_t LFNChecksum = 0;
    bool LFNPending = false;

    memset(LFNChunkUsed, 0, sizeof(LFNChunkUsed));

    if (!DirectoryIsUsable(Directory) || Visitor == 0)
    {
        return false;
    }

    Cluster = Directory->FirstCluster;
    Limit = DirectoryClusterLimit(Directory->Boot);
    for (Steps = 0; Steps < Limit; Steps++)
    {
        for (uint32_t SectorIndex = 0;
             SectorIndex < Directory->Boot->SectorsPerCluster; SectorIndex++)
        {
            uint32_t LBA;
            if (!DirectoryClusterSectorLBA(Directory, Cluster, SectorIndex, &LBA) ||
                !BlockRead(LBA, Sector))
            {
                return false;
            }
            for (uint32_t Index = 0; Index < FAT32_ENTRIES_PER_SECTOR; Index++)
            {
                FAT32DirectoryEntry *Entry = (FAT32DirectoryEntry *)
                    &Sector[Index * FAT32_DIRECTORY_ENTRY_SIZE];
                char Name[FAT32_SHORT_NAME_MAX + 1U];

                if (Entry->Name[0] == FAT32_DIRECTORY_ENTRY_FREE ||
                    Entry->Name[0] == FAT32_DIRECTORY_ENTRY_DELETED)
                {
                    LFNPending = false;
                    if (Entry->Name[0] == FAT32_DIRECTORY_ENTRY_FREE)
                    {
                        return true;
                    }
                    continue;
                }
                if (Entry->Attributes == FAT32_ATTRIBUTE_LONG_NAME)
                {
                    uint8_t Ordinal = Entry->Name[0] & 0x1FU;
                    uint32_t ChunkIndex;

                    if (Entry->Name[0] & 0x40U)
                    {
                        LFNCount = Ordinal;
                        LFNChecksum = Entry->CreationTimeTenths;
                        LFNPending = true;
                        memset(LFNChunkUsed, 0, sizeof(LFNChunkUsed));
                    }
                    if (!LFNPending)
                    {
                        continue;
                    }
                    if (LFNCount > 255U / 13U + 1U || Ordinal == 0U ||
                        Ordinal > LFNCount)
                    {
                        LFNPending = false;
                        continue;
                    }
                    ChunkIndex = Ordinal - 1U;
                    DirectoryDecodeLFNChars((const uint8_t *)Entry,
                                            LFNChunks[ChunkIndex]);
                    LFNChunkUsed[ChunkIndex] = true;
                    continue;
                }
                if (!DirectoryIsVisible(Entry))
                {
                    LFNPending = false;
                    continue;
                }

                if (LFNPending && LFNCount > 0U &&
                    DirectoryLFNChecksum(Entry->Name) == LFNChecksum)
                {
                    uint32_t Position = 0;
                    bool Complete = true;

                    LongName[0] = '\0';
                    for (uint32_t Chunk = 0; Chunk < LFNCount; Chunk++)
                    {
                        if (!LFNChunkUsed[Chunk])
                        {
                            Complete = false;
                            break;
                        }
                    }
                    if (Complete)
                    {
                        for (uint32_t Chunk = 0; Chunk < LFNCount; Chunk++)
                        {
                            for (uint32_t i = 0; i < 13U &&
                                 Position < 255U; i++)
                            {
                                uint8_t Char = LFNChunks[Chunk][i];

                                if (Char == 0x00U || Char == 0xFFU)
                                {
                                    i = 13U;
                                    break;
                                }
                                LongName[Position++] = (char)Char;
                            }
                        }
                        LongName[Position] = '\0';
                        if (!Visitor(Entry, LongName, Context))
                        {
                            return true;
                        }
                        LFNPending = false;
                        continue;
                    }
                }

                FAT32DirectoryDecodeShortName(Entry, Name);
                if (!Visitor(Entry, Name, Context))
                {
                    return true;
                }
                LFNPending = false;
            }
        }

        uint32_t NextCluster;
        if (!DirectoryNextCluster(Directory, Cluster, &NextCluster))
        {
            return false;
        }
        if (FATIsEndOfChain(NextCluster))
        {
            return true;
        }
        if (NextCluster < 2U)
        {
            return false;
        }
        Cluster = NextCluster;
    }
    return false;
}

typedef struct
{
    uint8_t Encoded[FAT32_SHORT_NAME_LENGTH];
    bool HaveEncoded;
    const char *Name;
    FAT32DirectoryEntry *Result;
    bool Found;
} FAT32FindContext;

static bool DirectoryFindVisitor(const FAT32DirectoryEntry *Entry,
                                 const char *Name, void *Context)
{
    FAT32FindContext *Find = (FAT32FindContext *)Context;
    if (Find->HaveEncoded && DirectoryNamesEqual(Entry->Name, Find->Encoded))
    {
        *Find->Result = *Entry;
        Find->Found = true;
        return false;
    }
    if (Find->Name != 0 && Name != 0 && strcasecmp(Find->Name, Name) == 0)
    {
        *Find->Result = *Entry;
        Find->Found = true;
        return false;
    }
    return true;
}

static bool DirectoryNameExists(const FAT32Directory *Directory,
                                const char *Name, bool *Exists)
{
    FAT32DirectoryEntry Entry;
    FAT32FindContext Context;

    if (Exists == 0)
    {
        return false;
    }
    memset(&Context, 0, sizeof(Context));
    Context.HaveEncoded = FAT32DirectoryEncodeShortName(Name, Context.Encoded);
    Context.Name = Name;
    Context.Result = &Entry;
    Context.Found = false;
    if (!FAT32DirectoryForEach(Directory, DirectoryFindVisitor, &Context))
    {
        return false;
    }
    *Exists = Context.Found;
    return true;
}

bool FAT32DirectoryFind(const FAT32Directory *Directory, const char *Name,
                        FAT32DirectoryEntry *Entry)
{
    FAT32FindContext Context;

    if (Entry == 0 || Name == 0 || Name[0] == '\0')
    {
        return false;
    }
    memset(&Context, 0, sizeof(Context));
    Context.HaveEncoded = FAT32DirectoryEncodeShortName(Name, Context.Encoded);
    Context.Name = Name;
    Context.Result = Entry;
    Context.Found = false;
    if (!FAT32DirectoryForEach(Directory, DirectoryFindVisitor, &Context))
    {
        return false;
    }
    return Context.Found;
}

static bool DirectoryFindFreeSlot(const FAT32Directory *Directory,
                                  FAT32DirectorySlot *Slot)
{
    uint8_t Sector[FAT32_SECTOR_SIZE];
    uint32_t Cluster = Directory->FirstCluster;
    uint64_t Limit = DirectoryClusterLimit(Directory->Boot);

    for (uint64_t Steps = 0; Steps < Limit; Steps++)
    {
        for (uint32_t SectorIndex = 0;
             SectorIndex < Directory->Boot->SectorsPerCluster; SectorIndex++)
        {
            uint32_t LBA;
            if (!DirectoryClusterSectorLBA(Directory, Cluster, SectorIndex, &LBA) ||
                !BlockRead(LBA, Sector)) return false;

            for (uint32_t Index = 0; Index < FAT32_ENTRIES_PER_SECTOR; Index++)
            {
                uint8_t FirstByte = Sector[Index * FAT32_DIRECTORY_ENTRY_SIZE];
                if (FirstByte == FAT32_DIRECTORY_ENTRY_DELETED ||
                    FirstByte == FAT32_DIRECTORY_ENTRY_FREE)
                {
                    Slot->Cluster = Cluster;
                    Slot->SectorIndex = SectorIndex;
                    Slot->EntryIndex = Index;
                    Slot->LBA = LBA;
                    Slot->IsEndMarker = FirstByte == FAT32_DIRECTORY_ENTRY_FREE;
                    return true;
                }
            }
        }

        uint32_t NextCluster;
        if (!DirectoryNextCluster(Directory, Cluster, &NextCluster)) return false;
        if (FATIsEndOfChain(NextCluster)) break;
        if (NextCluster < 2U) return false;
        Cluster = NextCluster;
    }
    return false;
}

static bool DirectoryClearCluster(const FAT32Directory *Directory,
                                  uint32_t Cluster)
{
    uint8_t Empty[FAT32_SECTOR_SIZE];
    memset(Empty, 0, sizeof(Empty));
    for (uint32_t Index = 0; Index < Directory->Boot->SectorsPerCluster; Index++)
    {
        uint32_t LBA;
        if (!DirectoryClusterSectorLBA(Directory, Cluster, Index, &LBA) ||
            !BlockWrite(LBA, Empty)) return false;
    }
    return true;
}

static bool DirectoryAppendCluster(FAT32Directory *Directory,
                                   uint32_t *NewCluster)
{
    uint32_t Cluster = Directory->FirstCluster;
    uint64_t Limit = DirectoryClusterLimit(Directory->Boot);

    for (uint64_t Steps = 0; Steps < Limit; Steps++)
    {
        uint32_t Next;
        if (!DirectoryNextCluster(Directory, Cluster, &Next)) return false;
        if (FATIsEndOfChain(Next))
        {
            uint32_t FreeCluster = FATFindFreeCluster(Directory->Boot,
                                                       Directory->PartitionStartLBA);
            if (FreeCluster == 0 || !DirectoryClearCluster(Directory, FreeCluster) ||
                !FATWriteEntry(Directory->Boot, Directory->PartitionStartLBA,
                               FreeCluster, FAT32_CLUSTER_EOC_LAST) ||
                !FATWriteEntry(Directory->Boot, Directory->PartitionStartLBA,
                               Cluster, FreeCluster)) return false;
            *NewCluster = FreeCluster;
            return true;
        }
        if (Next < 2U) return false;
        Cluster = Next;
    }
    return false;
}

static bool DirectoryGetWritableSlot(FAT32Directory *Directory,
                                     FAT32DirectorySlot *Slot)
{
    if (DirectoryFindFreeSlot(Directory, Slot)) return true;

    if (!DirectoryAppendCluster(Directory, &Slot->Cluster) ||
        !DirectoryClusterSectorLBA(Directory, Slot->Cluster, 0, &Slot->LBA))
    {
        return false;
    }
    Slot->SectorIndex = 0;
    Slot->EntryIndex = 0;
    Slot->IsEndMarker = true;
    return true;
}

bool FAT32DirectoryCreateEntry(FAT32Directory *Directory, const char *Name,
                               uint8_t Attributes, uint32_t FirstCluster,
                               uint32_t FileSize, FAT32DirectoryEntry *Entry)
{
    FAT32DirectoryEntry NewEntry;
    FAT32DirectorySlot Slot;
    uint8_t Sector[FAT32_SECTOR_SIZE];
    bool Exists;
    char Alias[FAT32_SHORT_NAME_MAX + 1U + 8U];

    if (!DirectoryIsUsable(Directory) || Name == 0 || Name[0] == '\0' ||
        Attributes == FAT32_ATTRIBUTE_LONG_NAME ||
        (Attributes & FAT32_ATTRIBUTE_VOLUME_ID) != 0)
    {
        return false;
    }

    /* Name too long for 8.3? Route through the LFN writer with an alias. */
    if (!FAT32DirectoryEncodeShortName(Name, NewEntry.Name))
    {
        if (!DirectoryMakeUniqueAlias(Directory, Name, Alias))
        {
            return false;
        }
        return FAT32DirectoryCreateLongFileEntry(Directory, Name, Alias,
                                                 Attributes, FirstCluster,
                                                 FileSize, Entry);
    }

    if (!DirectoryNameExists(Directory, Name, &Exists) || Exists ||
        !DirectoryGetWritableSlot(Directory, &Slot) ||
        !BlockRead(Slot.LBA, Sector))
    {
        return false;
    }

    memset(&NewEntry, 0, sizeof(NewEntry));
    FAT32DirectoryEncodeShortName(Name, NewEntry.Name);
    NewEntry.Attributes = Attributes;
    NewEntry.FirstClusterHigh = (uint16_t)(FirstCluster >> 16);
    NewEntry.FirstClusterLow = (uint16_t)FirstCluster;
    NewEntry.FileSize = FileSize;
    memcpy(&Sector[Slot.EntryIndex * FAT32_DIRECTORY_ENTRY_SIZE], &NewEntry,
           sizeof(NewEntry));

    /* A reused end marker must still be followed by an end marker. */
    if (Slot.IsEndMarker && Slot.EntryIndex + 1U < FAT32_ENTRIES_PER_SECTOR)
    {
        memset(&Sector[(Slot.EntryIndex + 1U) * FAT32_DIRECTORY_ENTRY_SIZE], 0,
               FAT32_DIRECTORY_ENTRY_SIZE);
    }
    if (!BlockWrite(Slot.LBA, Sector)) return false;

    if (Entry != 0) *Entry = NewEntry;
    return true;
}

static void DirectoryEncodeShortNameLiteral(const char *ShortName,
                                            uint8_t Encoded[FAT32_SHORT_NAME_LENGTH])
{
    uint32_t Position = 0;

    memset(Encoded, ' ', FAT32_SHORT_NAME_LENGTH);
    for (uint32_t Index = 0; ShortName[Index] != '\0' && Index < 12U; Index++)
    {
        char Character = ShortName[Index];
        if (Character == '.')
        {
            Position = 8;
            continue;
        }
        if (Position < 8U)
        {
            Encoded[Position++] = (uint8_t)DirectoryToUpper(Character);
        }
        else if (Position < 11U)
        {
            Encoded[Position++] = (uint8_t)DirectoryToUpper(Character);
        }
        else
        {
            break;
        }
    }
}

static uint8_t DirectoryLFNChecksum(const uint8_t Short[FAT32_SHORT_NAME_LENGTH])
{
    uint8_t Sum = 0;

    for (uint8_t Index = 0; Index < FAT32_SHORT_NAME_LENGTH; Index++)
    {
        Sum = (uint8_t)(((Sum & 0x01U) != 0 ? 0x80U : 0x00U) + (Sum >> 1) +
                        Short[Index]);
    }
    return Sum;
}

/* Number of 13-character LFN slots needed for Name. */
static uint32_t DirectoryLFNCount(const char *Name)
{
    uint32_t Length = 0;
    uint32_t Count;

    while (Name != 0 && Name[Length] != '\0' && Length < 255U)
    {
        Length++;
    }
    Count = (Length + 12U) / 13U;
    return Count > 0 ? Count : 1;
}

/* Builds a unique 8.3 alias (e.g. "LIMINE~1.SYS") for a name too long to
   encode directly. Tries suffixes ~1..~99 until the short name does not
   already exist in the directory. */
static bool DirectoryMakeUniqueAlias(const FAT32Directory *Directory,
                                     const char *Name,
                                     char Alias[FAT32_SHORT_NAME_MAX + 1U + 8U])
{
    char Base[9];
    char Extension[4];
    uint32_t BaseLength = 0;
    uint32_t ExtensionLength = 0;
    uint32_t ExtensionStart = 0;
    uint32_t Index;
    uint32_t Suffix;

    if (Directory == 0 || Name == 0 || Alias == 0)
    {
        return false;
    }

    for (Index = 0; Name[Index] != '\0'; Index++)
    {
        if (Name[Index] == '.')
        {
            ExtensionStart = Index + 1U;
        }
    }

    for (Index = 0; Name[Index] != '\0' && BaseLength < 8U; Index++)
    {
        char Character;

        if (ExtensionStart != 0U && Index >= ExtensionStart)
        {
            break;
        }
        Character = DirectoryToUpper(Name[Index]);
        if (Character != '.' && DirectoryIsValidShortCharacter(Character))
        {
            Base[BaseLength++] = Character;
        }
    }
    if (ExtensionStart != 0U)
    {
        for (Index = ExtensionStart; Name[Index] != '\0' &&
             ExtensionLength < 3U; Index++)
        {
            char Character = DirectoryToUpper(Name[Index]);

            if (DirectoryIsValidShortCharacter(Character))
            {
                Extension[ExtensionLength++] = Character;
            }
        }
    }
    if (BaseLength == 0)
    {
        return false;
    }

    for (Suffix = 1; Suffix <= 99U; Suffix++)
    {
        uint32_t Remaining = 8U - 1U - (Suffix >= 10U ? 2U : 1U);
        uint32_t Used = BaseLength < Remaining ? BaseLength : Remaining;
        uint32_t Position = 0;
        FAT32DirectoryEntry Existing;

        for (Index = 0; Index < Used; Index++)
        {
            Alias[Position++] = Base[Index];
        }
        Alias[Position++] = '~';
        if (Suffix >= 10U)
        {
            Alias[Position++] = (char)('0' + (Suffix / 10U));
        }
        Alias[Position++] = (char)('0' + (Suffix % 10U));
        if (ExtensionLength != 0)
        {
            Alias[Position++] = '.';
            for (Index = 0; Index < ExtensionLength; Index++)
            {
                Alias[Position++] = Extension[Index];
            }
        }
        Alias[Position] = '\0';

        if (!FAT32DirectoryFind(Directory, Alias, &Existing))
        {
            return true;
        }
    }
    return false;
}

static bool DirectoryFindFreeRun(const FAT32Directory *Directory,
                                 uint32_t Slots, FAT32DirectorySlot *RunStart)
{
    uint8_t Sector[FAT32_SECTOR_SIZE];
    uint32_t Cluster = Directory->FirstCluster;
    uint64_t Limit = DirectoryClusterLimit(Directory->Boot);
    uint32_t CurrentRun = 0;
    uint32_t RunCluster = 0;
    uint32_t RunSectorIndex = 0;
    uint32_t RunEntryIndex = 0;
    uint32_t RunLBA = 0;

    for (uint64_t Steps = 0; Steps < Limit; Steps++)
    {
        for (uint32_t SectorIndex = 0;
             SectorIndex < Directory->Boot->SectorsPerCluster; SectorIndex++)
        {
            uint32_t LBA;
            if (!DirectoryClusterSectorLBA(Directory, Cluster, SectorIndex, &LBA) ||
                !BlockRead(LBA, Sector)) return false;

            for (uint32_t Index = 0; Index < FAT32_ENTRIES_PER_SECTOR; Index++)
            {
                uint8_t FirstByte = Sector[Index * FAT32_DIRECTORY_ENTRY_SIZE];
                if (FirstByte == FAT32_DIRECTORY_ENTRY_DELETED ||
                    FirstByte == FAT32_DIRECTORY_ENTRY_FREE)
                {
                    if (CurrentRun == 0U)
                    {
                        RunCluster = Cluster;
                        RunSectorIndex = SectorIndex;
                        RunEntryIndex = Index;
                        RunLBA = LBA;
                    }
                    CurrentRun++;
                    if (CurrentRun >= Slots)
                    {
                        RunStart->Cluster = RunCluster;
                        RunStart->SectorIndex = RunSectorIndex;
                        RunStart->EntryIndex = RunEntryIndex;
                        RunStart->LBA = RunLBA;
                        RunStart->IsEndMarker =
                            Sector[RunEntryIndex * FAT32_DIRECTORY_ENTRY_SIZE] ==
                            FAT32_DIRECTORY_ENTRY_FREE;
                        return true;
                    }
                }
                else
                {
                    CurrentRun = 0;
                }
            }
        }

        uint32_t NextCluster;
        if (!DirectoryNextCluster(Directory, Cluster, &NextCluster)) return false;
        if (FATIsEndOfChain(NextCluster)) break;
        if (NextCluster < 2U) return false;
        Cluster = NextCluster;
    }

    /* No contiguous run in existing clusters: extend the directory chain and
       use the first Slots entries of the fresh cluster. */
    {
        uint32_t NewCluster;
        if (!DirectoryAppendCluster(Directory, &NewCluster)) return false;
        RunStart->Cluster = NewCluster;
        RunStart->SectorIndex = 0;
        RunStart->EntryIndex = 0;
        if (!DirectoryClusterSectorLBA(Directory, NewCluster, 0, &RunStart->LBA))
        {
            return false;
        }
        RunStart->IsEndMarker = true;
    }
    return true;
}

static bool DirectorySlotAdvance(const FAT32Directory *Directory,
                                 FAT32DirectorySlot *Slot)
{
    if (Slot == 0)
    {
        return false;
    }
    if (Slot->EntryIndex + 1U < FAT32_ENTRIES_PER_SECTOR)
    {
        Slot->EntryIndex++;
        return true;
    }
    if (Slot->SectorIndex + 1U >= Directory->Boot->SectorsPerCluster)
    {
        return false;
    }
    Slot->SectorIndex++;
    Slot->EntryIndex = 0;
    return DirectoryClusterSectorLBA(Directory, Slot->Cluster,
                                     Slot->SectorIndex, &Slot->LBA);
}

static void DirectoryWriteLFNNameField(uint8_t *Entry, const char *Name,
                                       uint32_t Offset)
{
    const uint32_t Offsets[3] = { 1, 14, 28 };
    const uint32_t Counts[3] = { 5, 6, 2 };
    uint32_t Pos = Offset;
    bool Terminated = false;

    for (uint32_t Field = 0; Field < 3; Field++)
    {
        for (uint32_t i = 0; i < Counts[Field]; i++, Pos++)
        {
            uint8_t Lo = 0xFF;
            uint8_t Hi = 0xFF;

            if (!Terminated)
            {
                char Char = Name[Pos];
                if (Char == '\0')
                {
                    /* First terminator after the last character is 0x0000;
                       the rest of the final slot is padded with 0xFFFF. */
                    Terminated = true;
                    Lo = 0x00;
                    Hi = 0x00;
                }
                else
                {
                    Lo = (uint8_t)Char;
                    Hi = 0x00;
                }
            }
            Entry[Offsets[Field] + 2U * i] = Lo;
            Entry[Offsets[Field] + 2U * i + 1U] = Hi;
        }
    }
}

static bool DirectoryWriteLFNEntry(const FAT32Directory *Directory,
                                   const FAT32DirectorySlot *Slot,
                                   const char *Name, uint32_t Offset,
                                   uint32_t Ordinal, uint8_t Checksum)
{
    uint8_t Sector[FAT32_SECTOR_SIZE];
    uint8_t *Entry;

    if (Slot == 0 || Name == 0 || !BlockRead(Slot->LBA, Sector)) return false;

    Entry = &Sector[Slot->EntryIndex * FAT32_DIRECTORY_ENTRY_SIZE];
    memset(Entry, 0, FAT32_DIRECTORY_ENTRY_SIZE);
    Entry[0] = Ordinal;
    Entry[11] = FAT32_ATTRIBUTE_LONG_NAME;
    Entry[12] = 0x00;
    Entry[13] = Checksum;

    DirectoryWriteLFNNameField(Entry, Name, Offset);

    return BlockWrite(Slot->LBA, Sector);
}

static bool DirectoryWriteSFNEntry(const FAT32Directory *Directory,
                                   const FAT32DirectorySlot *Slot,
                                   const uint8_t Short[FAT32_SHORT_NAME_LENGTH],
                                   uint8_t Attributes, uint32_t FirstCluster,
                                   uint32_t FileSize, FAT32DirectoryEntry *Out)
{
    uint8_t Sector[FAT32_SECTOR_SIZE];
    FAT32DirectoryEntry *Entry;
    FAT32DirectoryEntry NewEntry;

    if (Slot == 0 || !BlockRead(Slot->LBA, Sector)) return false;

    memset(&NewEntry, 0, sizeof(NewEntry));
    memcpy(NewEntry.Name, Short, FAT32_SHORT_NAME_LENGTH);
    NewEntry.Attributes = Attributes;
    NewEntry.FirstClusterHigh = (uint16_t)(FirstCluster >> 16);
    NewEntry.FirstClusterLow = (uint16_t)FirstCluster;
    NewEntry.FileSize = FileSize;

    Entry = (FAT32DirectoryEntry *)&Sector[Slot->EntryIndex *
                                           FAT32_DIRECTORY_ENTRY_SIZE];
    memcpy(Entry, &NewEntry, sizeof(NewEntry));

    /* A reused end marker must still be followed by an end marker. */
    if (Slot->IsEndMarker && Slot->EntryIndex + 1U < FAT32_ENTRIES_PER_SECTOR)
    {
        memset(&Sector[(Slot->EntryIndex + 1U) * FAT32_DIRECTORY_ENTRY_SIZE], 0,
               FAT32_DIRECTORY_ENTRY_SIZE);
    }
    if (!BlockWrite(Slot->LBA, Sector)) return false;

    if (Out != 0) *Out = NewEntry;
    return true;
}

bool FAT32DirectoryCreateLongFileEntry(FAT32Directory *Directory,
                                       const char *Name,
                                       const char *ShortName,
                                       uint8_t Attributes,
                                       uint32_t FirstCluster,
                                       uint32_t FileSize,
                                       FAT32DirectoryEntry *Entry)
{
    uint8_t Encoded[FAT32_SHORT_NAME_LENGTH];
    uint8_t Checksum;
    FAT32DirectorySlot Run;
    uint32_t LFNCount;
    bool Exists;

    if (!DirectoryIsUsable(Directory) || Name == 0 || ShortName == 0 ||
        Name[0] == '\0' || Attributes == FAT32_ATTRIBUTE_LONG_NAME ||
        (Attributes & FAT32_ATTRIBUTE_VOLUME_ID) != 0 ||
        !DirectoryNameExists(Directory, Name, &Exists) || Exists)
    {
        return false;
    }

    /* If the long name fits 8.3 on its own, a plain short entry suffices. */
    if (FAT32DirectoryEncodeShortName(Name, Encoded))
    {
        return FAT32DirectoryCreateEntry(Directory, Name, Attributes,
                                         FirstCluster, FileSize, Entry);
    }

    DirectoryEncodeShortNameLiteral(ShortName, Encoded);
    Checksum = DirectoryLFNChecksum(Encoded);
    LFNCount = DirectoryLFNCount(Name);

    if (!DirectoryFindFreeRun(Directory, LFNCount + 1U, &Run))
    {
        return false;
    }

    /* LFN entries appear right before the short entry. The entry farthest
       from the short entry holds the end of the name (ordinal 1) ... no:
       mtools stores the LAST chunk with ordinal 0x40|N first, then N-1 down
       to 1 with the FIRST chunk immediately before the short entry. */
    {
        FAT32DirectorySlot Slot = Run;
        for (uint32_t i = 0; i < LFNCount; i++)
        {
            uint32_t ChunkIndex = LFNCount - 1U - i;
            const char *Chunk = Name + ChunkIndex * 13U;
            uint32_t Ordinal = LFNCount - i;

            if (i == 0U) Ordinal = LFNCount | 0x40U;

            if (!DirectoryWriteLFNEntry(Directory, &Slot, Chunk, 0U, Ordinal,
                                        Checksum) ||
                (i + 1U < LFNCount &&
                 !DirectorySlotAdvance(Directory, &Slot)))
            {
                return false;
            }
        }
    }

    {
        FAT32DirectorySlot SFNSlot = Run;
        for (uint32_t i = 0; i < LFNCount; i++)
        {
            if (!DirectorySlotAdvance(Directory, &SFNSlot)) return false;
        }
        SFNSlot.IsEndMarker = false;
        return DirectoryWriteSFNEntry(Directory, &SFNSlot, Encoded, Attributes,
                                      FirstCluster, FileSize, Entry);
    }
}

bool FAT32DirectoryCreateDirectory(FAT32Directory *Parent, const char *Name,
                                   FAT32Directory *CreatedDirectory)
{
    uint32_t Cluster;
    uint32_t LBA;
    uint8_t Sector[FAT32_SECTOR_SIZE];
    FAT32DirectoryEntry *Dot;
    FAT32DirectoryEntry *DotDot;
    uint8_t Encoded[FAT32_SHORT_NAME_LENGTH];
    char Alias[FAT32_SHORT_NAME_MAX + 1U + 8U];
    bool Exists;

    if (!DirectoryIsUsable(Parent) || CreatedDirectory == 0 || Name == 0 ||
        Name[0] == '\0' ||
        !DirectoryNameExists(Parent, Name, &Exists) || Exists) return false;

    Cluster = FATFindFreeCluster(Parent->Boot, Parent->PartitionStartLBA);
    if (Cluster == 0 || !FATWriteEntry(Parent->Boot, Parent->PartitionStartLBA,
                                       Cluster, FAT32_CLUSTER_EOC_LAST) ||
        !DirectoryClearCluster(Parent, Cluster) ||
        !DirectoryClusterSectorLBA(Parent, Cluster, 0, &LBA) ||
        !BlockRead(LBA, Sector)) return false;

    Dot = (FAT32DirectoryEntry *)&Sector[0];
    DotDot = (FAT32DirectoryEntry *)&Sector[FAT32_DIRECTORY_ENTRY_SIZE];
    memset(Dot, 0, FAT32_DIRECTORY_ENTRY_SIZE * 2U);
    memset(Dot->Name, ' ', FAT32_SHORT_NAME_LENGTH);
    memset(DotDot->Name, ' ', FAT32_SHORT_NAME_LENGTH);
    Dot->Name[0] = '.';
    DotDot->Name[0] = '.';
    DotDot->Name[1] = '.';
    Dot->Attributes = FAT32_ATTRIBUTE_DIRECTORY;
    DotDot->Attributes = FAT32_ATTRIBUTE_DIRECTORY;
    Dot->FirstClusterHigh = (uint16_t)(Cluster >> 16);
    Dot->FirstClusterLow = (uint16_t)Cluster;

    /* FAT32: ".." points to 0 for directories directly below root. */
    if (Parent->FirstCluster == Parent->Boot->RootCluster)
    {
        DotDot->FirstClusterHigh = 0;
        DotDot->FirstClusterLow = 0;
    }
    else
    {
        DotDot->FirstClusterHigh = (uint16_t)(Parent->FirstCluster >> 16);
        DotDot->FirstClusterLow = (uint16_t)Parent->FirstCluster;
    }
    if (!BlockWrite(LBA, Sector)) return false;

    if (FAT32DirectoryEncodeShortName(Name, Encoded))
    {
        if (!FAT32DirectoryCreateEntry(Parent, Name, FAT32_ATTRIBUTE_DIRECTORY,
                                       Cluster, 0, 0))
        {
            return false;
        }
        return FAT32DirectoryOpen(CreatedDirectory, Parent->Boot,
                                  Parent->PartitionStartLBA, Cluster);
    }

    if (!DirectoryMakeUniqueAlias(Parent, Name, Alias))
    {
        return false;
    }
    if (!FAT32DirectoryCreateLongFileEntry(Parent, Name, Alias,
                                           FAT32_ATTRIBUTE_DIRECTORY,
                                           Cluster, 0, 0))
    {
        return false;
    }
    return FAT32DirectoryOpen(CreatedDirectory, Parent->Boot,
                              Parent->PartitionStartLBA, Cluster);
}
