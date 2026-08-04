#include "installer.h"

#include <stdint.h>
#include <stddef.h>

#include "kernel/terminal/terminal.h"
#include "kernel/debug/debug.h"
#include "drivers/input/keyboard/keyboard.h"
#include "drivers/storage/ide/ide.h"
#include "fs/ISO9660/iso9660.h"
#include "fs/fat32/directory/directory.h"
#include "fs/fat32/file/file.h"
#include "fs/fat32/fat/fat.h"
#include "fs/fat32/vbr/vbr.h"
#include "fs/partition/mbr/mbr.h"
#include "mm/heap/heap.h"
#include "lib/string/string.h"

#define INSTALL_ISO_SECTOR 2048U
#define INSTALL_MAX_DEPTH  8U

#define INST_COLOR_TITLE  0x00FFD700U
#define INST_COLOR_GOOD   0x0000FF00U
#define INST_COLOR_BAD    0xFFFF0000U
#define INST_COLOR_HEADER 0x0057DB92U
#define INST_COLOR_FIELD  0x00E8F0FFU

/* RAM tree of everything found in /rootfs on the CD. */
typedef struct InstallNode
{
    char Name[64];
    bool IsDir;
    uint32_t Size;
    void *Data;
    struct InstallNode *Next;
    struct InstallNode *Child;
} InstallNode;

/* Telemetry collected while staging /rootfs from the CD. */
static uint32_t g_StagedFiles = 0;
static uint32_t g_StagedDirs  = 0;
static uint32_t g_StagedBytes = 0;
static uint32_t g_MaxDepth    = 0;

/* Diagnostics for InstallMountDisk. */
typedef struct
{
    bool HasMBR;
    uint8_t PartitionCount;
    uint8_t PartitionType;
    uint32_t PartitionStartLBA;
    bool Superfloppy;
} InstallDiskInfo;

static void InstallBar(const char *Title)
{
    TerminalPrintLine32("--------------------------------------------------------",
                        INST_COLOR_HEADER);
    TerminalPrintLine32(Title, INST_COLOR_TITLE);
    TerminalPrintLine32("--------------------------------------------------------",
                        INST_COLOR_HEADER);
}

static void InstallField(const char *Name, const char *Value)
{
    TerminalWrite32("  ", PromColor);
    TerminalWrite32(Name, INST_COLOR_FIELD);
    TerminalWrite32(" : ", PromColor);
    TerminalWrite32(Value, PromColor);
    TerminalPutChar32('\n', PromColor);
}

static void InstallSerialHex(const uint8_t *Bytes, uint32_t Count)
{
    static const char Digits[] = "0123456789ABCDEF";
    char Line[256];
    uint32_t Position = 0;

    if (Bytes == NULL)
    {
        return;
    }
    for (uint32_t i = 0; i < Count && Position + 3U < sizeof(Line); i++)
    {
        Line[Position++] = Digits[Bytes[i] >> 4];
        Line[Position++] = Digits[Bytes[i] & 0xF];
        Line[Position++] = (i % 16U == 15U) ? '\n' : ' ';
    }
    Line[Position] = '\0';
    DbgSerialWrite("SECTOR0 ");
    DbgSerialWrite(Line);
    DbgSerialWrite("\n");
}

static void InstallSerialNumber(uint64_t Value)
{
    char Buffer[24];
    uint32_t Index = 0;

    if (Value == 0)
    {
        DbgSerialWrite("0");
        return;
    }
    while (Value != 0 && Index + 1U < sizeof(Buffer))
    {
        Buffer[Index++] = (char)('0' + (Value % 10U));
        Value /= 10U;
    }
    while (Index > 0)
    {
        DbgSerialPutChar(Buffer[--Index]);
    }
}

static bool InstallParseRecord(const uint8_t *Record, ISODirRecord *Out)
{
    uint8_t Len;
    uint8_t CopyLen;

    if (Record == NULL || Out == NULL)
    {
        return false;
    }

    Len = Record[0];
    if (Len < 33)
    {
        return false;
    }

    Out->Length = Len;
    memcpy(&Out->ExtentLBA, Record + 2, 4);
    memcpy(&Out->DataLength, Record + 10, 4);
    Out->Flags = Record[25];
    Out->FileIdentifierLength = Record[32];

    CopyLen = Out->FileIdentifierLength < 255U ? Out->FileIdentifierLength : 255U;
    for (uint8_t i = 0; i < CopyLen; i++)
    {
        Out->FileIdentifier[i] = (char)Record[33 + i];
    }
    Out->FileIdentifier[CopyLen] = '\0';

    return true;
}

