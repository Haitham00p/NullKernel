#include "nvme.h"

#include "arch/x86_64/pci/pci.h"
#include "include/drivers.h"
#include "kernel/debug/debug.h"
#include "lib/string/string.h"
#include "mm/pmm.h"

#define NVME_CLASS              0x01U
#define NVME_SUBCLASS           0x08U
#define NVME_PROGIF             0x02U

#define NVME_ADMIN_QD           16U
#define NVME_IO_QD              16U
#define NVME_PAGE_SIZE          4096ULL
#define NVME_TIMEOUT            1000000U

#define NVME_REG_CAP            0x00U
#define NVME_REG_CC             0x14U
#define NVME_REG_CSTS           0x1CU
#define NVME_REG_AQA            0x24U
#define NVME_REG_ASQ            0x28U
#define NVME_REG_ACQ            0x30U

#define NVME_CSTS_RDY           (1U << 0)
#define NVME_CSTS_CFS           (1U << 1)

#define NVME_CC_EN              (1U << 0)
#define NVME_CC_IOSQES_SHIFT    16U
#define NVME_CC_IOCQES_SHIFT    20U

#define NVME_OP_CREATE_IO_CQ    0x05U
#define NVME_OP_CREATE_IO_SQ    0x01U
#define NVME_OP_IDENTIFY        0x06U
#define NVME_OP_READ            0x02U
#define NVME_OP_WRITE           0x01U

#define NVME_IDENTIFY_NAMESPACE 0x00U

#define NVME_PCI_COMMAND        0x04U
#define NVME_PCI_BAR0           0x10U

typedef volatile struct
{
    uint32_t cdw0;
    uint32_t nsid;
    uint32_t rsvd2[2];
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} NVME_COMMAND;

typedef volatile struct
{
    uint32_t dw0;
    uint32_t dw1;
    uint32_t sq_head;
    uint32_t sq_id;
} NVME_COMPLETION;

static volatile uint8_t *g_Bar;
static uint64_t g_AdminSQPhysical;
static uint64_t g_AdminCQPhysical;
static uint64_t g_IOSQPhysical;
static uint64_t g_IOCQPhysical;
static uint64_t g_IdentifyPhysical;
static uint32_t g_DoorbellStride;
static uint16_t g_AdminSQTail;
static uint16_t g_AdminCQHead;
static uint8_t g_AdminCQPhase;
static uint16_t g_IOSQTail;
static uint16_t g_IOCQHead;
static uint8_t g_IOCQPhase;
static uint16_t g_CommandId;
static uint32_t g_NamespaceId;
static uint32_t g_SectorSize;
static bool g_Ready;

static uint32_t Reg32(uint32_t offset)
{
    return *(volatile uint32_t *)(g_Bar + offset);
}

static uint64_t Reg64(uint32_t offset)
{
    return *(volatile uint64_t *)(g_Bar + offset);
}

static void Write32(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(g_Bar + offset) = value;
}

static void Write64(uint32_t offset, uint64_t value)
{
    *(volatile uint64_t *)(g_Bar + offset) = value;
}

static volatile uint32_t *Doorbell(uint16_t queue, bool completion)
{
    uint32_t index = (uint32_t)queue * 2U + (completion ? 1U : 0U);
    return (volatile uint32_t *)(g_Bar + 0x1000U +
                                 index * g_DoorbellStride);
}

static void RingDoorbell(uint16_t queue, bool completion, uint32_t value)
{
    *Doorbell(queue, completion) = value;
}

static bool WaitReady(bool ready)
{
    uint32_t timeout = 0;

    while (timeout++ < NVME_TIMEOUT)
    {
        uint32_t csts = Reg32(NVME_REG_CSTS);

        if ((csts & NVME_CSTS_CFS) != 0)
            return false;

        if (((csts & NVME_CSTS_RDY) != 0) == ready)
            return true;

        __asm__ volatile("pause");
    }

    return false;
}

