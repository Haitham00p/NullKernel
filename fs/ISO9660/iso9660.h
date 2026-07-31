#ifndef ISO9660_H
#define ISO9660_H

#define PVDSector 16
#define PVDSizeSector 2048
#define PVDEndPVD 0xFF
#define PVDTypePVD 0x01
#define PVDIdOfPVD "CD001"

#include <stdint.h> 
#include <stdbool.h>

typedef struct
{
    uint8_t Type;
    char IdOfPVD[5];
    uint8_t Version;
} ISOPVD;

typedef struct
{
    uint32_t ExtentLBA;
    uint32_t DataLength;
    uint8_t Flags;
    uint8_t FileIdentifierLength;
    char FileIdentifier[256];
} ISODirRecord;

bool ISOInitialize(void);

bool ISOReadPVDSector(ISOPVD *PVD);

#endif