/* Strips ";1" version suffix and trailing dot from ISO names. */
static void InstallCleanName(const char *In, char *Out, uint32_t OutSize)
{
    uint32_t n = 0;

    while (In[n] != '\0' && In[n] != ';' && n + 1U < OutSize)
    {
        Out[n] = In[n];
        n++;
    }
    Out[n] = '\0';
    if (n > 0 && Out[n - 1U] == '.')
    {
        Out[n - 1U] = '\0';
    }
}

/* Phase 1: read the whole ISO directory tree into RAM (CD is active). */
static bool InstallReadDir(const ISODirRecord *Dir, InstallNode *Parent, uint32_t Depth)
{
    uint8_t Buffer[INSTALL_ISO_SECTOR];
    uint32_t TotalSectors;
    uint32_t BytesProcessed = 0;
    InstallNode **Tail;

    if (Dir == NULL || Parent == NULL || Depth > INSTALL_MAX_DEPTH || Dir->ExtentLBA == 0)
    {
        return false;
    }

    if (Depth > g_MaxDepth)
    {
        g_MaxDepth = Depth;
    }

    Tail = &Parent->Child;
    TotalSectors = (Dir->DataLength + INSTALL_ISO_SECTOR - 1U) / INSTALL_ISO_SECTOR;
    if (TotalSectors == 0)
    {
        TotalSectors = 1;
    }

    for (uint32_t s = 0; s < TotalSectors && BytesProcessed < Dir->DataLength; s++)
    {
        uint32_t Offset = 0;

        if (!ISOReadSector(Dir->ExtentLBA + s, Buffer))
        {
            return false;
        }

        while (Offset < INSTALL_ISO_SECTOR && (BytesProcessed + Offset) < Dir->DataLength)
        {
            const uint8_t *Record = Buffer + Offset;
            uint8_t RecLen = Record[0];
            ISODirRecord Entry;
            InstallNode *Node;

            if (RecLen == 0)
            {
                break;
            }
            Offset += RecLen;
            if (!InstallParseRecord(Record, &Entry))
            {
                continue;
            }

            /* Skip "." and ".." */
            if (Entry.FileIdentifierLength == 1U &&
                (Record[33] == 0x00 || Record[33] == 0x01))
            {
                continue;
            }

            Node = (InstallNode *)kmalloc(sizeof(InstallNode));
            if (Node == NULL)
            {
                return false;
            }
            memset(Node, 0, sizeof(InstallNode));
            InstallCleanName(Entry.FileIdentifier, Node->Name, sizeof(Node->Name));
            if (Node->Name[0] == '\0')
            {
                continue;
            }

            Node->IsDir = (Entry.Flags & 0x02) != 0;
            if (Node->IsDir)
            {
                g_StagedDirs++;
                if (!InstallReadDir(&Entry, Node, Depth + 1U))
                {
                    return false;
                }
            }
            else
            {
                uint32_t Read = 0;
                Node->Size = Entry.DataLength;
                if (Node->Size > 0)
                {
                    Node->Data = kmalloc(Node->Size);
                    if (Node->Data == NULL ||
                        !ISOReadFile(&Entry, Node->Data, Node->Size, &Read))
                    {
                        return false;
                    }
                }
                g_StagedFiles++;
                g_StagedBytes += Node->Size;
            }

            *Tail = Node;
            Tail = &Node->Next;
        }
        BytesProcessed += INSTALL_ISO_SECTOR;
    }

    return true;
}

/* Feeds a RAM-staged file to the FAT32 writer. */
static bool InstallRamReader(void *Context, uint32_t Offset, void *Buffer, uint32_t ByteCount)
{
    const InstallNode *Node = (const InstallNode *)Context;

    if (Node == NULL || Node->Data == NULL || Buffer == NULL)
    {
        return false;
    }
    memcpy(Buffer, (const uint8_t *)Node->Data + Offset, ByteCount);
    return true;
}

