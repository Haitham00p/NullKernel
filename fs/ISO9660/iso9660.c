#include "iso9660.h"
#include "drivers/storage/ide/ide.h"
#include <stddef.h>
#include "lib/string/string.h"
#include "kernel/terminal/terminal.h"
#include "kernel/debug/debug.h"

static ISOPVD g_PVD;
static ISODirRecord g_RootDir;
static bool g_ISOInitialized = false;
static uint32_t g_ActualPVDSector = PVDSector;

bool ISOReadSector(uint32_t Sector, uint8_t *Buffer)
{
    /* Always read from the ATAPI CD-ROM device.  The installer selects the
       CD explicitly; this path never depends on which IDE device happens to
       be "active" (the block layer owns the hard disk). */
    if (IDEIsReadyCD())
    {
        return IDEReadCDSector(Sector, Buffer);
    }

    /* Fallback for boot-from-disk ISO payloads: ISO sectors are 2048 bytes,
       presented as four 512-byte disk sectors. */
    if (!IDEIsReadyDisk())
    {
        return false;
    }
    uint32_t LBA = Sector * 4;
    for (uint8_t i = 0; i < 4; i++)
    {
        uint32_t BufOffset = 512 * i;
        if (!IDEReadDiskSector(LBA + i, Buffer + BufOffset))
        {
            return false;
        }
    }
    return true;
}

bool ISOReadPVDSector(ISOPVD *PVD)
{
    uint8_t Buffer[PVDSizeSector];

    if (PVD == NULL)
        return false;

    /* Scan volume descriptors starting at sector 16 up to sector 31 */
    for (uint32_t sec = PVDSector; sec < PVDSector + 16; sec++)
    {
        if (!ISOReadSector(sec, Buffer))
        {
            continue;
        }

        /* Check for Volume Descriptor Set Terminator */
        if (Buffer[0] == PVDEndPVD)
        {
            break;
        }

        /* Check for Primary Volume Descriptor (Type == 0x01 and ID == "CD001") */
        if (Buffer[0] == PVDTypePVD)
        {
            bool idMatch = true;
            for (uint8_t i = 0; i < 5; i++)
            {
                if (Buffer[1 + i] != PVDIdOfPVD[i])
                {
                    idMatch = false;
                    break;
                }
            }

            if (idMatch)
            {
                PVD->Type = Buffer[0];
                for (uint8_t i = 0; i < 5; i++)
                {
                    PVD->IdOfPVD[i] = Buffer[1 + i];
                }
                PVD->IdOfPVD[5] = '\0';
                PVD->Version = Buffer[6];

                g_ActualPVDSector = sec;
                return true;
            }
        }
    }

    return false;
}

static bool ParseDirectoryRecord(const uint8_t *Record, ISODirRecord *DirRec)
{
    if (Record == NULL || DirRec == NULL)
        return false;

    uint8_t len = Record[0];
    if (len < 33)
        return false;

    DirRec->Length = len;
    memcpy(&DirRec->ExtentLBA, Record + 2, 4);
    memcpy(&DirRec->DataLength, Record + 10, 4);
    DirRec->Flags = Record[25];
    DirRec->FileIdentifierLength = Record[32];

    uint8_t nameLen = DirRec->FileIdentifierLength;
    if (nameLen == 1 && Record[33] == 0x00)
    {
        DirRec->FileIdentifier[0] = '.';
        DirRec->FileIdentifier[1] = '\0';
    }
    else if (nameLen == 1 && Record[33] == 0x01)
    {
        DirRec->FileIdentifier[0] = '.';
        DirRec->FileIdentifier[1] = '.';
        DirRec->FileIdentifier[2] = '\0';
    }
    else
    {
        uint8_t copyLen = nameLen < 255 ? nameLen : 255;
        for (uint8_t i = 0; i < copyLen; i++)
        {
            DirRec->FileIdentifier[i] = (char)Record[33 + i];
        }
        DirRec->FileIdentifier[copyLen] = '\0';
    }

    /* Rock Ridge (SUSP/RRIP): the system use area follows the file
       identifier on an even byte offset.  An "NM" entry overrides the
       ISO-9660 truncated identifier with the real, long name.  The dot
       and dot-dot entries never carry a replacement name. */
    if (!(nameLen == 1U && (Record[33] == 0x00 || Record[33] == 0x01)))
    {
        uint16_t suisOff =
            (uint16_t)((33U + (uint16_t)nameLen + 1U) & (uint16_t)~1U);
        uint8_t Buffer[256];
        uint8_t BufferLen = 0;

        while (suisOff + 4U <= len)
        {
            uint32_t sysLen = Record[suisOff + 2];
            if (sysLen < 4U || suisOff + sysLen > len)
            {
                break;
            }

            if (Record[suisOff] == 'N' && Record[suisOff + 1] == 'M')
            {
                uint32_t DataLen = sysLen - 5U;
                if (DataLen > 0U)
                {
                    uint32_t Copy = (uint32_t)(255U - BufferLen);
                    if (DataLen < Copy) Copy = DataLen;
                    for (uint32_t i = 0; i < Copy; i++)
                    {
                        Buffer[BufferLen++] = Record[suisOff + 5 + i];
                    }
                }

                /* flag bit 0: the name continues in the next NM entry */
                if ((Record[suisOff + 4] & 0x01U) == 0U)
                {
                    break;
                }
            }

            suisOff += sysLen;
        }

        if (BufferLen > 0U)
        {
            for (uint16_t i = 0; i < BufferLen; i++)
            {
                DirRec->FileIdentifier[i] = (char)Buffer[i];
            }
            DirRec->FileIdentifier[BufferLen] = '\0';
            DirRec->FileIdentifierLength = BufferLen;
        }
    }

    return true;
}

