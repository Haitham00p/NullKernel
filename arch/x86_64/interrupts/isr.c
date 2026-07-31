#include "isr.h"
#include "idt.h"

static const char *ISRExceptionMessages[] =
    {
        "Divide By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "Bound Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault"};

void ISRHandler(uint32_t InterruptNumber)
{
    if (InterruptNumber < 32)
    {
        

        __asm__ volatile("cli");

    for (;;)
    {
        __asm__ volatile("hlt");
    }
    }
}

void ISRInitialize(void)
{

    IDTSetGate(
        0,
        (uint64_t)ISR0,
        0x08,
        0x8E);

    IDTSetGate(
        14,
        (uint64_t)ISR14,
        0x08,
        0x8E);
}
