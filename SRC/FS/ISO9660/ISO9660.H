#ifndef ISO9660_H
#define ISO9660_H

#define PVDSector 16
#define PVDSizeSector 2048
#define PVDEndPVD 0xFF
#define PVDTypePVD 0x01
#define PVDIdOfPVD "CD001"

#include <stdint.h> 
#include <stdbool.h>

extern uint32_t PromColor;

typedef struct
{
    uint8_t Type;
    char IdOfPVD[6];
    uint8_t Version;
} ISOPVD;

typedef struct
{
    uint8_t Length;
    uint32_t ExtentLBA;
    uint32_t DataLength;
    uint8_t Flags;
    uint8_t FileIdentifierLength;
    char FileIdentifier[256];
} ISODirRecord;

bool ISOInitialize(void);
bool ISOIsReady(void);
bool ISOReadSector(uint32_t Sector, uint8_t *Buffer);
bool ISOReadPVDSector(ISOPVD *PVD);
bool ISOGetRootDir(ISODirRecord *OutRoot);
bool ISOReadDirectory(const ISODirRecord *Root);
bool ISOFindFile(const ISODirRecord *Dir, const char *Name, ISODirRecord *OutRecord);
bool ISOFindPath(const char *Path, ISODirRecord *OutRecord);
bool ISOReadFile(const ISODirRecord *FileRecord, void *Buffer, uint32_t MaxSize, uint32_t *BytesRead);

#endif