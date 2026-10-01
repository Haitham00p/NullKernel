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

#include "idt.h"
#include "lib/string/string.h"
#include "arch/x86_64/cpu/gdt.h"

extern void Ring3Int80(void);
extern void TaskYieldTrap(void);

static IDT_ENTRY IDT[IDT_ENTRIES];
static IDT_POINTER IDTP;

void IDTSetGate(uint8_t Vector,
                uint64_t Handler,
                uint16_t Selector,
                uint8_t Flags)
{
    IDT[Vector].OffsetLow  = (uint16_t)(Handler & 0xFFFF);
    IDT[Vector].OffsetMid  = (uint16_t)((Handler >> 16) & 0xFFFF);
    IDT[Vector].OffsetHigh = (uint32_t)(Handler >> 32);
    IDT[Vector].Selector = Selector;
    IDT[Vector].IST = 0;
    IDT[Vector].TypeAttributes = Flags;
    IDT[Vector].Reserved = 0;
}

void IDTInitialize(void)
{
    memset(IDT, 0, sizeof(IDT));

    IDTP.Limit = sizeof(IDT_ENTRY) * IDT_ENTRIES - 1;
    IDTP.Base = (uint64_t)&IDT;

    /*
     * Ring 3 ABI: DPL3 software interrupt.
     * Ring 0 scheduler yield: DPL0 software interrupt.
     */
    IDTSetGate(0x80, (uint64_t)Ring3Int80, GdtKernelCode(), 0xEE);
    IDTSetGate(0x81, (uint64_t)TaskYieldTrap, GdtKernelCode(), 0x8E);

    __asm__ volatile(
        "lidt %0"
        :
        : "m"(IDTP)
        : "memory"
    );
}
