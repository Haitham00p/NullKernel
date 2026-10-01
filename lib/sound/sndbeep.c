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

#include "sndbeep.h"
#include "../../arch/x86_64/cpu/io.h"
#include "../../kernel/terminal/terminal.h"
#include "../../include/kterminal.h"
#include "../../kernel/time/time.h"

void StartBeep(uint64_t Freq, uint32_t Last){
    if (Freq < 19 || Freq > 1193182){
        TerminalWrite32("Error: ", 0xFFFF0000);
        TerminalWrite32("Invalid Frequency: ", 0xFFFFFFFF);
        TerminalWrite32("Should: 19 <= Frequency <= 1193182", PromColor);
        TerminalNewLine32(BackSpaceColor);
        return;
    }
    int8_t tmp;
    tmp = Inb(SPEAKER_PORT);
    tmp |= 0x03;
    Outb(SPEAKER_PORT, tmp);
    
    uint16_t Divisor = 1193182 / Freq;
    Outb(PIT_COMMAND, 0xB6);
    Outb(PIT_CHANNEL2, (uint8_t)Divisor);
    Outb(PIT_CHANNEL2, (uint8_t)(Divisor >> 8));

    
    KernelSleep(Last);
    tmp = Inb(SPEAKER_PORT);
    tmp &= ~0x03;
    Outb(SPEAKER_PORT, tmp);
}

void EndBeepEmrg(void){
    uint8_t tmp;
    tmp = Inb(SPEAKER_PORT);
    tmp &= ~0x03;
    Outb(SPEAKER_PORT, tmp);
}