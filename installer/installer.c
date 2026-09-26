#include "installer.h"

#include <stdint.h>
#include <stddef.h>

#include "kernel/terminal/terminal.h"
#include "kernel/debug/debug.h"
#include "drivers/input/keyboard/keyboard.h"
#include "drivers/storage/ide/ide.h"
#include "drivers/storage/block/block.h"
#include "limine-binary/limine-bios-hdd.h"
#include "fs/ISO9660/iso9660.h"
#include "fs/fat32/directory/directory.h"
#include "fs/fat32/file/file.h"
#include "fs/fat32/fat/fat.h"
#include "fs/fat32/vbr/vbr.h"
#include "fs/partition/mbr/mbr.h"
#include "mm/heap/heap.h"
#include "lib/string/string.h"
#include "libc/stdlib.h"

#define INSTALL_ISO_SECTOR 2048U
#define INSTALL_MAX_DEPTH  8U

/* 1970s monochrome terminal palette: phosphor green on black. */
#define INST_COLOR_TITLE  0x00A8FF70U
#define INST_COLOR_GOOD   0x00A8FF70U
#define INST_COLOR_BAD    0x00A8FF70U
#define INST_COLOR_HEADER 0x0070B84FU
#define INST_COLOR_FIELD  0x0070B84FU

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

/* Any failed FAT32 write (file, directory or subtree) marks the install as
   failed; SUCCESS is only reported when every staged node made it to disk. */
static uint32_t g_WriteFailures = 0;

#define INSTALL_PARTITION_LBA 2048U
#define INSTALL_RESERVED_SECTORS 32U
#define INSTALL_FAT_COUNT 2U
#define INSTALL_ROOT_CLUSTER 2U

/* NullLoader BIOS layout relative to the active FAT32 partition. */
#define NULLL_STAGE2_OFFSET    7U
#define NULLL_STAGE2_SECTORS   24U

/* Diagnostics for InstallMountDisk. */
typedef struct
{
    bool HasMBR;
    uint8_t PartitionCount;
    uint8_t PartitionType;
    uint32_t PartitionStartLBA;
    bool Superfloppy;
} InstallDiskInfo;

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
    return ISOParseDirectoryRecord(Record, Out);
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
            g_WriteFailures++;
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

    if (!FAT32FileWriteFromReader((FAT32Directory *)Parent, Node->Name,
                                  Node->Size, InstallRamReader, (void *)Node))
    {
        g_WriteFailures++;
        return false;
    }
    return true;
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
            !BlockRead(SectorLBA, Sector))
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

/* Phase 5: full recursive verification. Every staged file and directory must
   be present on disk with the expected size; SUCCESS is only printed when all
   staged entries are found. */
typedef struct
{
    uint32_t FoundEntries;
    uint32_t Missing;
    uint32_t SizeMismatch;
    uint32_t BytesOnDisk;
} InstallVerifyCtx;

/* Recurses over DiskDir, matching every child of the StagedDir node against
   the freshly written FAT32 tree. */
static bool InstallVerifyRecurse(const FAT32Directory *DiskDir,
                                 const InstallNode *StagedDir,
                                 InstallVerifyCtx *Ctx)
{
    const InstallNode *Node;

    if (DiskDir == NULL || StagedDir == NULL || Ctx == NULL)
    {
        return false;
    }

    for (Node = StagedDir->Child; Node != NULL; Node = Node->Next)
    {
        FAT32DirectoryEntry Entry;
        FAT32Directory Child;

        if (!FAT32DirectoryFind(DiskDir, Node->Name, &Entry))
        {
            Ctx->Missing++;
            TerminalWrite32("  MISSING   : ", INST_COLOR_BAD);
            TerminalWrite32(Node->Name, INST_COLOR_BAD);
            TerminalPutChar32('\n', INST_COLOR_BAD);
            continue;
        }

        Ctx->FoundEntries++;

        if (Node->IsDir)
        {
            if (!FAT32DirectoryOpen(&Child, DiskDir->Boot,
                                    DiskDir->PartitionStartLBA,
                                    FAT32DirectoryEntryFirstCluster(&Entry)))
            {
                Ctx->Missing++;
                TerminalWrite32("  MISSING   : ", INST_COLOR_BAD);
                TerminalWrite32(Node->Name, INST_COLOR_BAD);
                TerminalPutChar32('\n', INST_COLOR_BAD);
                continue;
            }
            if (!InstallVerifyRecurse(&Child, Node, Ctx))
            {
                return false;
            }
        }
        else
        {
            Ctx->BytesOnDisk += Entry.FileSize;
            if (Entry.FileSize != Node->Size)
            {
                Ctx->SizeMismatch++;
                TerminalWrite32("  SIZE MISMATCH : ", INST_COLOR_BAD);
                TerminalWrite32(Node->Name, INST_COLOR_BAD);
                TerminalWrite32(" (staged ", PromColor);
                TerminalPrintHex64Compact(Node->Size);
                TerminalWrite32(" disk ", PromColor);
                TerminalPrintHex64Compact(Entry.FileSize);
                TerminalPutChar32(')', INST_COLOR_BAD);
                TerminalPutChar32('\n', INST_COLOR_BAD);
            }
        }
    }

    return true;
}

