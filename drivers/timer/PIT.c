#include "PIT.h"
#include "../../arch/x86_64/cpu/io.h"
#include "../Kernel/kernel/terminal/terminal.h"

volatile uint64_t PITTicks = 0;

void PITInitialize(uint32_t Frequency){
    uint16_t Divisor = 1193182 / Frequency;
    Outb(0x43, 0x36);
    Outb(0x40, (uint8_t)(Divisor));
    Outb(0x40, (uint8_t)(Divisor >> 8));
    
}

uint64_t PITGetTicks(void)
{
    return PITTicks;
}

void PITTick(void){
    PITTicks++;
}

void PITTickReset(void){
    PITTicks = 0;
}

void PITTest(void){
    uint64_t Last = PITGetTicks();
    while (1)
    {
        if (PITGetTicks() != Last)
        {
            Last = PITGetTicks();
            TerminalPrintHex64Compact(Last);
            TerminalNewLine32(PromColor);
        }
    }
}

void PITSleepTicks(uint64_t Ticks){
    uint64_t CurrentTicks = PITGetTicks();
    uint64_t NeededTicks = CurrentTicks + Ticks;
    while (CurrentTicks < NeededTicks){
        __asm__ volatile ("hlt");
        CurrentTicks = PITGetTicks();
    }
}

uint64_t PITGetMilliseconds(){
    uint64_t Ticks = PITGetTicks();
    uint64_t Ms = (Ticks * 1000) / Freq;
    return Ms;
}

void PITSleepMS(uint64_t Milliseconds){
    uint64_t Ticks = (Milliseconds * Freq) / 1000;
    PITSleepTicks(Ticks);
}