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

#include "e1000.h"

#include "arch/x86_64/pci/pci.h"
#include "include/drivers.h"
#include "kernel/debug/debug.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

#define E1000_VENDOR_ID       0x8086U
#define E1000_DEVICE_ID       0x100EU

#define PCI_COMMAND           0x04U
#define PCI_BAR0              0x10U

#define REG_CTRL              0x0000U
#define REG_STATUS            0x0008U
#define REG_EERD              0x0014U
#define REG_ICR               0x00C0U
#define REG_IMS               0x00D0U

#define REG_RDBAL             0x2800U
#define REG_RDBAH             0x2804U
#define REG_RDLEN             0x2808U
#define REG_RDH               0x2810U
#define REG_RDT               0x2818U

#define REG_TDBAL             0x3800U
#define REG_TDBAH             0x3804U
#define REG_TDLEN             0x3808U
#define REG_TDH               0x3810U
#define REG_TDT               0x3818U

#define REG_RAL0              0x5400U
#define REG_RAH0              0x5404U

#define CTRL_RST              (1U << 26)
#define CTRL_SLU              (1U << 6)
#define STATUS_LU             (1U << 1)

#define EERD_START            (1U << 0)
#define EERD_DONE             (1U << 4)
#define EERD_ADDR_SHIFT       8U
#define EERD_DATA_SHIFT       16U

#define RCTL_EN               (1U << 1)
#define RCTL_BAM              (1U << 15)
#define RCTL_BSIZE_2048       (0U << 16)
#define RCTL_SECRC            (1U << 26)

#define TCTL_EN               (1U << 1)
#define TCTL_PSP              (1U << 3)
#define TCTL_CT_SHIFT         4U
#define TCTL_COLD_SHIFT       12U

#define RXD_STATUS_DD         (1U << 0)
#define RXD_STATUS_EOP        (1U << 1)

#define TXD_CMD_EOP           (1U << 0)
#define TXD_CMD_IFCS          (1U << 1)
#define TXD_CMD_RS            (1U << 3)
#define TXD_STATUS_DD         (1U << 0)

#define RING_COUNT             16U
#define RX_BUFFER_SIZE         2048U
#define MAX_ETH_FRAME          1518U
#define E1000_TIMEOUT          1000000U

typedef struct
{
    uint64_t Address;
    uint16_t Length;
    uint16_t Checksum;
    uint8_t Status;
    uint8_t Errors;
    uint16_t Special;
} __attribute__((packed)) E1000RxDesc;

typedef struct
{
    uint64_t Address;
    uint16_t Length;
    uint8_t Cso;
    uint8_t Command;
    uint8_t Status;
    uint8_t Css;
    uint16_t Special;
} __attribute__((packed)) E1000TxDesc;

static volatile uint8_t *g_Bar;
static uint64_t g_RxRingPhysical;
static uint64_t g_TxRingPhysical;
static uint64_t g_RxBufferPhysical[RING_COUNT];
static uint64_t g_TxBufferPhysical[RING_COUNT];
static uint16_t g_RxNext;
static uint16_t g_TxNext;
static uint8_t g_Mac[6];
static bool g_Ready;

static uint32_t Reg32(uint32_t Offset)
{
    return *(volatile uint32_t *)(g_Bar + Offset);
}

static void Write32(uint32_t Offset, uint32_t Value)
{
    *(volatile uint32_t *)(g_Bar + Offset) = Value;
}

static uint64_t ReadBAR(uint8_t Bus, uint8_t Dev, uint8_t Fn)
{
    uint32_t Low = PciConfigRead32(Bus, Dev, Fn, PCI_BAR0);
    uint64_t Bar = (uint64_t)(Low & 0xFFFFFFF0U);

    if ((Low & 0x6U) == 0x4U)
    {
        uint32_t High = PciConfigRead32(Bus, Dev, Fn, PCI_BAR0 + 4U);
        Bar |= (uint64_t)High << 32;
    }

    return Bar;
}

static bool FindDevice(uint8_t *Bus, uint8_t *Dev, uint8_t *Fn,
                       uint64_t *Bar)
{
    for (uint16_t B = 0; B < 256U; B++)
    {
        for (uint8_t D = 0; D < 32U; D++)
        {
            for (uint8_t F = 0; F < 8U; F++)
            {
                uint32_t Id = PciConfigRead32((uint8_t)B, D, F, 0x00);
                uint16_t Vendor = (uint16_t)(Id & 0xFFFFU);
                uint16_t Device = (uint16_t)(Id >> 16);

                if (Vendor != E1000_VENDOR_ID || Device != E1000_DEVICE_ID)
                    continue;

                *Bus = (uint8_t)B;
                *Dev = D;
                *Fn = F;
                *Bar = ReadBAR((uint8_t)B, D, F);
                return *Bar != 0;
            }
        }
    }

    return false;
}