bool ISOParseDirectoryRecord(const uint8_t *Record, ISODirRecord *DirRec)
{
    return ParseDirectoryRecord(Record, DirRec);
}

static bool RootDir(ISOPVD *PVD, ISODirRecord *DirRec)
{
    (void)PVD;
    uint8_t Buffer[PVDSizeSector];

    if (!ISOReadSector(g_ActualPVDSector, Buffer))
        return false;

    const uint8_t *Root = Buffer + 156;
    return ParseDirectoryRecord(Root, DirRec);
}

bool ISOInitialize(void)
{
    /* Ensure IDE driver is initialized before sector reads */
    IDEInitialize();

    if (!ISOReadPVDSector(&g_PVD))
    {
        DbgWarn("ISO9660 : Invalid PVD Sector", (uintptr_t)ISOReadPVDSector);
        g_ISOInitialized = false;
        return false;
    }

    if (!RootDir(&g_PVD, &g_RootDir))
    {
        DbgErr("ISO9660 : Failed to read Root Directory", (uintptr_t)ISOInitialize);
        g_ISOInitialized = false;
        return false;
    }

    g_ISOInitialized = true;
    DbgInfo("ISO9660 : Initialized successfully", (uintptr_t)ISOInitialize);
    return true;
}

bool ISOIsReady(void)
{
    return g_ISOInitialized;
}

bool ISOGetRootDir(ISODirRecord *OutRoot)
{
    if (!g_ISOInitialized || OutRoot == NULL)
        return false;
    *OutRoot = g_RootDir;
    return true;
}

bool ISOReadDirectory(const ISODirRecord *Dir)
{
    if (Dir == NULL || Dir->ExtentLBA == 0)
        return false;

    uint8_t Buffer[PVDSizeSector];
    uint32_t TotalSectors = (Dir->DataLength + PVDSizeSector - 1) / PVDSizeSector;
    if (TotalSectors == 0) TotalSectors = 1;

    TerminalPrintLine32("--- ISO9660 Directory Listing ---", PromColor);

    uint32_t BytesProcessed = 0;
    for (uint32_t s = 0; s < TotalSectors && BytesProcessed < Dir->DataLength; s++)
    {
        if (!ISOReadSector(Dir->ExtentLBA + s, Buffer))
        {
            TerminalPrintLine32("ISO9660: Sector read error", 0xFFFF0000);
            return false;
        }

        uint32_t SectorOffset = 0;
        while (SectorOffset < PVDSizeSector && (BytesProcessed + SectorOffset) < Dir->DataLength)
        {
            const uint8_t *Record = Buffer + SectorOffset;
            uint8_t RecLen = Record[0];

            if (RecLen == 0)
            {
                break;
            }

            ISODirRecord Entry;
            if (ParseDirectoryRecord(Record, &Entry))
            {
                char CleanName[256];
                uint32_t n = 0;
                while (Entry.FileIdentifier[n] != '\0' && Entry.FileIdentifier[n] != ';')
                {
                    CleanName[n] = Entry.FileIdentifier[n];
                    n++;
                }
                CleanName[n] = '\0';

                if ((Entry.Flags & 0x02) != 0)
                {
                    TerminalWrite32("[DIR]  ", 0x0057DB92);
                }
                else
                {
                    TerminalWrite32("[FILE] ", 0x005AA9FF);
                }
                TerminalWrite32(CleanName, PromColor);
                TerminalWrite32("  (LBA: ", PromColor);
                TerminalPrintHex64Compact(Entry.ExtentLBA);
                TerminalWrite32(", Size: ", PromColor);
                TerminalPrintHex64Compact(Entry.DataLength);
                TerminalPrintLine32(" B)", PromColor);
            }

            SectorOffset += RecLen;
        }
        BytesProcessed += PVDSizeSector;
    }

    return true;
}