/* Phase 2: write one RAM node to the FAT32 disk (disk is active). */
static bool InstallWriteNode(const FAT32Directory *Parent, const InstallNode *Node)
{
    if (Node->IsDir)
    {
        FAT32Directory Child;

        if (!FAT32DirectoryCreateDirectory((FAT32Directory *)Parent, Node->Name, &Child))
        {
            return false;
        }
        for (Node = Node->Child; Node != NULL; Node = Node->Next)
        {
            if (!InstallWriteNode(&Child, Node))
            {
                return false;
            }
        }
        return true;
    }

    return FAT32FileWriteFromReader((FAT32Directory *)Parent, Node->Name,
                                    Node->Size, InstallRamReader, (void *)Node);
}

/* Counts free (unallocated) clusters in the FAT table. */
static uint32_t InstallCountFreeClusters(const FAT32BootSector *Boot,
                                         uint32_t PartitionStartLBA)
{
    uint32_t TotalSectors = Boot->TotalSectors32 ? Boot->TotalSectors32
                                                 : Boot->TotalSectors16;
    uint32_t Reserved = (uint32_t)Boot->ReservedSectorCount +
                        (uint32_t)Boot->NumberOfFATs * Boot->FATSize32;
    uint64_t EntryCount;
    uint32_t Free = 0;
    uint32_t Cluster;

    (void)PartitionStartLBA;
    if (TotalSectors <= Reserved || Boot->SectorsPerCluster == 0)
    {
        return 0;
    }

    uint32_t DataSectors = TotalSectors - Reserved;
    uint32_t Clusters = DataSectors / Boot->SectorsPerCluster;

    /* Include FAT entries 0 and 1, which are reserved by FAT32. */
    EntryCount = Clusters + 2U;
    if (EntryCount > (uint64_t)Boot->FATSize32 * 128U)
    {
        EntryCount = (uint64_t)Boot->FATSize32 * 128U;
    }

    /* Read each FAT sector once instead of one IDE read per entry. */
    for (uint32_t FatSector = 0; FatSector < Boot->FATSize32; FatSector++)
    {
        uint8_t Sector[512];
        uint64_t SectorLBA = (uint64_t)PartitionStartLBA +
                             (uint64_t)Boot->ReservedSectorCount + FatSector;

        if (SectorLBA > UINT32_MAX ||
            !IDEReadSector((uint32_t)SectorLBA, Sector))
        {
            break;
        }
        for (uint32_t Entry = 0; Entry < 128U; Entry++)
        {
            uint32_t Next;

            Cluster = FatSector * 128U + Entry;
            if (Cluster >= EntryCount)
            {
                return Free;
            }
            if (Cluster < 2U)
            {
                continue;
            }
            Next = (uint32_t)Sector[Entry * 4U] |
                   ((uint32_t)Sector[Entry * 4U + 1U] << 8) |
                   ((uint32_t)Sector[Entry * 4U + 2U] << 16) |
                   ((uint32_t)Sector[Entry * 4U + 3U] << 24);
            if (FATIsFreeCluster(Next))
            {
                Free++;
            }
        }
    }
    return Free;
}

/* Dumps FAT32 BPB telemetry + free space before/after the write. */
static void InstallDumpBPB(const FAT32BootSector *Boot, uint32_t PartitionStartLBA)
{
    uint32_t TotalSectors = Boot->TotalSectors32 ? Boot->TotalSectors32
                                                 : Boot->TotalSectors16;
    uint32_t FreeClusters = InstallCountFreeClusters(Boot, PartitionStartLBA);
    uint32_t ClusterSize = (uint32_t)Boot->SectorsPerCluster *
                           (uint32_t)Boot->BytesPerSector;
    uint32_t DataSectors;
    uint32_t TotalClusters;
    char Text[80];
    char Hex[32];

    TerminalPrintLine32("  FAT32 volume telemetry :", INST_COLOR_TITLE);

    DataSectors = TotalSectors - ((uint32_t)Boot->ReservedSectorCount +
                                  (uint32_t)Boot->NumberOfFATs * Boot->FATSize32);
    TotalClusters = DataSectors / Boot->SectorsPerCluster;

    TerminalWrite32("  Cluster size       : ", PromColor);
    TerminalPrintHex64Compact(ClusterSize);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Sectors / cluster  : ", PromColor);
    TerminalPrintHex64Compact(Boot->SectorsPerCluster);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Reserved sectors   : ", PromColor);
    TerminalPrintHex64Compact(Boot->ReservedSectorCount);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  FAT count          : ", PromColor);
    TerminalPrintHex64Compact(Boot->NumberOfFATs);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  FAT size (sectors) : ", PromColor);
    TerminalPrintHex64Compact(Boot->FATSize32);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Root cluster       : ", PromColor);
    TerminalPrintHex64Compact(Boot->RootCluster);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Volume sectors     : ", PromColor);
    TerminalPrintHex64Compact(TotalSectors);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Data clusters      : ", PromColor);
    TerminalPrintHex64Compact(TotalClusters);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Free clusters      : ", PromColor);
    TerminalPrintHex64Compact(FreeClusters);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  Free space (bytes) : ", PromColor);
    TerminalPrintHex64Compact((uint64_t)FreeClusters * ClusterSize);
    TerminalPutChar32('\n', PromColor);

    (void)Text;
    (void)Hex;
}

