#include "idt.h"
#include "string.h"

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


    __asm__ volatile(
        "lidt %0"
        :
        : "m"(IDTP)
        : "memory"
    );
}