static InstallVerifyCtx InstallVerifyDisk(const FAT32Directory *Root,
                                          const InstallNode *RootNode)
{
    InstallVerifyCtx Ctx;

    Ctx.FoundEntries = 0;
    Ctx.Missing = 0;
    Ctx.SizeMismatch = 0;
    Ctx.BytesOnDisk = 0;

    TerminalPrintLine32("  [verify] re-reading entire FAT32 tree from disk...",
                        INST_COLOR_TITLE);

    if (!InstallVerifyRecurse(Root, RootNode, &Ctx))
    {
        TerminalPrintLine32("  [verify] FATAL: directory tree walk failed",
                            INST_COLOR_BAD);
        Ctx.Missing += g_StagedFiles + g_StagedDirs;
        return Ctx;
    }

    TerminalWrite32("  [verify] entries found on disk : ", PromColor);
    TerminalPrintHex64Compact(Ctx.FoundEntries);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  [verify] entries staged from CD : ", PromColor);
    TerminalPrintHex64Compact(g_StagedFiles + g_StagedDirs);
    TerminalPutChar32('\n', PromColor);

    TerminalWrite32("  [verify] bytes on disk         : ", PromColor);
    TerminalPrintHex64Compact(Ctx.BytesOnDisk);
    TerminalPutChar32('\n', PromColor);

    if (Ctx.Missing == 0U && Ctx.SizeMismatch == 0U)
    {
        TerminalPrintLine32("  [verify] RESULT: PASS - every staged entry on disk",
                            INST_COLOR_GOOD);
    }
    else
    {
        TerminalWrite32("  [verify] RESULT: FAIL - ", INST_COLOR_BAD);
        TerminalPrintHex64Compact(Ctx.Missing);
        TerminalWrite32(" missing, ", INST_COLOR_BAD);
        TerminalPrintHex64Compact(Ctx.SizeMismatch);
        TerminalPrintLine32(" size mismatches", INST_COLOR_BAD);
    }

    return Ctx;
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

    if (!ISOFindPath("/boot/limine/limine-bios.sys", &BiosSysRec))
    {
        TerminalWrite32("  [boot ] source missing  : ", INST_COLOR_BAD);
        TerminalPrintLine32("/boot/limine/limine-bios.sys", INST_COLOR_BAD);
        return;
    }

    if (!ISOFindPath("/boot/limine/limine.conf", &LimineConfRec))
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

/* Formats a blank IDE/AHCI/NVMe target as MBR + FAT32. The ISO installer must
   not depend on a pre-created host image: VMware can hand us a blank
   disk. The FAT layout follows the standard BPB/FSInfo structure. */
static bool InstallFormatNVMe(uint64_t SectorCount)
{
    uint32_t PartitionStart = INSTALL_PARTITION_LBA;
    uint64_t PartitionSectors64;
    uint32_t PartitionSectors;
    uint8_t SectorsPerCluster = 1U;
    uint32_t FatSectors = 0;
    uint32_t Clusters = 0;
    uint8_t Sector[512];
    MBR Mbr;
    FAT32BootSector Boot;

    if (SectorCount <= PartitionStart + 4096U || SectorCount - PartitionStart > UINT32_MAX)
        return false;
    PartitionSectors64 = SectorCount - PartitionStart;
    PartitionSectors = (uint32_t)PartitionSectors64;

    for (uint8_t Spc = 64U; Spc >= 1U; Spc >>= 1U)
    {
        uint32_t Fat = 1U;
        uint32_t C = 0;
        for (uint32_t Iter = 0; Iter < 8U; Iter++)
        {
            uint32_t Data = PartitionSectors - INSTALL_RESERVED_SECTORS - INSTALL_FAT_COUNT * Fat;
            C = Data / Spc;
            Fat = (uint32_t)(((uint64_t)(C + 2U) * 4U + 511U) / 512U);
        }
        if (C >= 65525U && C <= 0x0FFFFFF5U)
        {
            SectorsPerCluster = Spc;
            FatSectors = Fat;
            Clusters = C;
            break;
        }
    }
    if (FatSectors == 0U || Clusters < 65525U) return false;

    memset(&Mbr, 0, sizeof(Mbr));
    Mbr.Partitions[0].BootIndicator = 0x80U;
    Mbr.Partitions[0].PartitionType = 0x0CU;
    Mbr.Partitions[0].StartLBA = PartitionStart;
    Mbr.Partitions[0].SectorCount = PartitionSectors;
    Mbr.Signature = 0xAA55U;
    if (!BlockWrite(0, &Mbr)) return false;

    memset(&Boot, 0, sizeof(Boot));
    Boot.JumpInstruction[0] = 0xEB; Boot.JumpInstruction[1] = 0x58; Boot.JumpInstruction[2] = 0x90;
    memcpy(Boot.OEMName, "NULLOS  ", 8);
    Boot.BytesPerSector = 512U;
    Boot.SectorsPerCluster = SectorsPerCluster;
    Boot.ReservedSectorCount = INSTALL_RESERVED_SECTORS;
    Boot.NumberOfFATs = INSTALL_FAT_COUNT;
    Boot.MediaDescriptor = 0xF8U;
    Boot.SectorsPerTrack = 63U;
    Boot.NumberOfHeads = 255U;
    Boot.HiddenSectors = PartitionStart;
    Boot.TotalSectors32 = PartitionSectors;
    Boot.FATSize32 = FatSectors;
    Boot.RootCluster = INSTALL_ROOT_CLUSTER;
    Boot.FSInfoSector = 1U;
    Boot.BackupBootSector = 6U;
    Boot.DriveNumber = 0x80U;
    Boot.BootSignature = 0x29U;
    Boot.VolumeID = 0x4E554C4CU;
    memcpy(Boot.VolumeLabel, "NULLOS     ", 11);
    memcpy(Boot.FileSystemType, "FAT32   ", 8);

    memset(Sector, 0, sizeof(Sector));
    memcpy(Sector, &Boot, sizeof(Boot));
    Sector[510] = 0x55; Sector[511] = 0xAA;
    if (!BlockWrite(PartitionStart, Sector) || !BlockWrite(PartitionStart + Boot.BackupBootSector, Sector)) return false;

    memset(Sector, 0, sizeof(Sector));
    Sector[0] = 0x52; Sector[1] = 0x52; Sector[2] = 0x61; Sector[3] = 0x41;
    Sector[484] = 0x72; Sector[485] = 0x72; Sector[486] = 0x41; Sector[487] = 0x61;
    Sector[488] = 0xFF; Sector[489] = 0xFF; Sector[490] = 0xFF; Sector[491] = 0xFF;
    Sector[492] = 0xFF; Sector[493] = 0xFF; Sector[494] = 0xFF; Sector[495] = 0xFF;
    Sector[510] = 0x55; Sector[511] = 0xAA;
    if (!BlockWrite(PartitionStart + Boot.FSInfoSector, Sector) || !BlockWrite(PartitionStart + Boot.BackupBootSector + Boot.FSInfoSector, Sector)) return false;

    memset(Sector, 0, sizeof(Sector));
    for (uint32_t Fat = 0; Fat < INSTALL_FAT_COUNT; Fat++)
    {
        uint64_t Base = (uint64_t)PartitionStart + Boot.ReservedSectorCount + (uint64_t)Fat * Boot.FATSize32;
        for (uint32_t S = 0; S < Boot.FATSize32; S++) if (!BlockWrite(Base + S, Sector)) return false;
    }
    Sector[0] = 0xF8; Sector[1] = 0xFF; Sector[2] = 0xFF; Sector[3] = 0x0F;
    Sector[4] = 0xFF; Sector[5] = 0xFF; Sector[6] = 0xFF; Sector[7] = 0x0F;
    Sector[8] = 0xFF; Sector[9] = 0xFF; Sector[10] = 0xFF; Sector[11] = 0x0F;
    for (uint32_t Fat = 0; Fat < INSTALL_FAT_COUNT; Fat++)
    {
        uint64_t Base = (uint64_t)PartitionStart + Boot.ReservedSectorCount + (uint64_t)Fat * Boot.FATSize32;
        if (!BlockWrite(Base, Sector)) return false;
    }

    memset(Sector, 0, sizeof(Sector));
    uint64_t RootLBA = (uint64_t)PartitionStart + Boot.ReservedSectorCount + (uint64_t)Boot.NumberOfFATs * Boot.FATSize32;
    for (uint32_t S = 0; S < Boot.SectorsPerCluster; S++) if (!BlockWrite(RootLBA + S, Sector)) return false;
    return true;
}

/* Install the exact Limine BIOS HDD image shipped with this repository. */
static bool InstallLimineBIOS(const MBR *FormattedMBR)
{
    const uint8_t *Image = binary_limine_hdd_bin_data;
    uint32_t ImageSize = (uint32_t)sizeof(binary_limine_hdd_bin_data);
    uint8_t Sector[512];
    if (FormattedMBR == NULL || ImageSize < 512U || ImageSize > (INSTALL_PARTITION_LBA - 1U) * 512U) return false;
    for (uint32_t Offset = 0; Offset < ImageSize; Offset += 512U)
    {
        uint32_t Bytes = ImageSize - Offset;
        if (Bytes > 512U) Bytes = 512U;
        memset(Sector, 0, sizeof(Sector));
        memcpy(Sector, Image + Offset, Bytes);
        if (!BlockWrite(Offset / 512U, Sector)) return false;
    }
    if (!BlockRead(0, Sector)) return false;
    /* Limine's own bios-install defaults the MBR stage-2 location to
       byte offset 512 (LBA 1) and writes bootloader_img[512..] there.
       Keep the same contract so the saved legacy MBR can chainload the
       untouched Limine stage-2 payload after NullL replaces LBA 0. */
    Sector[0x1A4] = 0x00; Sector[0x1A5] = 0x02;
    Sector[0x1A6] = 0x00; Sector[0x1A7] = 0x00;
    Sector[0x1A8] = 0x00; Sector[0x1A9] = 0x00;
    Sector[0x1AA] = 0x00; Sector[0x1AB] = 0x00;
    memcpy(Sector + 440U, ((const uint8_t *)FormattedMBR) + 440U, 70U);
    return BlockWrite(0, Sector);
}

static bool InstallReadISOFile(const char *Path, uint8_t **Data, uint32_t *Size)
{
    ISODirRecord Record;
    uint8_t *Buffer;
    uint32_t Read = 0;

    if (Path == NULL || Data == NULL || Size == NULL ||
        !ISOFindPath(Path, &Record) || Record.DataLength == 0U)
    {
        return false;
    }

    Buffer = (uint8_t *)kmalloc(Record.DataLength);
    if (Buffer == NULL)
    {
        return false;
    }

    if (!ISOReadFile(&Record, Buffer, Record.DataLength, &Read) ||
        Read != Record.DataLength)
    {
        kfree(Buffer);
        return false;
    }

    *Data = Buffer;
    *Size = Read;
    return true;
}

/*
 * Install the first NullLoader slice.
 *
 * Layout:
 *   LBA 0      = NullL stage 1 MBR code + existing partition table
 *   LBA 1..2046= the old Limine BIOS payload, left untouched
 *   partition_start-1 = complete old MBR / Old Boot Record backup
 *   partition_start   = FAT32 VBR
 *   partition_start+7  = NullL stage 2 inside FAT32 reserved sectors
 *
 * The old MBR is deliberately kept as a complete 512-byte record.  When
 * option 0 is selected, NullL restores it to 0x7C00 and jumps to it, while
 * its original Limine stage-2 sectors remain at LBA 1..2046.
 */
static bool InstallNullL(uint32_t PartitionStartLBA, uint16_t ReservedSectorCount)
{
    uint8_t *Stage1 = NULL;
    uint8_t *Stage2 = NULL;
    uint32_t Stage1Size = 0;
    uint32_t Stage2Size = 0;
    uint8_t OldMBR[512];
    uint8_t Sector[512];
    uint32_t Stage2Sectors;
    uint32_t OldMBRLBA;
    uint32_t Stage2LBA;

    if (PartitionStartLBA <= 1U ||
        ReservedSectorCount < 32U)
    {
        return false;
    }

    OldMBRLBA = PartitionStartLBA - 1U;
    Stage2LBA = PartitionStartLBA + NULLL_STAGE2_OFFSET;

    if (!IDESelectCD())
    {
        return false;
    }

    if (!InstallReadISOFile("/boot/nulll/stage1.bin", &Stage1, &Stage1Size) ||
        !InstallReadISOFile("/boot/nulll/stage2.bin", &Stage2, &Stage2Size))
    {
        if (Stage1 != NULL) kfree(Stage1);
        if (Stage2 != NULL) kfree(Stage2);
        return false;
    }

    if (Stage1Size == 0U || Stage1Size > 446U || Stage2Size == 0U)
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }

    Stage2Sectors = (Stage2Size + 511U) / 512U;
    if (Stage2Sectors > NULLL_STAGE2_SECTORS)
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }

    if (!IDESelectDisk() || !BlockRead(0, OldMBR) ||
        OldMBR[510] != 0x55U || OldMBR[511] != 0xAAU)
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }

    /* Preserve the complete previous boot record for menu option 0. */
    if (!BlockWrite(OldMBRLBA, OldMBR))
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }

    /* FAT32 reserved sector 2 is our one-shot mailbox; stage 2 lives
       later in the same reserved region. FSInfo is sector 1 and the backup
       VBR is sector 6, so neither is touched. */
    /* Clear the mailbox before installing stage 2. */
    memset(Sector, 0, sizeof(Sector));
    if (!BlockWrite(PartitionStartLBA + 2U, Sector))
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }

    for (uint32_t i = 0; i < NULLL_STAGE2_SECTORS; i++)
    {
        memset(Sector, 0, sizeof(Sector));
        if (i < Stage2Sectors)
        {
            uint32_t Offset = i * 512U;
            uint32_t Bytes = Stage2Size - Offset;
            if (Bytes > 512U) Bytes = 512U;
            memcpy(Sector, Stage2 + Offset, Bytes);
        }
        if (!BlockWrite(Stage2LBA + i, Sector))
        {
            kfree(Stage1);
            kfree(Stage2);
            return false;
        }
    }

    /* Replace only the 446-byte boot code.  Keep the disk signature,
       partition table, and 0xAA55 signature from the existing MBR. */
    if (!BlockRead(0, Sector))
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }
    memcpy(Sector, Stage1, Stage1Size);
    if (Stage1Size < 446U)
    {
        memset(Sector + Stage1Size, 0, 446U - Stage1Size);
    }

    if (!BlockWrite(0, Sector))
    {
        kfree(Stage1);
        kfree(Stage2);
        return false;
    }

    kfree(Stage1);
    kfree(Stage2);
    return true;
}