/* Phase 5: verification walk over the freshly written disk root. */
typedef struct
{
    const InstallNode *Expected;
    uint32_t FoundEntries;
    uint32_t Missing;
    uint32_t BytesOnDisk;
} InstallVerifyCtx;

static bool InstallVerifyVisitor(const FAT32DirectoryEntry *Entry,
                                 const char *ShortName, void *Context)
{
    InstallVerifyCtx *Ctx = (InstallVerifyCtx *)Context;
    InstallNode **Prev = (InstallNode **)&Ctx->Expected->Child;
    InstallNode *Node;

    if (Entry == NULL || ShortName == NULL || Ctx == NULL)
    {
        return false;
    }

    for (Node = *Prev; Node != NULL; Node = Node->Next)
    {
        if (strcasecmp(Node->Name, ShortName) == 0)
        {
            *Prev = Node->Next;
            Node->Next = NULL;
            Ctx->FoundEntries++;
            Ctx->BytesOnDisk += Node->Size;
            TerminalWrite32("  verified  : ", INST_COLOR_GOOD);
            TerminalWrite32(ShortName, INST_COLOR_FIELD);
            TerminalPutChar32('\n', INST_COLOR_GOOD);
            return true;
        }
        Prev = &Node->Next;
    }

    /* Entry on disk was not staged (e.g. leftover from an earlier install). */
    TerminalWrite32("  extra     : ", 0x00FFA500U);
    TerminalWrite32(ShortName, 0x00FFA500U);
    TerminalPutChar32('\n', 0x00FFA500U);
    return true;
}

static void InstallVerifyDisk(FAT32Directory *Root, const InstallNode *RootNode)
{
    InstallVerifyCtx Ctx;
    const InstallNode *Node;

    Ctx.Expected = RootNode;
    Ctx.FoundEntries = 0;
    Ctx.Missing = 0;
    Ctx.BytesOnDisk = 0;

    TerminalPrintLine32("  [verify] re-reading FAT32 root directory...", INST_COLOR_TITLE);

    if (!FAT32DirectoryForEach(Root, InstallVerifyVisitor, &Ctx))
    {
        TerminalPrintLine32("  [verify] FATAL: root directory walk failed", INST_COLOR_BAD);
        return;
    }

    for (Node = RootNode->Child; Node != NULL; Node = Node->Next)
    {
        Ctx.Missing++;
        TerminalWrite32("  MISSING   : ", INST_COLOR_BAD);
        TerminalPrintLine32(Node->Name, INST_COLOR_BAD);
    }

    TerminalWrite32("  [verify] entries found on disk : ", PromColor);
    TerminalPrintHex64Compact(Ctx.FoundEntries);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  [verify] entries staged from CD : ", PromColor);
    TerminalPrintHex64Compact(g_StagedFiles + g_StagedDirs);
    TerminalPutChar32('\n', PromColor);

    if (Ctx.Missing == 0U)
    {
        TerminalPrintLine32("  [verify] RESULT: PASS - all staged entries present",
                            INST_COLOR_GOOD);
    }
    else
    {
        TerminalPrintLine32("  [verify] RESULT: FAIL - missing entries above",
                            INST_COLOR_BAD);
    }
}

