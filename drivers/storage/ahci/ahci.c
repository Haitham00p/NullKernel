#include "ahci.h"
#include "arch/x86_64/pci/pci.h"
#include "kernel/debug/debug.h"
#include "lib/string/string.h"
#include "mm/pmm.h"

#include <stdbool.h>
#include <stdint.h>

#define AHCI_CLASS 0x01U
#define AHCI_SUBCLASS 0x06U
#define AHCI_PROGIF 0x01U

#define AHCI_GHC_AE (1U << 31)
#define AHCI_GHC_HR (1U << 0)

#define AHCI_PXCMD_ST (1U << 0)
#define AHCI_PXCMD_FRE (1U << 4)
#define AHCI_PXCMD_FR (1U << 14)
#define AHCI_PXCMD_CR (1U << 15)

#define AHCI_PXIS_TFES (1U << 30)

#define AHCI_PxSSTS_DET_MASK 0x0FU
#define AHCI_PxSSTS_IPM_MASK 0x0F00U
#define AHCI_PxSSTS_DET_PRESENT 0x03U
#define AHCI_PxSSTS_IPM_ACTIVE 0x100U

#define ATA_CMD_READ_DMA_EXT 0x25U
#define ATA_CMD_WRITE_DMA_EXT 0x35U

#define AHCI_TIMEOUT 1000000U

typedef volatile struct
{
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
} HBA_PORT;

typedef volatile struct
{
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t reserved[0x100 - 0x2C];
    uint8_t vendor[0x100];
    HBA_PORT ports[32];
} HBA_MEM;

typedef struct __attribute__((packed))
{
    uint8_t fis_type;
    uint8_t pmport_c;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0, lba1, lba2, device;
    uint8_t lba3, lba4, lba5, featureh;
    uint8_t countl, counth, icc, control;
    uint8_t reserved[4];
} FIS_REG_H2D;

typedef struct __attribute__((packed))
{
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc_i;
} HBA_PRDT;

typedef struct __attribute__((packed))
{
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    HBA_PRDT prdt[1];
} HBA_CMD_TBL;

typedef struct __attribute__((packed))
{
    uint32_t flags_prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} HBA_CMD_HEADER;

static HBA_MEM *g_HBA;
static HBA_PORT *g_Port;
static uint64_t g_PortPhysical;
static uint64_t g_ClbPhysical;
static uint64_t g_FisPhysical;
static uint64_t g_CmdTablePhysical;
static bool g_Ready;

static uint32_t ReadClass(uint8_t bus, uint8_t dev, uint8_t fn)
{
    return PciConfigRead32(bus, dev, fn, 0x08);
}

static uint64_t ReadABAR(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t low = PciConfigRead32(bus, dev, fn, 0x24);
    uint64_t bar = (uint64_t)(low & 0xFFFFFFF0U);

    if ((low & 0x6U) == 0x4U)
    {
        uint32_t high = PciConfigRead32(bus, dev, fn, 0x28);
        bar |= (uint64_t)high << 32;
    }

    return bar;
}

static void StopPort(HBA_PORT *port)
{
    uint32_t timeout = 0;

    port->cmd &= ~AHCI_PXCMD_ST;
    port->cmd &= ~AHCI_PXCMD_FRE;

    while ((port->cmd & (AHCI_PXCMD_CR | AHCI_PXCMD_FR)) != 0 &&
           timeout++ < AHCI_TIMEOUT)
    {
        __asm__ volatile("pause");
    }
}

static bool StartPort(HBA_PORT *port)
{
    uint32_t timeout = 0;

    while ((port->cmd & AHCI_PXCMD_CR) != 0 && timeout++ < AHCI_TIMEOUT)
        __asm__ volatile("pause");

    if (port->cmd & AHCI_PXCMD_CR)
        return false;

    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
    return true;
}

static int FindPort(void)
{
    uint32_t pi = g_HBA->pi;

    for (int i = 0; i < 32; i++)
    {
        if (!(pi & (1U << i)))
            continue;

        uint32_t ssts = g_HBA->ports[i].ssts;
        uint32_t det = ssts & AHCI_PxSSTS_DET_MASK;
        uint32_t ipm = ssts & AHCI_PxSSTS_IPM_MASK;

        if (det == AHCI_PxSSTS_DET_PRESENT &&
            ipm == AHCI_PxSSTS_IPM_ACTIVE)
            return i;
    }

    return -1;
}

static bool SetupPort(HBA_PORT *port)
{
    uint64_t clb = PmmAllocPage();
    uint64_t fis = PmmAllocPage();
    uint64_t table = PmmAllocPage();

    if (!clb || !fis || !table)
        return false;

    memset((void *)(uintptr_t)clb, 0, 4096);
    memset((void *)(uintptr_t)fis, 0, 4096);
    memset((void *)(uintptr_t)table, 0, 4096);

    StopPort(port);

    port->clb = (uint32_t)clb;
    port->clbu = (uint32_t)(clb >> 32);
    port->fb = (uint32_t)fis;
    port->fbu = (uint32_t)(fis >> 32);
    port->serr = 0xFFFFFFFFU;
    port->is = 0xFFFFFFFFU;

    HBA_CMD_HEADER *headers = (HBA_CMD_HEADER *)(uintptr_t)clb;
    headers[0].flags_prdtl = 5U | (1U << 16);
    headers[0].ctba = (uint32_t)table;
    headers[0].ctbau = (uint32_t)(table >> 32);

    g_ClbPhysical = clb;
    g_FisPhysical = fis;
    g_CmdTablePhysical = table;

    return StartPort(port);
}

