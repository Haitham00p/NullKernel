#include "block.h"
#include "drivers/storage/nvme/nvme.h"
#include "drivers/storage/ahci/ahci.h"
#include "drivers/storage/ide/ide.h"
#include "include/drivers.h"
#include "kernel/debug/debug.h"
#include "kernel/terminal/terminal.h"
#include <stdint.h>

#define BLOCK_COLOR_LABEL 0x00E8F0FFU
#define BLOCK_COLOR_VALUE 0x00D0D0D0U

static bool g_BlockReady;

static void BlockPrintBackend(const char *Label)
{
    uint64_t Sectors = BlockGetSectorCount();
    uint64_t Bytes = Sectors * BLOCK_SECTOR_SIZE;

    DbgInfo("Block device backend", (uintptr_t)BlockInitialize);

    TerminalWrite32("  [storage] backend       : ", BLOCK_COLOR_LABEL);
    TerminalPrintLine32(Label, BLOCK_COLOR_VALUE);
    TerminalWrite32("  [storage] target sectors : ", BLOCK_COLOR_LABEL);
    TerminalPrintHex64Compact(Sectors);
    TerminalPutChar32('\n', BLOCK_COLOR_VALUE);
    TerminalWrite32("  [storage] target bytes   : ", BLOCK_COLOR_LABEL);
    TerminalPrintHex64Compact(Bytes);
    TerminalPutChar32('\n', BLOCK_COLOR_VALUE);
}

bool BlockInitialize(void)
{
    g_BlockReady = false;

    if (NVMeInitialize())
    {
        g_BlockReady = true;
        BlockPrintBackend("NVMe");
        return true;
    }

    if (AHCIInitialize())
    {
        g_BlockReady = true;
        BlockPrintBackend("AHCI SATA");
        return true;
    }

    if (IDEInitialize())
    {
        if (IDESelectDisk())
        {
            g_BlockReady = true;
            BlockPrintBackend("legacy ATA/IDE");
            return true;
        }

        if (IDEIsATAPI())
        {
            g_BlockReady = true;
            BlockPrintBackend("ATAPI optical");
            return true;
        }
    }

    DbgWarn("Block device: no storage device available", (uintptr_t)BlockInitialize);
    return false;
}

bool BlockIsReady(void)
{
    return g_BlockReady;
}

bool BlockRead(uint64_t LBA, void *Buffer)
{
    if (!g_BlockReady || Buffer == 0)
    {
        DbgSerialWrite("[block] Read not ready\n");
        return false;
    }

    if (NVMeIsReady())
        return NVMeReadSector(LBA, Buffer);

    if (AHCIIsReady())
        return AHCIReadSector(LBA, Buffer);

    return IDEReadDiskSector((uint32_t)LBA, Buffer);
}

bool BlockWrite(uint64_t LBA, const void *Buffer)
{
    if (!g_BlockReady || Buffer == 0)
        return false;

    if (NVMeIsReady())
        return NVMeWriteSector(LBA, Buffer);

    if (AHCIIsReady())
        return AHCIWriteSector(LBA, Buffer);

    return IDEWriteDiskSector((uint32_t)LBA, Buffer);
}

uint64_t BlockGetSectorCount(void)
{
    if (!g_BlockReady)
        return 0;
    if (NVMeIsReady())
        return NVMeGetSectorCount();
    if (AHCIIsReady())
        return AHCIGetSectorCount();
    return IDEGetSectorCount();
}

static void block_driver_init(void)
{
    (void)BlockInitialize();
}

DRIVER_INIT(block_driver_init);