/* Switches IDE to the hard disk and opens its FAT32 root. */
static bool InstallMountDisk(FAT32BootSector *Boot, FAT32Directory *Root,
                             InstallDiskInfo *Info)
{
    uint8_t Sector[512];
    uint32_t PartitionStartLBA = 0;
    uint64_t Capacity = BlockGetSectorCount();

    if (Info == NULL)
    {
        return false;
    }
    memset(Info, 0, sizeof(*Info));

    if (Boot == NULL || Root == NULL || !BlockIsReady() || !BlockRead(0, Sector))
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
            if (!BlockRead(PartitionStartLBA, Sector))
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
        if (Capacity == 0 || !InstallFormatNVMe(Capacity) || !BlockRead(INSTALL_PARTITION_LBA, Sector))
        {
            TerminalPrintLine32("  [disk ] FATAL: invalid FAT32 disk and target cannot be formatted", INST_COLOR_BAD);
            DbgSerialWrite("FAT32 VBR INVALID / FORMAT FAILED\n");
            if (Capacity == 0)
            {
                DbgSerialWrite("REASON: capacity unknown (BlockGetSectorCount == 0)\n");
            }
            else
            {
                TerminalWrite32("  [disk ] target sectors : ", INST_COLOR_BAD);
                TerminalPrintHex64Compact(Capacity);
                TerminalPutChar32('\n', INST_COLOR_BAD);
            }
            return false;
        }
        memcpy(Boot, Sector, sizeof(FAT32BootSector));
        PartitionStartLBA = INSTALL_PARTITION_LBA;
        Info->HasMBR = true;
        Info->PartitionCount = 1U;
        Info->PartitionType = 0x0CU;
        Info->PartitionStartLBA = INSTALL_PARTITION_LBA;
        Info->Superfloppy = false;
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

InstallerResult InstallerRun(void)
{
    uint8_t Key;
    ISODirRecord RootFs;
    InstallNode RootNode;
    InstallNode *Node;
    uint32_t ItemCount = 0;

    KbdFlushBuffer();

    TerminalPrintLine32("NULL OPERATING SYSTEM - INSTALL", INST_COLOR_TITLE);
    TerminalPrintLine32("Mass storage installation terminal", INST_COLOR_FIELD);
    TerminalPrintLine32("", INST_COLOR_FIELD);
    TerminalPrintLine32("[T] Try live system", INST_COLOR_GOOD);
    TerminalPrintLine32("[I] Install to fixed disk", INST_COLOR_GOOD);
    TerminalWrite32("Command: ", INST_COLOR_TITLE);

    while (!KbdAvailable())
    {
        __asm__ volatile("hlt");
    }
    Key = (uint8_t)KbdReadChar();

    if (Key != 'i' && Key != 'I')
    {
        TerminalPrintLine32("Trying live environment...", PromColor);
        return INSTALLER_RESULT_LIVE;
    }

    TerminalPrintLine32("", INST_COLOR_FIELD);
    TerminalPrintLine32("NULL INSTALLER - SYSTEM BUILD", INST_COLOR_TITLE);
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

    /* Driver initialization leaves the block layer on the install target
       disk.  The live installer must explicitly select the CD before every
       ISO9660 read; otherwise ISOFindPath() searches the FAT32 disk instead
       of the boot media. */
    if (!IDESelectCD())
    {
        TerminalPrintLine32("install: cannot select ATAPI CD-ROM", INST_COLOR_BAD);
        DbgSerialWrite("INSTALL RESULT: FAIL (cannot select CD-ROM)\n");
        return INSTALLER_RESULT_FAILED;
    }

    if (!ISOFindPath("/rootfs", &RootFs))
    {
    TerminalPrintLine32("install: /rootfs not found on CD", INST_COLOR_BAD);
    DbgSerialWrite("INSTALL RESULT: FAIL (no /rootfs on CD)\n");
    return INSTALLER_RESULT_FAILED;
    }

    memset(&RootNode, 0, sizeof(RootNode));
    RootNode.IsDir = true;
    if (!InstallReadDir(&RootFs, &RootNode, 0))
    {
        TerminalPrintLine32("install: failed to read /rootfs from CD", INST_COLOR_BAD);
        DbgSerialWrite("INSTALL RESULT: FAIL (cannot read /rootfs)\n");
        return INSTALLER_RESULT_FAILED;
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
            return INSTALLER_RESULT_FAILED;
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
        InstallVerifyCtx Verify = InstallVerifyDisk(&FatRoot, &RootNode);

        TerminalPrintLine32("  [phase] installing bootloader payload", INST_COLOR_TITLE);
        InstallCopyBootFiles(&FatRoot);
        TerminalPrintLine32("  [phase] installing Limine BIOS stages", INST_COLOR_TITLE);
        {
            MBR FormattedMBR;
            memset(&FormattedMBR, 0, sizeof(FormattedMBR));
            FormattedMBR.Partitions[0].BootIndicator = 0x80U;
            FormattedMBR.Partitions[0].PartitionType = Info.PartitionType;
            FormattedMBR.Partitions[0].StartLBA = Info.PartitionStartLBA;
            FormattedMBR.Partitions[0].SectorCount = Boot.TotalSectors32;
            FormattedMBR.Signature = 0xAA55U;
            if (!InstallLimineBIOS(&FormattedMBR))
            {
                TerminalPrintLine32("  [boot ] FATAL: Limine BIOS stage install failed", INST_COLOR_BAD);
                g_WriteFailures++;
            }
        }

        TerminalPrintLine32("  [phase] installing NullLoader stage 1/2", INST_COLOR_TITLE);
        if (!InstallNullL(Info.PartitionStartLBA, Boot.ReservedSectorCount))
        {
            TerminalPrintLine32("  [boot ] FATAL: NullLoader installation failed", INST_COLOR_BAD);
            DbgSerialWrite("NULLL INSTALL: FAILED\n");
            g_WriteFailures++;
        }
        else
        {
            TerminalPrintLine32("  [boot ] NullLoader BIOS menu installed", INST_COLOR_GOOD);
            DbgSerialWrite("NULLL INSTALL: OK (old MBR saved at LBA 2047)\n");
        }

        TerminalPrintLine32("  [phase] final free space", INST_COLOR_TITLE);
        InstallDumpBPB(&Boot, Info.PartitionStartLBA);

        /* Compile every /BIN/*.C into a /BIN/*.EXE once here at install
           time, so the installed OS boots straight from the executables that
           already exist on the disk (no recompile at boot or reboot). */
        TerminalPrintLine32("  [phase] one-time self-host build of /BIN", INST_COLOR_TITLE);
        ShellBootBuild();

        if (g_WriteFailures != 0U || Verify.Missing != 0U ||
            Verify.SizeMismatch != 0U)
        {
            TerminalPrintLine32("", INST_COLOR_FIELD);
            TerminalPrintLine32("INSTALL FAILED", INST_COLOR_BAD);
            TerminalWrite32("  Write failures : ", INST_COLOR_BAD);
            TerminalPrintHex64Compact(g_WriteFailures);
            TerminalPutChar32('\n', INST_COLOR_BAD);
            TerminalWrite32("  Missing files  : ", INST_COLOR_BAD);
            TerminalPrintHex64Compact(Verify.Missing);
            TerminalPutChar32('\n', INST_COLOR_BAD);
            TerminalWrite32("  Size mismatches: ", INST_COLOR_BAD);
            TerminalPrintHex64Compact(Verify.SizeMismatch);
            TerminalPutChar32('\n', INST_COLOR_BAD);
            DbgSerialWrite("INSTALL RESULT: FAILED\n");
            DbgSerialWrite("INSTALLER: returning FAILED (kernel will halt)\n");
            return INSTALLER_RESULT_FAILED;
        }
    }

    TerminalPrintLine32("", INST_COLOR_FIELD);
    TerminalPrintLine32("INSTALL SUCCESS", INST_COLOR_GOOD);
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
    TerminalPrintLine32("SYSTEM READY. REMOVE INSTALL MEDIA AND RESTART.", INST_COLOR_GOOD);
    TerminalPrintLine32("Host: limine-binary/limine bios-install esp.img", INST_COLOR_FIELD);
    TerminalPrintLine32("QEMU: qemu-system-x86_64 -hda esp.img -boot order=c -m 3 -serial stdio", INST_COLOR_FIELD);
    DbgSerialWrite("INSTALLER: returning SUCCEEDED (kernel will reboot)\n");
    return INSTALLER_RESULT_SUCCEEDED;
}
