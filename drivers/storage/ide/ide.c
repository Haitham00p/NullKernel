#include "ide.h"
#include "arch/x86_64/cpu/io.h"
#include "kernel/terminal/terminal.h"
#include "kernel/debug/debug.h"
#include "lib/string/string.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ATA_PRIMARY_IO         0x1F0
#define ATA_PRIMARY_CONTROL    0x3F6
#define ATA_SECONDARY_IO       0x170
#define ATA_SECONDARY_CONTROL  0x376

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
#define ATAPI_CMD_IDENTIFY     0xA1
#define ATAPI_CMD_PACKET       0xA0

#define ATA_STATUS_ERR         0x01
#define ATA_STATUS_DRQ         0x08
#define ATA_STATUS_DF          0x20
#define ATA_STATUS_DRDY        0x40
#define ATA_STATUS_BSY         0x80

#define ATA_MASTER             0xE0
#define ATA_SLAVE              0xF0

#define ATA_TIMEOUT            100000

typedef struct {
    uint16_t IoBase;
    uint16_t ControlBase;
    uint8_t DriveSelect;
    bool Present;
    bool IsATAPI;
} IDEDevice;

static IDEDevice g_ActiveDevice;
static bool g_IDEInitialized = false;

static void IDEDelay400ns(uint16_t io_base)
{
    uint16_t alt_port = (io_base == ATA_PRIMARY_IO) ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL;
    Inb(alt_port);
    Inb(alt_port);
    Inb(alt_port);
    Inb(alt_port);
}

static uint8_t IDEReadStatusPort(uint16_t io_base)
{
    return Inb(io_base + ATA_REG_STATUS);
}

static bool IDEWaitBusyTimeout(uint16_t io_base)
{
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++)
    {
        uint8_t status = IDEReadStatusPort(io_base);
        if (status == 0xFF) return false;
        if (!(status & ATA_STATUS_BSY)) return true;
        __asm__ volatile("pause");
    }
    return false;
}

static bool IDEWaitDRQTimeout(uint16_t io_base)
{
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++)
    {
        uint8_t status = IDEReadStatusPort(io_base);
        if (status == 0xFF) return false;
        if (status & ATA_STATUS_ERR) return false;
        if (status & ATA_STATUS_DRQ) return true;
        __asm__ volatile("pause");
    }
    return false;
}

static void IDESelectDrivePort(uint16_t io_base, uint8_t drive_sel)
{
    Outb(io_base + ATA_REG_DRIVE_SELECT, drive_sel);
    IDEDelay400ns(io_base);
}

static bool IDEReadATAPISector(uint32_t lba, void *buffer)
{
    uint16_t io = g_ActiveDevice.IoBase;

    IDESelectDrivePort(io, g_ActiveDevice.DriveSelect);
    if (!IDEWaitBusyTimeout(io)) return false;

    Outb(io + ATA_REG_FEATURES, 0x00);
    Outb(io + ATA_REG_LBA1, 2048 & 0xFF);
    Outb(io + ATA_REG_LBA2, (2048 >> 8) & 0xFF);

    Outb(io + ATA_REG_COMMAND, ATAPI_CMD_PACKET);
    IDEDelay400ns(io);

    if (!IDEWaitBusyTimeout(io)) return false;
    if (!IDEWaitDRQTimeout(io)) return false;

    uint8_t packet[12];
    for (int i = 0; i < 12; i++) packet[i] = 0;
    packet[0] = 0x28; // READ 10
    packet[2] = (lba >> 24) & 0xFF;
    packet[3] = (lba >> 16) & 0xFF;
    packet[4] = (lba >> 8) & 0xFF;
    packet[5] = lba & 0xFF;
    packet[8] = 1; // 1 sector

    uint16_t *packet16 = (uint16_t *)packet;
    for (int i = 0; i < 6; i++)
    {
        Outw(io + ATA_REG_DATA, packet16[i]);
    }

    if (!IDEWaitBusyTimeout(io)) return false;
    if (!IDEWaitDRQTimeout(io)) return false;

    uint16_t *dest = (uint16_t *)buffer;
    for (int i = 0; i < 1024; i++)
    {
        dest[i] = Inw(io + ATA_REG_DATA);
    }

    return true;
}

