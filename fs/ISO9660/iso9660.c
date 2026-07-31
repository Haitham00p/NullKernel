#include "iso9660.h"
#include "../drivers/storage/ide/ide.h"
#include <stddef.h>

bool ISOReadSector(
    uint32_t Sector,
    uint8_t *Buffer
){
    uint32_t LBA = Sector * 4;
    for (uint8_t i = 0; i < 4; i++){
        uint32_t Buf = 512 * i;
        IDEReadSector(LBA + i, Buffer + Buf);
    }
    return true;
}

bool ISOReadPVDSector(ISOPVD *PVD)
{
    uint8_t Buffer[PVDSizeSector];

    if (PVD == NULL)
    {
        return false;
    }

    if (!ISOReadSector(PVDSector, Buffer))
    {
        return false;
    }

    PVD->Type = Buffer[0];

    for (uint8_t i = 0; i < 5; i++)
    {
        PVD->IdOfPVD[i] = Buffer[1 + i];
    }

    PVD->Version = Buffer[6];

    if (PVD->Type != PVDTypePVD)
    {
        return false;
    }

    for (uint8_t i = 0; i < 5; i++)
    {
        if (PVD->IdOfPVD[i] != PVDIdOfPVD[i])
        {
            return false;
        }
    }

    return true;
}