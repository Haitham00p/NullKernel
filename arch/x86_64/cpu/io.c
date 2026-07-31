#include "io.h"

uint8_t Inb(uint16_t port)
{
    uint8_t ret;

    __asm__ volatile(
        "inb %1, %0"
        : "=a"(ret)
        : "dN"(port)
    );

    return ret;
}


void Outb(uint16_t port, uint8_t value)
{
    __asm__ volatile(
        "outb %0, %1"
        :
        : "a"(value), "dN"(port)
    );
}

uint16_t Inw(uint16_t Port)
{
    uint16_t Result;

    __asm__ volatile(
        "inw %1, %0"
        : "=a"(Result)
        : "Nd"(Port)
    );

    return Result;
}

void Outw(uint16_t Port, uint16_t Value)
{
    __asm__ volatile(
        "outw %0, %1"
        :
        : "a"(Value), "Nd"(Port)
    );
}