static bool ISOCompareName(const char *ISOFileName, const char *SearchName)
{
    char CleanISO[256];
    uint32_t len = 0;
    while (ISOFileName[len] != '\0' && ISOFileName[len] != ';' && len < 255)
    {
        CleanISO[len] = ISOFileName[len];
        len++;
    }
    CleanISO[len] = '\0';

    if (len > 0 && CleanISO[len - 1] == '.')
    {
        uint64_t slen = strlen(SearchName);
        if (slen > 0 && SearchName[slen - 1] != '.')
        {
            CleanISO[len - 1] = '\0';
        }
    }

    return strcasecmp(CleanISO, SearchName) == 0;
}

bool ISOFindFile(const ISODirRecord *Dir, const char *Name, ISODirRecord *OutRecord)
{
    if (Dir == NULL || Name == NULL || OutRecord == NULL || Dir->ExtentLBA == 0)
        return false;

    uint8_t Buffer[PVDSizeSector];
    uint32_t TotalSectors = (Dir->DataLength + PVDSizeSector - 1) / PVDSizeSector;
    if (TotalSectors == 0) TotalSectors = 1;

    uint32_t BytesProcessed = 0;
    for (uint32_t s = 0; s < TotalSectors && BytesProcessed < Dir->DataLength; s++)
    {
        if (!ISOReadSector(Dir->ExtentLBA + s, Buffer))
            return false;

        uint32_t SectorOffset = 0;
        while (SectorOffset < PVDSizeSector && (BytesProcessed + SectorOffset) < Dir->DataLength)
        {
            const uint8_t *Record = Buffer + SectorOffset;
            uint8_t RecLen = Record[0];

            if (RecLen == 0)
                break;

            ISODirRecord Entry;
            if (ParseDirectoryRecord(Record, &Entry))
            {
                if (ISOCompareName(Entry.FileIdentifier, Name))
                {
                    *OutRecord = Entry;
                    return true;
                }
            }

            SectorOffset += RecLen;
        }
        BytesProcessed += PVDSizeSector;
    }

    return false;
}

bool ISOFindPath(const char *Path, ISODirRecord *OutRecord)
{
    if (!g_ISOInitialized || Path == NULL || OutRecord == NULL)
        return false;

    if (Path[0] == '/' && Path[1] == '\0')
    {
        *OutRecord = g_RootDir;
        return true;
    }

    ISODirRecord CurrentDir = g_RootDir;
    char Segment[128];
    uint32_t SegmentIndex = 0;
    uint32_t i = 0;

    if (Path[0] == '/') i++;

    while (Path[i] != '\0')
    {
        if (Path[i] == '/')
        {
            if (SegmentIndex > 0)
            {
                Segment[SegmentIndex] = '\0';
                ISODirRecord NextRecord;
                if (!ISOFindFile(&CurrentDir, Segment, &NextRecord))
                    return false;
                CurrentDir = NextRecord;
                SegmentIndex = 0;
            }
        }
        else
        {
            if (SegmentIndex < sizeof(Segment) - 1)
            {
                Segment[SegmentIndex++] = Path[i];
            }
        }
        i++;
    }

    if (SegmentIndex > 0)
    {
        Segment[SegmentIndex] = '\0';
        return ISOFindFile(&CurrentDir, Segment, OutRecord);
    }

    *OutRecord = CurrentDir;
    return true;
}

bool ISOReadFile(const ISODirRecord *FileRecord, void *Buffer, uint32_t MaxSize, uint32_t *BytesRead)
{
    if (FileRecord == NULL || Buffer == NULL || BytesRead == NULL || FileRecord->ExtentLBA == 0)
        return false;

    uint32_t ReadLen = FileRecord->DataLength < MaxSize ? FileRecord->DataLength : MaxSize;
    uint32_t TotalSectors = (ReadLen + PVDSizeSector - 1) / PVDSizeSector;

    uint8_t *Dest = (uint8_t *)Buffer;
    uint32_t Remaining = ReadLen;

    uint8_t SectorBuf[PVDSizeSector];
    for (uint32_t s = 0; s < TotalSectors; s++)
    {
        if (!ISOReadSector(FileRecord->ExtentLBA + s, SectorBuf))
        {
            *BytesRead = ReadLen - Remaining;
            return false;
        }

        uint32_t Chunk = Remaining < PVDSizeSector ? Remaining : PVDSizeSector;
        memcpy(Dest + (s * PVDSizeSector), SectorBuf, Chunk);
        Remaining -= Chunk;
    }

    *BytesRead = ReadLen;
    return true;
}