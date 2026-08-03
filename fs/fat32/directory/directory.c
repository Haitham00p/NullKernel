#include "directory.h"

#include "drivers/storage/ide/ide.h"
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

bool FAT32DirectoryForEach(const FAT32Directory *Directory,
                           FAT32DirectoryVisitor Visitor, void *Context)
{
    uint8_t Sector[FAT32_SECTOR_SIZE];
    uint32_t Cluster;
    uint64_t Steps;
    uint64_t Limit;

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
                !IDEReadSector(LBA, Sector))
            {
                return false;
            }
            for (uint32_t Index = 0; Index < FAT32_ENTRIES_PER_SECTOR; Index++)
            {
                FAT32DirectoryEntry *Entry = (FAT32DirectoryEntry *)
                    &Sector[Index * FAT32_DIRECTORY_ENTRY_SIZE];
                char Name[FAT32_SHORT_NAME_MAX + 1U];

                if (Entry->Name[0] == FAT32_DIRECTORY_ENTRY_FREE)
                {
                    return true;
                }
                if (!DirectoryIsVisible(Entry))
                {
                    continue;
                }
                FAT32DirectoryDecodeShortName(Entry, Name);
                if (!Visitor(Entry, Name, Context))
                {
                    return true;
                }
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
    FAT32DirectoryEntry *Result;
    bool Found;
} FAT32FindContext;

static bool DirectoryFindVisitor(const FAT32DirectoryEntry *Entry,
                                 const char *ShortName, void *Context)
{
    FAT32FindContext *Find = (FAT32FindContext *)Context;
    (void)ShortName;
    if (DirectoryNamesEqual(Entry->Name, Find->Encoded))
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

    if (Exists == 0 || !FAT32DirectoryEncodeShortName(Name, Context.Encoded))
    {
        return false;
    }
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

    if (Entry == 0 || !FAT32DirectoryEncodeShortName(Name, Context.Encoded))
    {
        return false;
    }
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
                !IDEReadSector(LBA, Sector)) return false;

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
            !IDEWriteSector(LBA, Empty)) return false;
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

    if (!DirectoryIsUsable(Directory) ||
        !FAT32DirectoryEncodeShortName(Name, NewEntry.Name) ||
        Attributes == FAT32_ATTRIBUTE_LONG_NAME ||
        (Attributes & FAT32_ATTRIBUTE_VOLUME_ID) != 0 ||
        !DirectoryNameExists(Directory, Name, &Exists) || Exists ||
        !DirectoryGetWritableSlot(Directory, &Slot) ||
        !IDEReadSector(Slot.LBA, Sector))
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
    if (!IDEWriteSector(Slot.LBA, Sector)) return false;

    if (Entry != 0) *Entry = NewEntry;
    return true;
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
    bool Exists;

    if (!DirectoryIsUsable(Parent) || CreatedDirectory == 0 ||
        !FAT32DirectoryEncodeShortName(Name, Encoded) ||
        !DirectoryNameExists(Parent, Name, &Exists) || Exists) return false;

    Cluster = FATFindFreeCluster(Parent->Boot, Parent->PartitionStartLBA);
    if (Cluster == 0 || !FATWriteEntry(Parent->Boot, Parent->PartitionStartLBA,
                                       Cluster, FAT32_CLUSTER_EOC_LAST) ||
        !DirectoryClearCluster(Parent, Cluster) ||
        !DirectoryClusterSectorLBA(Parent, Cluster, 0, &LBA) ||
        !IDEReadSector(LBA, Sector)) return false;

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
    DotDot->FirstClusterHigh = (uint16_t)(Parent->FirstCluster >> 16);
    DotDot->FirstClusterLow = (uint16_t)Parent->FirstCluster;
    if (!IDEWriteSector(LBA, Sector) ||
        !FAT32DirectoryCreateEntry(Parent, Name, FAT32_ATTRIBUTE_DIRECTORY,
                                   Cluster, 0, 0)) return false;

    return FAT32DirectoryOpen(CreatedDirectory, Parent->Boot,
                              Parent->PartitionStartLBA, Cluster);
}