static bool IDEReadATASector(uint32_t lba, void *buffer)
{
    uint16_t io = g_ActiveDevice.IoBase;

    IDESelectDrivePort(io, g_ActiveDevice.DriveSelect | ((lba >> 24) & 0x0F));
    if (!IDEWaitBusyTimeout(io)) return false;

    Outb(io + ATA_REG_SECTOR_COUNT, 1);
    Outb(io + ATA_REG_LBA0, (uint8_t)lba);
    Outb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    Outb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    Outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    IDEDelay400ns(io);

    if (!IDEWaitBusyTimeout(io)) return false;
    if (!IDEWaitDRQTimeout(io)) return false;

    uint16_t *dest = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++)
    {
        dest[i] = Inw(io + ATA_REG_DATA);
    }

    return true;
}

bool IDEReadSector(uint32_t LBA, void *Buffer)
{
    if (!g_IDEInitialized || Buffer == NULL) return false;

    if (g_ActiveDevice.IsATAPI)
    {
        return IDEReadATAPISector(LBA, Buffer);
    }
    else
    {
        return IDEReadATASector(LBA, Buffer);
    }
}

bool IDEWriteSector(uint32_t LBA, const void *Buffer)
{
    if (!g_IDEInitialized || Buffer == NULL || g_ActiveDevice.IsATAPI) return false;

    uint16_t io = g_ActiveDevice.IoBase;

    IDESelectDrivePort(io, g_ActiveDevice.DriveSelect | ((LBA >> 24) & 0x0F));
    if (!IDEWaitBusyTimeout(io)) return false;

    Outb(io + ATA_REG_SECTOR_COUNT, 1);
    Outb(io + ATA_REG_LBA0, (uint8_t)LBA);
    Outb(io + ATA_REG_LBA1, (uint8_t)(LBA >> 8));
    Outb(io + ATA_REG_LBA2, (uint8_t)(LBA >> 16));

    Outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    IDEDelay400ns(io);

    if (!IDEWaitBusyTimeout(io)) return false;
    if (!IDEWaitDRQTimeout(io)) return false;

    const uint16_t *src = (const uint16_t *)Buffer;
    for (int i = 0; i < 256; i++)
    {
        Outw(io + ATA_REG_DATA, src[i]);
    }

    Outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    IDEWaitBusyTimeout(io);
    return true;
}

static bool ProbeIDEDevice(uint16_t io_base, uint8_t drive_sel, IDEDevice *dev)
{
    dev->IoBase = io_base;
    dev->DriveSelect = drive_sel;
    dev->Present = false;
    dev->IsATAPI = false;

    IDESelectDrivePort(io_base, drive_sel);
    if (!IDEWaitBusyTimeout(io_base)) return false;

    Outb(io_base + ATA_REG_SECTOR_COUNT, 0);
    Outb(io_base + ATA_REG_LBA0, 0);
    Outb(io_base + ATA_REG_LBA1, 0);
    Outb(io_base + ATA_REG_LBA2, 0);
    Outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    IDEDelay400ns(io_base);

    uint8_t status = IDEReadStatusPort(io_base);
    if (status == 0 || status == 0xFF) return false;

    uint8_t lba1 = Inb(io_base + ATA_REG_LBA1);
    uint8_t lba2 = Inb(io_base + ATA_REG_LBA2);
    (void)lba1;
    (void)lba2;

    if (status & ATA_STATUS_ERR)
    {
        /* ERR after ATA IDENTIFY: could be ATAPI or an empty channel.
           Issue ATAPI IDENTIFY and require a real data transfer; the
           empty-channel ghost sets ERR without DRQ. */
        Outb(io_base + ATA_REG_COMMAND, ATAPI_CMD_IDENTIFY);
        IDEDelay400ns(io_base);

        if (!IDEWaitBusyTimeout(io_base)) return false;

        uint8_t atapi_status = IDEReadStatusPort(io_base);
        if (atapi_status != 0 && atapi_status != 0xFF &&
            !(atapi_status & ATA_STATUS_ERR) && IDEWaitDRQTimeout(io_base))
        {
            uint16_t buf[256];
            for (int i = 0; i < 256; i++) buf[i] = Inw(io_base + ATA_REG_DATA);
            (void)buf;
            dev->Present = true;
            dev->IsATAPI = true;
            return true;
        }
        return false;
    }

    if (IDEWaitDRQTimeout(io_base))
    {
        uint16_t buf[256];
        for (int i = 0; i < 256; i++) buf[i] = Inw(io_base + ATA_REG_DATA);
        (void)buf;
        dev->Present = true;
        dev->IsATAPI = false;
        return true;
    }

    return false;
}