static bool ResetDevice(void)
{
    uint32_t Ctrl = Reg32(REG_CTRL);
    uint32_t Timeout = 0;

    Write32(REG_CTRL, Ctrl | CTRL_RST);

    while (Timeout++ < E1000_TIMEOUT)
    {
        if ((Reg32(REG_CTRL) & CTRL_RST) == 0)
            return true;
        __asm__ volatile("pause");
    }

    return false;
}

static bool ReadEEPROM(uint8_t Address, uint16_t *Value)
{
    uint32_t Timeout = 0;

    if (!Value)
        return false;

    Write32(REG_EERD,
            EERD_START | ((uint32_t)Address << EERD_ADDR_SHIFT));

    while (Timeout++ < E1000_TIMEOUT)
    {
        uint32_t Eerd = Reg32(REG_EERD);

        if ((Eerd & EERD_DONE) != 0)
        {
            *Value = (uint16_t)(Eerd >> EERD_DATA_SHIFT);
            return true;
        }

        __asm__ volatile("pause");
    }

    return false;
}

static bool ReadMac(void)
{
    uint16_t Word[3];

    for (uint8_t I = 0; I < 3; I++)
    {
        if (!ReadEEPROM(I, &Word[I]))
            return false;

        g_Mac[I * 2] = (uint8_t)(Word[I] & 0xFFU);
        g_Mac[I * 2 + 1] = (uint8_t)(Word[I] >> 8);
    }

    return true;
}

static void ProgramMac(void)
{
    uint32_t Low = (uint32_t)g_Mac[0] |
                   ((uint32_t)g_Mac[1] << 8) |
                   ((uint32_t)g_Mac[2] << 16) |
                   ((uint32_t)g_Mac[3] << 24);

    uint32_t High = (uint32_t)g_Mac[4] |
                    ((uint32_t)g_Mac[5] << 8) |
                    (1U << 31);

    Write32(REG_RAL0, Low);
    Write32(REG_RAH0, High);
}

static bool AllocateRings(void)
{
    E1000RxDesc *Rx;
    E1000TxDesc *Tx;

    g_RxRingPhysical = PmmAllocPage();
    g_TxRingPhysical = PmmAllocPage();

    if (!g_RxRingPhysical || !g_TxRingPhysical)
        return false;

    Rx = (E1000RxDesc *)(uintptr_t)g_RxRingPhysical;
    Tx = (E1000TxDesc *)(uintptr_t)g_TxRingPhysical;

    for (uint16_t I = 0; I < RING_COUNT; I++)
    {
        g_RxBufferPhysical[I] = PmmAllocPage();
        g_TxBufferPhysical[I] = PmmAllocPage();

        if (!g_RxBufferPhysical[I] || !g_TxBufferPhysical[I])
            return false;

        Rx[I].Address = g_RxBufferPhysical[I];
        Rx[I].Length = 0;
        Rx[I].Checksum = 0;
        Rx[I].Status = 0;
        Rx[I].Errors = 0;
        Rx[I].Special = 0;

        Tx[I].Address = g_TxBufferPhysical[I];
        Tx[I].Length = 0;
        Tx[I].Cso = 0;
        Tx[I].Command = 0;
        Tx[I].Status = TXD_STATUS_DD;
        Tx[I].Css = 0;
        Tx[I].Special = 0;
    }

    return true;
}

static void SetupRx(void)
{
    Write32(REG_RDBAL, (uint32_t)g_RxRingPhysical);
    Write32(REG_RDBAH, (uint32_t)(g_RxRingPhysical >> 32));
    Write32(REG_RDLEN, RING_COUNT * (uint32_t)sizeof(E1000RxDesc));
    Write32(REG_RDH, 0);
    Write32(REG_RDT, RING_COUNT - 1U);

    Write32(0x0100U,
            RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);
}

static void SetupTx(void)
{
    Write32(REG_TDBAL, (uint32_t)g_TxRingPhysical);
    Write32(REG_TDBAH, (uint32_t)(g_TxRingPhysical >> 32));
    Write32(REG_TDLEN, RING_COUNT * (uint32_t)sizeof(E1000TxDesc));
    Write32(REG_TDH, 0);
    Write32(REG_TDT, 0);

    Write32(0x0400U,
            TCTL_EN | TCTL_PSP |
            (0x10U << TCTL_CT_SHIFT) |
            (0x40U << TCTL_COLD_SHIFT));
}

