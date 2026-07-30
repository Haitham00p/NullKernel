#include "irq.h"
#include "idt.h"
#include "pic.h"
#include "../terminal/terminal.h"
#include "keyboard.h"

void IRQHandler(uint32_t IRQ)
{
    if (IRQ == IRQ_KEYBOARD)
    {
        KeyboardInterrupt();
    }

    PICSendEOI((uint8_t)IRQ);
}

void IRQInitialize(void)
{
    /*
        Remap PIC

        IRQ0 -> IDT 32
        IRQ1 -> IDT 33
    */
    PICRemap(32, 40);



    /*
        Install IRQ gates
    */

    IDTSetGate(32, (uint64_t)IRQ0, 0x28, 0x8E);
    IDTSetGate(33, (uint64_t)IRQ1, 0x28, 0x8E);

    IDTSetGate(34, (uint64_t)IRQ2, 0x28, 0x8E);
    IDTSetGate(35, (uint64_t)IRQ3, 0x28, 0x8E);
    IDTSetGate(36, (uint64_t)IRQ4, 0x28, 0x8E);
    IDTSetGate(37, (uint64_t)IRQ5, 0x28, 0x8E);
    IDTSetGate(38, (uint64_t)IRQ6, 0x28, 0x8E);
    IDTSetGate(39, (uint64_t)IRQ7, 0x28, 0x8E);

    IDTSetGate(40, (uint64_t)IRQ8, 0x28, 0x8E);
    IDTSetGate(41, (uint64_t)IRQ9, 0x28, 0x8E);
    IDTSetGate(42, (uint64_t)IRQ10, 0x28, 0x8E);
    IDTSetGate(43, (uint64_t)IRQ11, 0x28, 0x8E);
    IDTSetGate(44, (uint64_t)IRQ12, 0x28, 0x8E);
    IDTSetGate(45, (uint64_t)IRQ13, 0x28, 0x8E);
    IDTSetGate(46, (uint64_t)IRQ14, 0x28, 0x8E);
    IDTSetGate(47, (uint64_t)IRQ15, 0x28, 0x8E);



    /*
        Enable only keyboard IRQ
    */
    PICMaskIRQ(IRQ_TIMER);
    PICUnmaskIRQ(IRQ_KEYBOARD);
}