/* Phase 6: copy boot files from CD so the disk becomes self-bootable. */
static bool InstallCopyOneBootFile(const FAT32Directory *Dir, const char *SourceLabel,
                                   const char *DiskName, const ISODirRecord *Record)
{
    uint32_t Read = 0;
    InstallNode Node;

    memset(&Node, 0, sizeof(Node));

    if (Record == NULL || Record->ExtentLBA == 0 || Record->DataLength == 0)
    {
        TerminalWrite32("  [boot ] source missing  : ", INST_COLOR_BAD);
        TerminalPrintLine32(SourceLabel, INST_COLOR_BAD);
        return false;
    }

    Node.Size = Record->DataLength;
    if (Node.Size > 8U * 1024U * 1024U)
    {
        TerminalWrite32("  [boot ] bad size        : ", INST_COLOR_BAD);
        TerminalPrintLine32(SourceLabel, INST_COLOR_BAD);
        return false;
    }

    if (!IDESelectCD())
    {
        TerminalPrintLine32("  [boot ] FATAL: cannot switch to CD-ROM", INST_COLOR_BAD);
        return false;
    }

    Node.Data = kmalloc(Node.Size);
    if (Node.Data == NULL || !ISOReadFile(Record, Node.Data, Node.Size, &Read))
    {
        TerminalWrite32("  [boot ] read failed     : ", INST_COLOR_BAD);
        TerminalPrintLine32(SourceLabel, INST_COLOR_BAD);
        return false;
    }

    if (!IDESelectDisk())
    {
        TerminalPrintLine32("  [boot ] FATAL: cannot switch back to disk", INST_COLOR_BAD);
        return false;
    }

    if (!FAT32FileWriteFromReader((FAT32Directory *)Dir, DiskName, Node.Size,
                                  InstallRamReader, (void *)&Node))
    {
        TerminalWrite32("  [boot ] write failed    : ", INST_COLOR_BAD);
        TerminalPrintLine32(DiskName, INST_COLOR_BAD);
        return false;
    }

    TerminalWrite32("  [boot ] copied          : ", INST_COLOR_GOOD);
    TerminalWrite32(DiskName, INST_COLOR_FIELD);
    TerminalWrite32(" (", PromColor);
    TerminalPrintHex64Compact(Node.Size);
    TerminalPrintLine32(" bytes)", INST_COLOR_GOOD);

    return true;
}

static bool InstallOpenOrCreateDir(const FAT32Directory *Parent, const char *Name,
                                   FAT32Directory *Out)
{
    FAT32DirectoryEntry Entry;

    if (FAT32DirectoryCreateDirectory((FAT32Directory *)Parent, Name, Out))
    {
        return true;
    }

    if (!FAT32DirectoryFind(Parent, Name, &Entry))
    {
        return false;
    }
    return FAT32DirectoryOpen(Out, Parent->Boot, Parent->PartitionStartLBA,
                              FAT32DirectoryEntryFirstCluster(&Entry));
}

static void InstallCopyBootFiles(const FAT32Directory *Root)
{
    FAT32Directory BootDir;
    FAT32Directory LimineDir;
    ISODirRecord KernelRec;
    ISODirRecord BiosSysRec;
    ISODirRecord LimineConfRec;

    TerminalPrintLine32("  [boot ] installing bootloader payload to disk...",
                        INST_COLOR_TITLE);

    if (!InstallOpenOrCreateDir(Root, "BOOT", &BootDir))
    {
        TerminalPrintLine32("  [boot ] FATAL: cannot open/create BOOT directory",
                            INST_COLOR_BAD);
        return;
    }

    if (!InstallOpenOrCreateDir(&BootDir, "LIMINE", &LimineDir))
    {
        TerminalPrintLine32("  [boot ] FATAL: cannot open/create LIMINE directory",
                            INST_COLOR_BAD);
        return;
    }

    {
        FAT32DirectoryEntry Existing;

        /* run-install.sh stages the boot files on the host side; skip the
           slow CD-ROM copy when the payload is already on the disk. */
        if (FAT32DirectoryFind(&LimineDir, "LIMINE~1.SYS", &Existing))
        {
            TerminalPrintLine32("  [boot ] payload already on disk, skipping CD copy",
                                INST_COLOR_GOOD);
            return;
        }
    }

    if (!IDESelectCD())
    {
        TerminalPrintLine32("  [boot ] FATAL: cannot switch to CD-ROM", INST_COLOR_BAD);
        return;
    }

    if (!ISOFindPath("/boot/kernel.elf", &KernelRec))
    {
        TerminalWrite32("  [boot ] source missing  : ", INST_COLOR_BAD);
        TerminalPrintLine32("/boot/kernel.elf", INST_COLOR_BAD);
        return;
    }

    if (!ISOFindPath("/boot/limine/limine_b.sys", &BiosSysRec))
    {
        TerminalWrite32("  [boot ] source missing  : ", INST_COLOR_BAD);
        TerminalPrintLine32("/boot/limine/limine-bios.sys", INST_COLOR_BAD);
        return;
    }

    if (!ISOFindPath("/boot/limine/limine.con", &LimineConfRec))
    {
        TerminalWrite32("  [boot ] source missing  : ", INST_COLOR_BAD);
        TerminalPrintLine32("/boot/limine/limine.conf", INST_COLOR_BAD);
        return;
    }

    InstallCopyOneBootFile(&BootDir, "/boot/kernel.elf", "kernel.elf",
                           &KernelRec);
    InstallCopyOneBootFile(&LimineDir, "/boot/limine/limine-bios.sys",
                           "limine-bios.sys", &BiosSysRec);
    InstallCopyOneBootFile(&LimineDir, "/boot/limine/limine.conf",
                           "limine.conf", &LimineConfRec);

    TerminalPrintLine32("  [boot ] run 'limine bios-install esp.img' on the host",
                        INST_COLOR_FIELD);
    TerminalPrintLine32("  [boot ] then boot QEMU with: -hda esp.img (no CD)",
                        INST_COLOR_FIELD);
}

