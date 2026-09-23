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