static uint64_t ReadBAR(uint8_t bus, uint8_t dev, uint8_t fn)
{
    uint32_t low = PciConfigRead32(bus, dev, fn, NVME_PCI_BAR0);
    uint64_t bar = (uint64_t)(low & 0xFFFFFFF0U);

    if ((low & 0x6U) == 0x4U)
    {
        uint32_t high = PciConfigRead32(bus, dev, fn,
                                        NVME_PCI_BAR0 + 4U);
        bar |= (uint64_t)high << 32;
    }

    return bar;
}

static bool FindController(uint8_t *outBus, uint8_t *outDev, uint8_t *outFn,
                           uint64_t *outBar)
{
    uint8_t bus, dev, fn;

    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (fn = 0; fn < 8; fn++)
            {
                uint16_t vendor = PciVendor(bus, dev, fn);
                uint32_t classreg;
                uint64_t bar;

                if (vendor == 0xFFFF)
                    continue;

                classreg = PciConfigRead32(bus, dev, fn, 0x08);

                if (((classreg >> 24) & 0xFFU) != NVME_CLASS ||
                    ((classreg >> 16) & 0xFFU) != NVME_SUBCLASS ||
                    ((classreg >> 8) & 0xFFU) != NVME_PROGIF)
                    continue;

                bar = ReadBAR(bus, dev, fn);
                if (bar == 0)
                    continue;

                *outBus = bus;
                *outDev = dev;
                *outFn = fn;
                *outBar = bar;
                return true;
            }
        }
    }

    return false;
}

static bool DisableController(void)
{
    uint32_t cc = Reg32(NVME_REG_CC);

    if ((cc & NVME_CC_EN) == 0)
        return WaitReady(false);

    cc &= ~NVME_CC_EN;
    Write32(NVME_REG_CC, cc);
    return WaitReady(false);
}

static bool SetupController(uint64_t cap)
{
    uint32_t mpsmin = (uint32_t)((cap >> 48) & 0xFU);
    uint32_t mpsmax = (uint32_t)((cap >> 52) & 0xFU);
    uint16_t mqes = (uint16_t)((cap & 0xFFFFU) + 1U);
    uint64_t cc;
    uint32_t aqa;

    /*
     * NullOS currently allocates 4 KiB pages. MPS=0 therefore means
     * the controller must accept a 4 KiB memory page size.
     */
    if (mpsmin > 0 || mpsmax < 0)
        return false;

    if (mqes < NVME_ADMIN_QD || mqes < NVME_IO_QD)
        return false;

    g_AdminSQPhysical = PmmAllocPage();
    g_AdminCQPhysical = PmmAllocPage();
    g_IOSQPhysical = PmmAllocPage();
    g_IOCQPhysical = PmmAllocPage();

    if (!g_AdminSQPhysical || !g_AdminCQPhysical ||
        !g_IOSQPhysical || !g_IOCQPhysical)
        return false;

    memset((void *)(uintptr_t)g_AdminSQPhysical, 0, NVME_PAGE_SIZE);
    memset((void *)(uintptr_t)g_AdminCQPhysical, 0, NVME_PAGE_SIZE);
    memset((void *)(uintptr_t)g_IOSQPhysical, 0, NVME_PAGE_SIZE);
    memset((void *)(uintptr_t)g_IOCQPhysical, 0, NVME_PAGE_SIZE);

    g_AdminSQTail = 0;
    g_AdminCQHead = 0;
    g_AdminCQPhase = 1;
    g_IOSQTail = 0;
    g_IOCQHead = 0;
    g_IOCQPhase = 1;
    g_CommandId = 1;

    aqa = ((NVME_ADMIN_QD - 1U) << 16) | (NVME_ADMIN_QD - 1U);
    Write32(NVME_REG_AQA, aqa);
    Write64(NVME_REG_ASQ, g_AdminSQPhysical);
    Write64(NVME_REG_ACQ, g_AdminCQPhysical);

    /*
     * IOSQES=6 means 64-byte submission entries.
     * IOCQES=4 means 16-byte completion entries.
     * MPS=0 selects the 4 KiB page size.
     */
    cc = (6ULL << NVME_CC_IOSQES_SHIFT) |
         (4ULL << NVME_CC_IOCQES_SHIFT) |
         NVME_CC_EN;

    Write32(NVME_REG_CC, (uint32_t)cc);
    return WaitReady(true);
}

