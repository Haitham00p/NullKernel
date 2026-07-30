

#include "pic.h"
#include "../cpu/io.h"

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

