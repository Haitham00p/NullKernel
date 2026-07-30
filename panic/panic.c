#include "panic.h"
#include "terminal.h"
#include <stdint.h>
#include "framebuffer.h"
#include "debug.h"

void PanicKernel(char *str, uintptr_t API){
    TerminalClear32(0x00000000);
    DbgErr("Kernel Panic!!!", (uintptr_t *)PanicKernel);
    DbgWarn("IRQ : Disabled", (uintptr_t *)DbgWarn);
    DbgWarn("ISR : Disabled", (uintptr_t *)DbgWarn);
    DbgWarn("PIC : Disabled", (uintptr_t *)DbgWarn);
    DbgWarn("IQT : Disabled", (uintptr_t *)DbgWarn);
    TerminalPrintLine32("Interrupt System Has Been Shutdown", 0xFFFFFFFF);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("REASON : ", 0xFF9900FF);
    TerminalPrintLine32(str, 0xFFFFFFFF);
    TerminalWrite32("LAST API CALLED : ", 0xFF9900FF);
    TerminalPrintHex64Compact(API);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("PANIC API ADDR : ", 0xFF9900FF);
    TerminalPrintHex64Compact((uintptr_t)PanicKernel);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("Please Contact The Developer And Explain as Much as You Can: ", 0xFFFFFFFF);
    TerminalPrintLine32("tomtofficer@gmail.com", 0xFFFFFFFF);
    __asm__ volatile ("cli");
    while(1){
        __asm__ volatile ("hlt");
    }
}