/* Switches IDE to the hard disk and opens its FAT32 root. */
static bool InstallMountDisk(FAT32BootSector *Boot, FAT32Directory *Root,
                             InstallDiskInfo *Info)
{
    uint8_t Sector[512];
    uint32_t PartitionStartLBA = 0;

    if (Info == NULL)
    {
        return false;
    }
    memset(Info, 0, sizeof(*Info));

    if (Boot == NULL || Root == NULL || !IDESelectDisk() || !IDEReadSector(0, Sector))
    {
        return false;
    }

    InstallSerialHex(Sector, 64);

    {
        const MBR *Table = (const MBR *)Sector;
        Info->HasMBR = MBRIsValid(Table);
        Info->PartitionCount = MBRGetPartitionCount(Table);

        DbgSerialWrite("MBR valid=");
        DbgSerialWrite(Info->HasMBR ? "yes" : "no");
        DbgSerialWrite(" count=");
        InstallSerialNumber(Info->PartitionCount);
        DbgSerialWrite("\n");

        if (Info->HasMBR && Info->PartitionCount > 0)
        {
            const MBRPartitionEntry *Part = MBRGetPartition(Table, 0);
            if (Part == NULL ||
                !(Part->PartitionType == 0x0BU || Part->PartitionType == 0x0CU))
            {
                TerminalWrite32("  [disk ] bad partition type  : 0x", INST_COLOR_BAD);
                if (Part != NULL)
                {
                    TerminalPrintHex64Compact(Part->PartitionType);
                }
                else
                {
                    TerminalPrintLine32("none", INST_COLOR_BAD);
                }
                TerminalPutChar32('\n', INST_COLOR_BAD);
                return false;
            }
            Info->PartitionType = Part->PartitionType;
            Info->PartitionStartLBA = Part->StartLBA;
            PartitionStartLBA = Part->StartLBA;
            DbgSerialWrite("PARTITION path type=0x0C start=");
            InstallSerialNumber(PartitionStartLBA);
            DbgSerialWrite("\n");
            if (!IDEReadSector(PartitionStartLBA, Sector))
            {
                return false;
            }
        }
        else
        {
            /* Superfloppy layout: FAT32 volume starts at LBA 0. */
            Info->Superfloppy = true;
            DbgSerialWrite("SUPERFLOPPY path (LBA 0 VBR)\n");
        }
    }

    memcpy(Boot, Sector, sizeof(FAT32BootSector));
    if (!FAT32IsValid(Boot))
    {
        TerminalPrintLine32("  [disk ] FATAL: invalid FAT32 boot sector", INST_COLOR_BAD);
        DbgSerialWrite("FAT32 VBR INVALID\n");
        return false;
    }

    DbgSerialWrite("VBR bps=");
    InstallSerialNumber(Boot->BytesPerSector);
    DbgSerialWrite(" spc=");
    InstallSerialNumber(Boot->SectorsPerCluster);
    DbgSerialWrite(" res=");
    InstallSerialNumber(Boot->ReservedSectorCount);
    DbgSerialWrite(" fats=");
    InstallSerialNumber(Boot->NumberOfFATs);
    DbgSerialWrite(" fat32=");
    InstallSerialNumber(Boot->FATSize32);
    DbgSerialWrite(" total=");
    InstallSerialNumber(Boot->TotalSectors32 ? Boot->TotalSectors32
                                             : Boot->TotalSectors16);
    DbgSerialWrite(" rootcl=");
    InstallSerialNumber(Boot->RootCluster);
    DbgSerialWrite("\n");
    DbgSerialWrite("FAT32 geometry: first FAT=");
    InstallSerialNumber(FAT32GetFirstFATSector(Boot, PartitionStartLBA));
    DbgSerialWrite(" first data=");
    InstallSerialNumber(FAT32GetFirstDataSector(Boot, PartitionStartLBA));
    DbgSerialWrite(" free clusters=");
    InstallSerialNumber(InstallCountFreeClusters(Boot, PartitionStartLBA));
    DbgSerialWrite("\n");

    return FAT32DirectoryOpenRoot(Root, Boot, PartitionStartLBA);
}

