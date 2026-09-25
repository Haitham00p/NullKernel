#include "block.h"
#include "drivers/storage/nvme/nvme.h"
#include "drivers/storage/ahci/ahci.h"
#include "drivers/storage/ide/ide.h"
#include "kernel/debug/debug.h"

static bool g_BlockReady;

bool BlockInitialize(void)
{
    g_BlockReady = false;

    if (NVMeInitialize())
    {
        g_BlockReady = true;
        DbgInfo("Block device: NVMe selected", (uintptr_t)BlockInitialize);
        return true;
    }

    if (AHCIInitialize())
    {
        g_BlockReady = true;
        DbgInfo("Block device: AHCI SATA selected", (uintptr_t)BlockInitialize);
        return true;
    }

    if (IDESelectDisk())
    {
        g_BlockReady = true;
        DbgInfo("Block device: legacy ATA/IDE selected", (uintptr_t)BlockInitialize);
        return true;
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
        return false;

    if (NVMeIsReady())
        return NVMeReadSector(LBA, Buffer);

    if (AHCIIsReady())
        return AHCIReadSector(LBA, Buffer);

    return IDEReadSector((uint32_t)LBA, Buffer);
}

bool BlockWrite(uint64_t LBA, const void *Buffer)
{
    if (!g_BlockReady || Buffer == 0)
        return false;

    if (NVMeIsReady())
        return NVMeWriteSector(LBA, Buffer);

    if (AHCIIsReady())
        return AHCIWriteSector(LBA, Buffer);

    return IDEWriteSector((uint32_t)LBA, Buffer);
}

uint64_t BlockGetSectorCount(void)
{
    if (!g_BlockReady)
        return 0;
    if (NVMeIsReady())
        return NVMeGetSectorCount();
    return 0;
}

static void block_driver_init(void)
{
    (void)BlockInitialize();
}