static bool SubmitAdmin(NVME_COMMAND *command, NVME_COMPLETION *completion)
{
    NVME_COMMAND *sq;
    NVME_COMPLETION *cq;
    uint16_t cid;
    uint32_t timeout = 0;

    sq = (NVME_COMMAND *)(uintptr_t)g_AdminSQPhysical;
    cq = (NVME_COMPLETION *)(uintptr_t)g_AdminCQPhysical;

    cid = g_CommandId++;
    if (g_CommandId == 0)
        g_CommandId = 1;

    command->cdw0 = (command->cdw0 & 0x0000FFFFU) |
                    ((uint32_t)cid << 16);

    sq[g_AdminSQTail] = *command;
    g_AdminSQTail = (uint16_t)((g_AdminSQTail + 1U) % NVME_ADMIN_QD);
    RingDoorbell(0, false, g_AdminSQTail);

    while (timeout++ < NVME_TIMEOUT)
    {
        NVME_COMPLETION *entry = &cq[g_AdminCQHead];
        uint16_t status = (uint16_t)(entry->dw3 >> 17);
        uint8_t phase = (uint8_t)(entry->dw3 & 1U);

        if (phase == g_AdminCQPhase)
        {
            if (completion)
                *completion = *entry;

            g_AdminCQHead =
                (uint16_t)((g_AdminCQHead + 1U) % NVME_ADMIN_QD);

            if (g_AdminCQHead == 0)
                g_AdminCQPhase ^= 1U;

            RingDoorbell(0, true, g_AdminCQHead);
            return (status == 0);
        }

        __asm__ volatile("pause");
    }

    return false;
}

static bool IdentifyNamespace(void)
{
    NVME_COMMAND command;
    NVME_COMPLETION completion;
    volatile uint8_t *id;
    uint8_t flbas;
    uint8_t lbaf;
    uint8_t lbads;

    g_IdentifyPhysical = PmmAllocPage();
    if (!g_IdentifyPhysical)
        return false;

    memset((void *)(uintptr_t)g_IdentifyPhysical, 0, NVME_PAGE_SIZE);

    memset((void *)&command, 0, sizeof(command));
    command.cdw0 = NVME_OP_IDENTIFY;
    command.nsid = g_NamespaceId;
    command.prp1 = g_IdentifyPhysical;
    command.cdw10 = NVME_IDENTIFY_NAMESPACE;

    if (!SubmitAdmin(&command, &completion))
        return false;

    id = (volatile uint8_t *)(uintptr_t)g_IdentifyPhysical;
    flbas = id[26] & 0x0FU;
    if (flbas >= 16)
        return false;

    lbaf = id[128U + (uint32_t)flbas * 4U];
    lbads = id[130U + (uint32_t)flbas * 4U];

    if (lbads >= 32)
        return false;

    g_SectorSize = 1U << lbads;

    if (g_SectorSize != 512U)
        return false;

    return true;
}

static bool CreateIOQueues(void)
{
    NVME_COMMAND command;
    NVME_COMPLETION completion;

    g_IOSQPhysical = g_IOSQPhysical;
    g_IOCQPhysical = g_IOCQPhysical;

    memset((void *)&command, 0, sizeof(command));
    command.cdw0 = NVME_OP_CREATE_IO_CQ;
    command.prp1 = g_IOCQPhysical;
    command.cdw10 = 1U | ((NVME_IO_QD - 1U) << 16);
    command.cdw11 = 1U;

    if (!SubmitAdmin(&command, &completion))
        return false;

    memset((void *)&command, 0, sizeof(command));
    command.cdw0 = NVME_OP_CREATE_IO_SQ;
    command.prp1 = g_IOSQPhysical;
    command.cdw10 = 1U | ((NVME_IO_QD - 1U) << 16);
    command.cdw11 = 1U;

    return SubmitAdmin(&command, &completion);
}