bool InstallerRun(void)
{
    uint8_t Key;
    ISODirRecord RootFs;
    InstallNode RootNode;
    InstallNode *Node;
    uint32_t ItemCount = 0;

    KbdFlushBuffer();

    TerminalPrintLine32("==============================", INST_COLOR_HEADER);
    TerminalPrintLine32("        NullOS Live CD", INST_COLOR_TITLE);
    TerminalPrintLine32("==============================", INST_COLOR_HEADER);
    TerminalPrintLine32("(Try OS or Install?) [T/I]", INST_COLOR_GOOD);
    TerminalWrite32("> ", PromColor);

    while (!KbdAvailable())
    {
        __asm__ volatile("hlt");
    }
    Key = (uint8_t)KbdReadChar();

    if (Key != 'i' && Key != 'I')
    {
        TerminalPrintLine32("Trying live environment...", PromColor);
        return false;
    }

    InstallBar("N U L L O S   I N S T A L L E R   -   VERBOSE MODE");
    TerminalPrintLine32("  [phase] boot device sanity check", INST_COLOR_TITLE);

    if (!IDEIsATAPI())
    {
        TerminalPrintLine32("  [cd   ] WARN: no ATAPI CD-ROM active (booting from disk?)",
                            INST_COLOR_BAD);
    }
    else
    {
        TerminalPrintLine32("  [cd   ] OK: ATAPI CD-ROM is the active boot device",
                            INST_COLOR_GOOD);
    }

    TerminalPrintLine32("  [phase] mounting /rootfs from ISO9660 CD", INST_COLOR_TITLE);

    if (!ISOFindPath("/rootfs", &RootFs))
    {
    TerminalPrintLine32("install: /rootfs not found on CD", INST_COLOR_BAD);
    DbgSerialWrite("INSTALL RESULT: FAIL (no /rootfs on CD)\n");
    return false;
    }

    memset(&RootNode, 0, sizeof(RootNode));
    RootNode.IsDir = true;
    if (!InstallReadDir(&RootFs, &RootNode, 0))
    {
        TerminalPrintLine32("install: failed to read /rootfs from CD", INST_COLOR_BAD);
        return false;
    }

    for (Node = RootNode.Child; Node != NULL; Node = Node->Next)
    {
        ItemCount++;
    }

    TerminalPrintLine32("  [phase] staging tree in RAM complete", INST_COLOR_TITLE);
    TerminalWrite32("  [ram  ] top-level entries : ", PromColor);
    TerminalPrintHex64Compact(ItemCount);
    TerminalPutChar32('\n', PromColor);
    TerminalWrite32("  [ram  ] files / dirs      : ", PromColor);
    TerminalPrintHex64Compact(g_StagedFiles);
    TerminalWrite32(" / ", PromColor);
    TerminalPrintHex64Compact(g_StagedDirs);
    TerminalPutChar32('\n', PromColor);
    TerminalWrite32("  [ram  ] total bytes       : ", PromColor);
    TerminalPrintHex64Compact(g_StagedBytes);
    TerminalPutChar32('\n', PromColor);
    TerminalWrite32("  [ram  ] max tree depth    : ", PromColor);
    TerminalPrintHex64Compact(g_MaxDepth);
    TerminalPutChar32('\n', PromColor);

    TerminalPrintLine32("  [phase] probing IDE and mounting FAT32 disk", INST_COLOR_TITLE);

    {
        FAT32BootSector Boot;
        FAT32Directory FatRoot;
        InstallDiskInfo Info;

        if (!InstallMountDisk(&Boot, &FatRoot, &Info))
        {
            TerminalPrintLine32("install: no FAT32 disk found", INST_COLOR_BAD);
            DbgSerialWrite("INSTALL RESULT: FAIL (no FAT32 disk)\n");
            return false;
        }

        TerminalWrite32("  [disk ] MBR present      : ", PromColor);
        TerminalPrintLine32(Info.HasMBR ? "yes" : "no", Info.HasMBR ? INST_COLOR_GOOD : INST_COLOR_BAD);
        TerminalWrite32("  [disk ] MBR partitions   : ", PromColor);
        TerminalPrintHex64Compact(Info.PartitionCount);
        TerminalPutChar32('\n', PromColor);
        if (Info.Superfloppy)
        {
            TerminalPrintLine32("  [disk ] layout: superfloppy (FAT32 at LBA 0)",
                                INST_COLOR_FIELD);
        }
        else
        {
            TerminalWrite32("  [disk ] partition type   : 0x", PromColor);
            TerminalPrintHex64Compact(Info.PartitionType);
            TerminalPutChar32('\n', PromColor);
            TerminalWrite32("  [disk ] partition LBA    : ", PromColor);
            TerminalPrintHex64Compact(Info.PartitionStartLBA);
            TerminalPutChar32('\n', PromColor);
        }

        InstallDumpBPB(&Boot, Info.PartitionStartLBA);

        TerminalPrintLine32("  [phase] writing staged tree to disk", INST_COLOR_TITLE);

        for (Node = RootNode.Child; Node != NULL; Node = Node->Next)
        {
            TerminalWrite32("  [write] ", PromColor);
            TerminalWrite32(Node->Name, INST_COLOR_FIELD);
            TerminalWrite32(" ... ", PromColor);
            if (InstallWriteNode(&FatRoot, Node))
            {
                TerminalWrite32("OK", INST_COLOR_GOOD);
                TerminalWrite32(" (", PromColor);
                TerminalPrintHex64Compact(Node->Size);
                TerminalPrintLine32(" bytes)", INST_COLOR_GOOD);
            }
            else
            {
                TerminalPrintLine32("FAILED", INST_COLOR_BAD);
            }
        }

        TerminalPrintLine32("  [phase] post-write free space", INST_COLOR_TITLE);
        InstallDumpBPB(&Boot, Info.PartitionStartLBA);

        TerminalPrintLine32("  [phase] verification pass", INST_COLOR_TITLE);
        InstallVerifyDisk(&FatRoot, &RootNode);

        TerminalPrintLine32("  [phase] installing bootloader payload", INST_COLOR_TITLE);
        InstallCopyBootFiles(&FatRoot);

        TerminalPrintLine32("  [phase] final free space", INST_COLOR_TITLE);
        InstallDumpBPB(&Boot, Info.PartitionStartLBA);
    }

    InstallBar("I N S T A L L   S U C C E S S");
    DbgSerialWrite("INSTALL RESULT: SUCCESS\n");
    DbgSerialWrite("files=");
    InstallSerialNumber(g_StagedFiles);
    DbgSerialWrite(" dirs=");
    InstallSerialNumber(g_StagedDirs);
    DbgSerialWrite(" bytes=");
    InstallSerialNumber(g_StagedBytes);
    DbgSerialWrite("\n");
    TerminalWrite32("  Files written : ", PromColor);
    TerminalPrintHex64Compact(g_StagedFiles);
    TerminalPutChar32('\n', PromColor);
    TerminalWrite32("  Bytes written : ", PromColor);
    TerminalPrintHex64Compact(g_StagedBytes);
    TerminalPutChar32('\n', PromColor);
    TerminalPrintLine32("  Disk is now bootable via Limine BIOS.", INST_COLOR_GOOD);
    TerminalPrintLine32("  Host:  limine-binary/limine bios-install esp.img",
                        INST_COLOR_FIELD);
    TerminalPrintLine32("  QEMU:  qemu-system-x86_64 -hda esp.img -boot order=c -m 3 -serial stdio",
                        INST_COLOR_FIELD);
    return false;
}
