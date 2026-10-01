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



#include "pic.h"
#include "arch/x86_64/cpu/io.h"

void PICSendEOI(uint8_t IRQ)
{
    if (IRQ >= 8)
    {
        Outb(PIC2_COMMAND, PIC_EOI);
    }

    Outb(PIC1_COMMAND, PIC_EOI);
}

void PICRemap(uint8_t Offset1, uint8_t Offset2)
{
    uint8_t Mask1 = Inb(PIC1_DATA);
    uint8_t Mask2 = Inb(PIC2_DATA);

    Outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    Outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    Outb(PIC1_DATA, Offset1);
    Outb(PIC2_DATA, Offset2);

    Outb(PIC1_DATA, 4);
    Outb(PIC2_DATA, 2);

    Outb(PIC1_DATA, ICW4_8086);
    Outb(PIC2_DATA, ICW4_8086);

    Outb(PIC1_DATA, Mask1);
    Outb(PIC2_DATA, Mask2);
}

void PICMaskIRQ(uint8_t IRQ)
{
    uint16_t Port;
    uint8_t Value;

    if (IRQ < 8)
    {
        Port = PIC1_DATA;
    }
    else
    {
        Port = PIC2_DATA;
        IRQ -= 8;
    }

    Value = Inb(Port);

    Value |= (1 << IRQ);

    Outb(Port, Value);
}

void PICUnmaskIRQ(uint8_t IRQ)
{
    uint16_t Port;
    uint8_t Value;

    if (IRQ < 8)
    {
        Port = PIC1_DATA;
    }
    else
    {
        Port = PIC2_DATA;
        IRQ -= 8;
    }

    Value = Inb(Port);

    Value &= ~(1 << IRQ);

    Outb(Port, Value);


}

