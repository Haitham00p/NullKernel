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
    uint64_t SectorCount;
} IDEDevice;

static IDEDevice g_ActiveDevice;
static IDEDevice g_CDDevice;
static IDEDevice g_DiskDevice;
static bool g_HasCDDevice = false;
static bool g_HasDiskDevice = false;
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

static bool IDEReadATAPISectorOn(const IDEDevice *dev, uint32_t lba, void *buffer)
{
    uint16_t io = dev->IoBase;

    IDESelectDrivePort(io, dev->DriveSelect);
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

static bool IDEReadATASectorOn(const IDEDevice *dev, uint32_t lba, void *buffer)
{
    uint16_t io = dev->IoBase;

    IDESelectDrivePort(io, dev->DriveSelect | ((lba >> 24) & 0x0F));
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

static bool IDEWriteATASectorOn(const IDEDevice *dev, uint32_t lba, const void *buffer)
{
    uint16_t io = dev->IoBase;

    IDESelectDrivePort(io, dev->DriveSelect | ((lba >> 24) & 0x0F));
    if (!IDEWaitBusyTimeout(io)) return false;

    Outb(io + ATA_REG_SECTOR_COUNT, 1);
    Outb(io + ATA_REG_LBA0, (uint8_t)lba);
    Outb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    Outb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    Outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    IDEDelay400ns(io);

    if (!IDEWaitBusyTimeout(io)) return false;
    if (!IDEWaitDRQTimeout(io)) return false;

    const uint16_t *src = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++)
    {
        Outw(io + ATA_REG_DATA, src[i]);
    }

    Outb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    IDEWaitBusyTimeout(io);
    return true;
}

/* Parses the 512-byte IDENTIFY payload for the 48-bit sector count. */
static uint64_t IDEIdentifySectorCount(const uint16_t *identify)
{
    bool lba48;
    uint64_t count;

    if (identify == NULL)
    {
        return 0;
    }

    /* Word 83 bit 10: 48-bit addressing supported. */
    lba48 = (identify[83] & (1U << 10)) != 0;

    if (lba48)
    {
        count = (uint64_t)identify[100] |
                ((uint64_t)identify[101] << 16) |
                ((uint64_t)identify[102] << 32) |
                ((uint64_t)identify[103] << 48);
    }
    else
    {
        count = (uint64_t)identify[60] |
                ((uint64_t)identify[61] << 16);
    }

    return count;
}

bool IDEReadSector(uint32_t LBA, void *Buffer)
{
    if (!g_IDEInitialized || Buffer == NULL) return false;

    if (g_ActiveDevice.IsATAPI)
    {
        return IDEReadATAPISectorOn(&g_ActiveDevice, LBA, Buffer);
    }
    else
    {
        return IDEReadATASectorOn(&g_ActiveDevice, LBA, Buffer);
    }
}

bool IDEWriteSector(uint32_t LBA, const void *Buffer)
{
    if (!g_IDEInitialized || Buffer == NULL || g_ActiveDevice.IsATAPI) return false;
    return IDEWriteATASectorOn(&g_ActiveDevice, LBA, Buffer);
}

/* Block-layer entry points: ALWAYS the ATA hard disk, never the CD. */
bool IDEReadDiskSector(uint32_t LBA, void *Buffer)
{
    if (!g_IDEInitialized || !g_HasDiskDevice || Buffer == NULL) return false;
    return IDEReadATASectorOn(&g_DiskDevice, LBA, Buffer);
}

bool IDEWriteDiskSector(uint32_t LBA, const void *Buffer)
{
    if (!g_IDEInitialized || !g_HasDiskDevice || Buffer == NULL) return false;
    return IDEWriteATASectorOn(&g_DiskDevice, LBA, Buffer);
}

/* ISO9660 entry point: ALWAYS the ATAPI CD-ROM. */
bool IDEReadCDSector(uint32_t LBA, void *Buffer)
{
    if (!g_IDEInitialized || !g_HasCDDevice || Buffer == NULL) return false;
    return IDEReadATAPISectorOn(&g_CDDevice, LBA, Buffer);
}

uint64_t IDEGetSectorCount(void)
{
    if (!g_IDEInitialized || !g_HasDiskDevice)
    {
        return 0;
    }
    return g_DiskDevice.SectorCount;
}

bool IDEIsReadyCD(void)
{
    return g_IDEInitialized && g_HasCDDevice;
}

bool IDEIsReadyDisk(void)
{
    return g_IDEInitialized && g_HasDiskDevice;
}

static bool ProbeIDEDevice(uint16_t io_base, uint8_t drive_sel, IDEDevice *dev)
{
    uint16_t identify[256];

    dev->IoBase = io_base;
    dev->ControlBase = (io_base == ATA_PRIMARY_IO) ? ATA_PRIMARY_CONTROL
                                                   : ATA_SECONDARY_CONTROL;
    dev->DriveSelect = drive_sel;
    dev->Present = false;
    dev->IsATAPI = false;
    dev->SectorCount = 0;

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
            dev->SectorCount = 0;
            return true;
        }
        return false;
    }

    if (IDEWaitDRQTimeout(io_base))
    {
        for (int i = 0; i < 256; i++) identify[i] = Inw(io_base + ATA_REG_DATA);
        dev->Present = true;
        dev->IsATAPI = false;
        dev->SectorCount = IDEIdentifySectorCount(identify);
        return true;
    }

    return false;
}