static int FindSlot(void)
{
    uint32_t slots = g_Port->sact | g_Port->ci;

    for (int i = 0; i < 32; i++)
    {
        if (!(slots & (1U << i)))
            return i;
    }

    return -1;
}

static bool Transfer(uint64_t lba, void *buffer, bool write)
{
    int slot;
    HBA_CMD_HEADER *header;
    HBA_CMD_TBL *table;
    FIS_REG_H2D *fis;
    uint64_t data;
    uint32_t timeout = 0;

    if (!g_Ready || buffer == 0 || lba > 0x0000FFFFFFFFFFFFULL)
        return false;

    slot = FindSlot();
    if (slot < 0)
        return false;

    /*
     * DMA uses a page allocated by the PMM. The current VMM keeps the
     * low physical mapping accessible to kernel code, so the physical
     * address can also be used as the temporary virtual address here.
     */
    data = PmmAllocPage();
    if (!data)
        return false;

    if (write)
        memcpy((void *)(uintptr_t)data, buffer, 512);

    header = (HBA_CMD_HEADER *)(uintptr_t)(g_ClbPhysical +
             (uint64_t)slot * sizeof(HBA_CMD_HEADER));
    table = (HBA_CMD_TBL *)(uintptr_t)g_CmdTablePhysical;
    memset(table, 0, 4096);

    header->flags_prdtl = 5U | (write ? (1U << 6) : 0U) | (1U << 16);
    header->prdbc = 0;
    header->ctba = (uint32_t)g_CmdTablePhysical;
    header->ctbau = (uint32_t)(g_CmdTablePhysical >> 32);

    table->prdt[0].dba = (uint32_t)data;
    table->prdt[0].dbau = (uint32_t)(data >> 32);
    table->prdt[0].dbc_i = 511U;

    fis = (FIS_REG_H2D *)table->cfis;
    memset(fis, 0, sizeof(*fis));
    fis->fis_type = 0x27;
    fis->pmport_c = 0x80;
    fis->command = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    fis->device = 0x40;
    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = 1;

    while ((g_Port->tfd & 0x88U) != 0 && timeout++ < AHCI_TIMEOUT)
        __asm__ volatile("pause");

    if (g_Port->tfd & 0x88U)
    {
        PmmFreePage(data);
        return false;
    }

    g_Port->is = 0xFFFFFFFFU;
    g_Port->ci = 1U << slot;

    timeout = 0;
    while ((g_Port->ci & (1U << slot)) != 0 && timeout++ < AHCI_TIMEOUT)
    {
        if (g_Port->is & AHCI_PXIS_TFES)
            break;
        __asm__ volatile("pause");
    }

    if ((g_Port->ci & (1U << slot)) != 0 || (g_Port->is & AHCI_PXIS_TFES))
    {
        PmmFreePage(data);
        return false;
    }

    if (!write)
        memcpy(buffer, (void *)(uintptr_t)data, 512);

    PmmFreePage(data);
    return true;
}

bool AHCIInitialize(void)
{
    uint8_t bus, dev, fn;
    uint32_t classreg;
    uint64_t abar = 0;

    g_Ready = false;

    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (fn = 0; fn < 8; fn++)
            {
                uint16_t vendor = PciVendor(bus, dev, fn);
                if (vendor == 0xFFFF)
                    continue;

                classreg = ReadClass(bus, dev, fn);
                if (((classreg >> 24) & 0xFFU) != AHCI_CLASS ||
                    ((classreg >> 16) & 0xFFU) != AHCI_SUBCLASS ||
                    ((classreg >> 8) & 0xFFU) != AHCI_PROGIF)
                    continue;

                abar = ReadABAR(bus, dev, fn);
                if (!abar)
                    continue;

                {
                    uint32_t command = PciConfigRead32(bus, dev, fn, 0x04);
                    command |= 0x00000006U;
                    PciConfigWrite32(bus, dev, fn, 0x04, command);
                }

                g_HBA = (HBA_MEM *)(uintptr_t)abar;
                g_HBA->ghc |= AHCI_GHC_AE;

                /*
                 * Keep controller reset conservative here. Firmware may already
                 * have active devices. We only take ownership of the SATA port.
                 */
                int portno = FindPort();
                if (portno < 0)
                    continue;

                g_Port = &g_HBA->ports[portno];
                g_PortPhysical = abar + 0x100 + (uint64_t)portno * 0x80;

                if (!SetupPort(g_Port))
                    continue;

                g_Ready = true;
                DbgInfo("AHCI SATA controller initialized", (uintptr_t)AHCIInitialize);
                DbgInfo("AHCI SATA port ready", (uintptr_t)portno);
                return true;
            }
        }
    }

    return false;
}

bool AHCIIsReady(void)
{
    return g_Ready;
}

bool AHCIReadSector(uint64_t LBA, void *Buffer)
{
    return Transfer(LBA, Buffer, false);
}

bool AHCIWriteSector(uint64_t LBA, const void *Buffer)
{
    return Transfer(LBA, (void *)Buffer, true);
}