bool IDEInitialize(void)
{
    static const uint16_t io_ports[2] = { ATA_PRIMARY_IO, ATA_SECONDARY_IO };
    static const uint8_t drive_sels[2] = { ATA_MASTER, ATA_SLAVE };

    IDEDevice dev;
    for (int p = 0; p < 2; p++)
    {
        for (int d = 0; d < 2; d++)
        {
            if (ProbeIDEDevice(io_ports[p], drive_sels[d], &dev))
            {
                if (dev.IsATAPI)
                {
                    g_ActiveDevice = dev;
                    g_IDEInitialized = true;
                    DbgInfo("IDE Driver: ATAPI CD-ROM drive detected and active", (uintptr_t)IDEInitialize);
                    return true;
                }
            }
        }
    }

    for (int p = 0; p < 2; p++)
    {
        for (int d = 0; d < 2; d++)
        {
            if (ProbeIDEDevice(io_ports[p], drive_sels[d], &dev))
            {
                g_ActiveDevice = dev;
                g_IDEInitialized = true;
                DbgInfo("IDE Driver: ATA Hard Disk drive detected and active", (uintptr_t)IDEInitialize);
                return true;
            }
        }
    }

    DbgWarn("IDE Driver: No IDE device responding", (uintptr_t)IDEInitialize);
    g_IDEInitialized = false;
    return false;
}

bool IDEIsATAPI(void)
{
    return g_IDEInitialized && g_ActiveDevice.IsATAPI;
}

bool IDESelectDisk(void)
{
    static const uint16_t io_ports[2] = { ATA_PRIMARY_IO, ATA_SECONDARY_IO };
    static const uint8_t drive_sels[2] = { ATA_MASTER, ATA_SLAVE };

    IDEDevice dev;
    for (int p = 0; p < 2; p++)
    {
        for (int d = 0; d < 2; d++)
        {
            if (ProbeIDEDevice(io_ports[p], drive_sels[d], &dev) && !dev.IsATAPI)
            {
                g_ActiveDevice = dev;
                g_IDEInitialized = true;
                DbgInfo("IDE Driver: switched to ATA hard disk", (uintptr_t)IDESelectDisk);
                return true;
            }
        }
    }

    DbgWarn("IDE Driver: no ATA hard disk found", (uintptr_t)IDESelectDisk);
    return false;
}

bool IDESelectCD(void)
{
    static const uint16_t io_ports[2] = { ATA_PRIMARY_IO, ATA_SECONDARY_IO };
    static const uint8_t drive_sels[2] = { ATA_MASTER, ATA_SLAVE };

    IDEDevice dev;
    for (int p = 0; p < 2; p++)
    {
        for (int d = 0; d < 2; d++)
        {
            if (ProbeIDEDevice(io_ports[p], drive_sels[d], &dev) && dev.IsATAPI)
            {
                g_ActiveDevice = dev;
                g_IDEInitialized = true;
                DbgInfo("IDE Driver: switched to ATAPI CD-ROM", (uintptr_t)IDESelectCD);
                return true;
            }
        }
    }

    DbgWarn("IDE Driver: no ATAPI CD-ROM found", (uintptr_t)IDESelectCD);
    return false;
}
