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
    /*
     * PITTicks only advances from the PIT IRQ0 handler, so a sleep is only
     * meaningful with interrupts enabled. Entering with IF clear (early boot,
     * the panic path) would otherwise spin on a hlt that can never wake.
     */
    uint64_t Flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(Flags) : : "memory");
    __asm__ volatile("sti" ::: "memory");

    uint64_t CurrentTicks = PITGetTicks();
    uint64_t NeededTicks = CurrentTicks + Ticks;
    while (CurrentTicks < NeededTicks){
        __asm__ volatile("hlt");
        CurrentTicks = PITGetTicks();
    }

    if ((Flags & 0x200) == 0)
        __asm__ volatile("cli" ::: "memory");
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