bool IDEInitialize(void)
{
    static const uint16_t io_ports[2] = { ATA_PRIMARY_IO, ATA_SECONDARY_IO };
    static const uint8_t drive_sels[2] = { ATA_MASTER, ATA_SLAVE };

    IDEDevice dev;

    g_HasCDDevice = false;
    g_HasDiskDevice = false;

    /* Probe once and cache every device we find.  The live installer must
       be able to switch back to the boot CD after the block layer selects
       the ATA hard disk; re-probing an ATAPI device after ATA traffic can
       be unreliable on some virtual/real IDE controllers. */
    for (int p = 0; p < 2; p++)
    {
        for (int d = 0; d < 2; d++)
        {
            if (!ProbeIDEDevice(io_ports[p], drive_sels[d], &dev))
                continue;

            if (dev.IsATAPI && !g_HasCDDevice)
            {
                g_CDDevice = dev;
                g_HasCDDevice = true;
            }
            else if (!dev.IsATAPI && !g_HasDiskDevice)
            {
                g_DiskDevice = dev;
                g_HasDiskDevice = true;
            }
        }
    }

    if (g_HasCDDevice)
    {
        g_ActiveDevice = g_CDDevice;
        g_IDEInitialized = true;
        DbgInfo("IDE Driver: ATAPI CD-ROM drive detected and active", (uintptr_t)IDEInitialize);
        return true;
    }

    if (g_HasDiskDevice)
    {
        g_ActiveDevice = g_DiskDevice;
        g_IDEInitialized = true;
        DbgInfo("IDE Driver: ATA Hard Disk drive detected and active", (uintptr_t)IDEInitialize);
        return true;
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
    if (g_HasDiskDevice)
    {
        g_ActiveDevice = g_DiskDevice;
        g_IDEInitialized = true;
        DbgInfo("IDE Driver: switched to ATA hard disk", (uintptr_t)IDESelectDisk);
        return true;
    }

    DbgWarn("IDE Driver: no ATA hard disk found", (uintptr_t)IDESelectDisk);
    return false;
}

bool IDESelectCD(void)
{
    if (g_HasCDDevice)
    {
        g_ActiveDevice = g_CDDevice;
        g_IDEInitialized = true;
        DbgInfo("IDE Driver: switched to ATAPI CD-ROM", (uintptr_t)IDESelectCD);
        return true;
    }

    DbgWarn("IDE Driver: no ATAPI CD-ROM found", (uintptr_t)IDESelectCD);
    return false;
}