bool E1000Initialize(void)
{
    uint8_t Bus, Dev, Fn;
    uint64_t Bar;
    uint16_t Command;

    g_Ready = false;
    g_Bar = 0;
    g_RxNext = 0;
    g_TxNext = 0;

    if (!FindDevice(&Bus, &Dev, &Fn, &Bar))
        return false;

    Command = (uint16_t)PciConfigRead32(Bus, Dev, Fn, PCI_COMMAND);
    Command |= 0x0006U; /* Memory Space + Bus Master. */
    PciConfigWrite16(Bus, Dev, Fn, PCI_COMMAND, Command);

    /*
     * NullOS currently uses the same physical-MMIO access model as the
     * existing NVMe driver. If the VMM later gets a dedicated MMIO map,
     * only this BAR mapping needs to move behind that API.
     */
    /*
     * PCI BARs are device MMIO, not RAM. The kernel page tables are not
     * guaranteed to identity-map the E1000 BAR, so touching it directly can
     * raise a page fault and leave the boot screen black. Map the register
     * window at its physical address before the first MMIO access.
     */
    const uint64_t BarPage = Bar & ~0xFFFULL;
    const uint64_t BarOffset = Bar & 0xFFFULL;
    const uint64_t BarWindow = 0x20000ULL;
    for (uint64_t Offset = 0; Offset < BarWindow; Offset += 0x1000ULL)
    {
        if (!VmmMapPage(BarPage + Offset, BarPage + Offset, VMM_W | VMM_NX | VMM_PCD | VMM_PWT))
            return false;
    }
    g_Bar = (volatile uint8_t *)(uintptr_t)(BarPage + BarOffset);

    if (!ResetDevice())
        return false;

    if (!ReadMac())
        return false;

    ProgramMac();

    Write32(REG_CTRL, Reg32(REG_CTRL) | CTRL_SLU);

    if (!AllocateRings())
        return false;

    SetupRx();
    SetupTx();

    g_Ready = (Reg32(REG_STATUS) & STATUS_LU) != 0;

    DbgInfo("E1000 initialized", (uintptr_t)Bar);
    DbgInfo("E1000 MAC low", (uintptr_t)(
        (uint32_t)g_Mac[0] |
        ((uint32_t)g_Mac[1] << 8) |
        ((uint32_t)g_Mac[2] << 16) |
        ((uint32_t)g_Mac[3] << 24)));

    return g_Ready;
}

bool E1000IsReady(void)
{
    return g_Ready;
}

bool E1000GetMac(uint8_t Mac[6])
{
    if (!Mac || !g_Ready)
        return false;

    for (uint8_t I = 0; I < 6; I++)
        Mac[I] = g_Mac[I];

    return true;
}

bool E1000SendFrame(const void *Data, uint16_t Length)
{
    E1000TxDesc *Tx;
    uint16_t Index;
    uint32_t Timeout = 0;

    if (!g_Ready || !Data || Length == 0 || Length > MAX_ETH_FRAME)
        return false;

    Tx = (E1000TxDesc *)(uintptr_t)g_TxRingPhysical;
    Index = g_TxNext;

    while ((Tx[Index].Status & TXD_STATUS_DD) == 0)
    {
        if (Timeout++ >= E1000_TIMEOUT)
            return false;
        __asm__ volatile("pause");
    }

    for (uint16_t I = 0; I < Length; I++)
        ((uint8_t *)(uintptr_t)g_TxBufferPhysical[Index])[I] =
            ((const uint8_t *)Data)[I];

    Tx[Index].Length = Length;
    Tx[Index].Command = TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS;
    Tx[Index].Status = 0;

    g_TxNext = (uint16_t)((Index + 1U) % RING_COUNT);
    Write32(REG_TDT, g_TxNext);

    Timeout = 0;
    while ((Tx[Index].Status & TXD_STATUS_DD) == 0)
    {
        if (Timeout++ >= E1000_TIMEOUT)
            return false;
        __asm__ volatile("pause");
    }

    return true;
}

bool E1000ReceiveFrame(void *Buffer, uint16_t BufferSize, uint16_t *Length)
{
    E1000RxDesc *Rx;
    uint16_t Index;
    uint16_t Count;

    if (!g_Ready || !Buffer || !Length || BufferSize == 0)
        return false;

    Rx = (E1000RxDesc *)(uintptr_t)g_RxRingPhysical;
    Index = g_RxNext;

    if ((Rx[Index].Status & RXD_STATUS_DD) == 0)
        return false;

    if ((Rx[Index].Status & RXD_STATUS_EOP) == 0)
    {
        /* Multi-descriptor frames are intentionally deferred. */
        Rx[Index].Status = 0;
        Write32(REG_RDT, Index);
        g_RxNext = (uint16_t)((Index + 1U) % RING_COUNT);
        return false;
    }

    Count = Rx[Index].Length;
    if (Count > BufferSize)
        Count = BufferSize;

    for (uint16_t I = 0; I < Count; I++)
        ((uint8_t *)Buffer)[I] =
            ((const uint8_t *)(uintptr_t)g_RxBufferPhysical[Index])[I];

    *Length = Count;

    Rx[Index].Status = 0;
    Rx[Index].Length = 0;

    Write32(REG_RDT, Index);
    g_RxNext = (uint16_t)((Index + 1U) % RING_COUNT);

    return true;
}

/* Polling is deliberate: IRQ support is a separate layer after TX/RX works.
 * The registry runs these in link order once the heap and VMM are up, which
 * is the first point where PmmAllocPage() and VmmMapPage() are usable. */
static void e1000_driver_init(void)
{
    if (E1000IsReady())
        return;

    (void)E1000Initialize();
}

DRIVER_INIT(e1000_driver_init);
