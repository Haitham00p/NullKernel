#include "ide.h"

#include "arch/x86_64/cpu/io.h"
#include "kernel/terminal/terminal.h"

#include <stdint.h>
#include <stdbool.h>

#define ATA_PRIMARY_IO         0x1F0
#define ATA_PRIMARY_CONTROL    0x3F6

#define ATA_REG_DATA           0
#define ATA_REG_ERROR          1
#define ATA_REG_FEATURES       1
#define ATA_REG_SECTOR_COUNT   2
#define ATA_REG_LBA0           3
#define ATA_REG_LBA1           4
#define ATA_REG_LBA2           5
#define ATA_REG_DRIVE_SELECT   6
#define ATA_REG_STATUS         7
#define ATA_REG_COMMAND        7

#define ATA_CMD_READ_PIO       0x20
#define ATA_CMD_WRITE_PIO      0x30
#define ATA_CMD_CACHE_FLUSH    0xE7
#define ATA_CMD_IDENTIFY       0xEC

#define ATA_STATUS_ERR         0x01
#define ATA_STATUS_DRQ         0x08
#define ATA_STATUS_DF          0x20
#define ATA_STATUS_DRDY        0x40
#define ATA_STATUS_BSY         0x80

#define ATA_MASTER             0xE0
#define ATA_SLAVE              0xF0

#define ATA_SECTOR_SIZE        512

static uint8_t IDEReadStatus(void){
    return Inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
}

static void IDEWaitBusy(void){
    while (IDEReadStatus() & ATA_STATUS_BSY){
        __asm__ volatile("pause");
    }
}

static void IDEWaitDRQ(void)
{
    while (!(IDEReadStatus() & ATA_STATUS_DRQ))
    {
        __asm__ volatile("pause");
    }
}

static void IDESelectDrive(bool Master){
    if (Master){
        Outb(ATA_PRIMARY_IO + ATA_REG_DRIVE_SELECT, ATA_MASTER);

    }
    if (!Master){
        Outb(ATA_PRIMARY_IO + ATA_REG_DRIVE_SELECT, ATA_SLAVE);
    }
}

static void IDESetSectorCount(uint8_t Count){
    Outb(ATA_PRIMARY_IO + ATA_REG_SECTOR_COUNT, Count);
}

static void IDESetLBA(uint32_t LBA){
    Outb(ATA_PRIMARY_IO + ATA_REG_LBA0, ((uint8_t)LBA));
    Outb(ATA_PRIMARY_IO + ATA_REG_LBA1, ((uint8_t)(LBA >> 8)));
    Outb(ATA_PRIMARY_IO + ATA_REG_LBA2, ((uint8_t)(LBA >> 16)));
}

static void IDESendCommand(uint8_t Command){
    Outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, Command);
}

static void IDEReadData(uint16_t *Buf, uint16_t Words){
    uint16_t *Data = (uint16_t *)Buf;
    for(uint16_t i = 0; i < Words; i++){
        Data[i] = Inw(ATA_PRIMARY_IO + ATA_REG_DATA);
    }
}

static bool IDEReturnIfError(){
    return (IDEReadStatus() & ATA_STATUS_ERR) != 0;
}

static bool IDEHasFault(){
    return (IDEReadStatus() & ATA_STATUS_DF) != 0;
}

bool IDEReadSector(
    uint32_t LBA,
    uint8_t *Buffer
){
    IDEWaitBusy();
    IDESelectDrive(true);
    IDESetLBA(LBA);
    IDESetSectorCount(1);
    IDESendCommand(ATA_CMD_READ_PIO);
    IDEWaitBusy();
    if (IDEReturnIfError()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Wrong LBA or Currupted Sector", 0xFFFFFFFF);
        return false;
    }
    if (IDEHasFault()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Driver Fault", 0xFFFFFFFF);
        return false;
    }
    IDEWaitDRQ();
    if (IDEReturnIfError()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Wrong LBA or Corrupted Sector", 0xFFFFFFFF);
        return false;
    }
    if (IDEHasFault()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Driver Fault", 0xFFFFFFFF);
        return false;
    }
    IDEReadData((uint16_t *)Buffer, 256);
    return true;
}

static void IDEWriteData(uint16_t *Buf, uint16_t Words){
    const uint16_t *Data = Buf;
    for (uint32_t i = 0; i < Words; i++){
        Outw(ATA_PRIMARY_IO + ATA_REG_DATA, Data[i]);
    }
}

bool IDEWriteSector(
    uint32_t LBA,
    const void *Buffer
){
    IDEWaitBusy();
    IDESelectDrive(true);
    IDESetLBA(LBA);
    IDESetSectorCount(1);
    IDESendCommand(ATA_CMD_WRITE_PIO);
    IDEWaitBusy();
    if (IDEReturnIfError()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Wrong LBA or Currupted Sector", 0xFFFFFFFF);
        return false;
    }
    if (IDEHasFault()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Driver Fault", 0xFFFFFFFF);
        return false;
    }
    IDEWaitDRQ();
    if (IDEReturnIfError()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Wrong LBA or Corrupted Sector", 0xFFFFFFFF);
        return false;
    }
    if (IDEHasFault()){
        TerminalWrite32("Kernel/ide/ide.c :", 0xFFFFFFFF);
        TerminalWrite32("Error :", 0xFFFF0000);
        TerminalPrintLine32("Driver Fault", 0xFFFFFFFF);
        return false;
    }
    IDEWriteData((uint16_t *)Buffer, 256);
    IDESendCommand(ATA_CMD_CACHE_FLUSH);
    IDEWaitBusy();
    return true;
}

static bool IDEID(bool Master, uint16_t *Buf){
    IDEWaitBusy();
    IDESelectDrive(Master);
    IDESendCommand(ATA_CMD_IDENTIFY);
    IDEWaitBusy();
    if (IDEReturnIfError()){
        return false;
    }
    if (IDEHasFault()){
        return false;
    }
    IDEWaitDRQ();
    if (IDEReturnIfError()){
        return false;
    }
    if (IDEHasFault()){
        return false;
    };
    IDEReadData(Buf, 256);
    return true;
}

static uint32_t IDEGetSectorCount(const uint16_t *IDEID){
    uint32_t SectorCount = 0;
    SectorCount |= (uint32_t)IDEID[60];
    SectorCount |= ((uint32_t)IDEID[61] << 16);
    return SectorCount;
}
