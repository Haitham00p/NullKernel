#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct
{
    uint16_t OffsetLow;

    uint16_t Selector;

    uint8_t IST;

    uint8_t TypeAttributes;

    uint16_t OffsetMid;

    uint32_t OffsetHigh;

    uint32_t Reserved;

} __attribute__((packed)) IDT_ENTRY;


typedef struct
{
    uint16_t Limit;

    uint64_t Base;

} __attribute__((packed)) IDT_POINTER;


void IDTInitialize(void);

void IDTSetGate(uint8_t Vector,
                uint64_t Handler,
                uint16_t Selector,
                uint8_t Flags);

#endif