static bool Transfer(uint64_t lba, void *buffer, bool write)
{
    NVME_COMMAND *sq;
    NVME_COMPLETION *cq;
    NVME_COMMAND command;
    uint64_t data;
    uint16_t cid;
    uint32_t timeout = 0;

    if (!g_Ready || !buffer || g_SectorSize != 512U)
        return false;

    if (lba > 0x0000FFFFFFFFFFFFULL)
        return false;

    data = PmmAllocPage();
    if (!data)
        return false;

    if (write)
        memcpy((void *)(uintptr_t)data, buffer, 512);

    sq = (NVME_COMMAND *)(uintptr_t)g_IOSQPhysical;
    cq = (NVME_COMPLETION *)(uintptr_t)g_IOCQPhysical;

    memset((void *)&command, 0, sizeof(command));
    cid = g_CommandId++;
    if (g_CommandId == 0)
        g_CommandId = 1;

    command.cdw0 = (write ? NVME_OP_WRITE : NVME_OP_READ) |
                   ((uint32_t)cid << 16);
    command.nsid = g_NamespaceId;
    command.prp1 = data;
    command.cdw10 = (uint32_t)lba;
    command.cdw11 = (uint32_t)(lba >> 32);
    command.cdw12 = 0;

    sq[g_IOSQTail] = command;
    g_IOSQTail = (uint16_t)((g_IOSQTail + 1U) % NVME_IO_QD);
    RingDoorbell(1, false, g_IOSQTail);

    while (timeout++ < NVME_TIMEOUT)
    {
        NVME_COMPLETION *entry = &cq[g_IOCQHead];
        uint16_t status = (uint16_t)(entry->dw3 >> 17);
        uint8_t phase = (uint8_t)(entry->dw3 & 1U);

        if (phase == g_IOCQPhase)
        {
            bool ok = (status == 0);

            g_IOCQHead =
                (uint16_t)((g_IOCQHead + 1U) % NVME_IO_QD);

            if (g_IOCQHead == 0)
                g_IOCQPhase ^= 1U;

            RingDoorbell(1, true, g_IOCQHead);

            if (ok && !write)
                memcpy(buffer, (void *)(uintptr_t)data, 512);

            PmmFreePage(data);
            return ok;
        }

        __asm__ volatile("pause");
    }

    PmmFreePage(data);
    return false;
}

bool NVMeInitialize(void)
{
    uint8_t bus, dev, fn;
    uint64_t bar;
    uint64_t cap;
    uint32_t dstrd;
    uint16_t command;

    g_Ready = false;
    g_Bar = 0;
    g_NamespaceId = 1;
    g_SectorSize = 0;

    if (!FindController(&bus, &dev, &fn, &bar))
        return false;

    command = (uint16_t)PciConfigRead32(bus, dev, fn, NVME_PCI_COMMAND);
    command |= 0x0006U;
    PciConfigWrite16(bus, dev, fn, NVME_PCI_COMMAND, command);

    g_Bar = (volatile uint8_t *)(uintptr_t)bar;
    cap = Reg64(NVME_REG_CAP);

    dstrd = (uint32_t)((cap >> 32) & 0xFU);
    g_DoorbellStride = 4U << dstrd;

    if (!DisableController())
        return false;

    if (!SetupController(cap))
        return false;

    if (!IdentifyNamespace())
        return false;

    if (!CreateIOQueues())
        return false;

    g_Ready = true;

    DbgInfo("NVMe controller initialized", (uintptr_t)NVMeInitialize);
    DbgInfo("NVMe namespace ready", (uintptr_t)g_NamespaceId);
    return true;
}

bool NVMeIsReady(void)
{
    return g_Ready;
}

bool NVMeReadSector(uint64_t LBA, void *Buffer)
{
    return Transfer(LBA, Buffer, false);
}

bool NVMeWriteSector(uint64_t LBA, const void *Buffer)
{
    return Transfer(LBA, (void *)Buffer, true);
}

static void nvme_driver_init(void)
{
    (void)NVMeInitialize();
}

DRIVER_INIT(nvme_driver_init);
