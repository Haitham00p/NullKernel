#include "panic.h"

#include "drivers/video_framebuffer/framebuffer.h"
#include "kernel/terminal/terminal.h"

#define PANIC_BACKGROUND 0x00000000
#define PANIC_HEADER     0x00B51D3AU
#define PANIC_ACCENT     0x00FF4D6DU
#define PANIC_TEXT       0x00F6F8FFU
#define PANIC_MUTED      0x009AA8BDU

static void PanicLine(const char *Label, const char *Value)
{
    TerminalWrite32(Label, PANIC_ACCENT);
    TerminalPrintLine32(Value, PANIC_TEXT);
}

void PanicKernel(const char *Reason, uintptr_t FaultAddress)
{
    __asm__ volatile("cli");

    TerminalClear32(PANIC_BACKGROUND);
    FbFillRect32(0, 0, FbGetWidth32(), 88, PANIC_HEADER);
    FbFillRect32(0, 88, FbGetWidth32(), 5, PANIC_ACCENT);

    TerminalSetCursor32(32, 16);
    TerminalPrintLine32("NULLOS KERNEL PANIC", PANIC_TEXT);
    TerminalSetCursor32(32, 48);
    TerminalPrintLine32("FATAL STOP  |  SYSTEM HALTED SAFELY", PANIC_TEXT);

    TerminalWriteScaled32(":(", 32, 116, 3, PANIC_ACCENT);

    TerminalSetCursor32(28, 184);
    TerminalNewLine32(PANIC_BACKGROUND);
    PanicLine("STOP CODE: ", "NULL_KERNEL_FATAL_ERROR");
    PanicLine("REASON:    ", Reason);
    TerminalWrite32("FAULT API: ", PANIC_ACCENT);
    TerminalPrintHex64Compact(FaultAddress);
    TerminalPutChar32('\n', PANIC_TEXT);
    TerminalWrite32("PANIC API: ", PANIC_ACCENT);
    TerminalPrintHex64Compact((uintptr_t)PanicKernel);
    TerminalPutChar32('\n', PANIC_TEXT);

    TerminalNewLine32(PANIC_BACKGROUND);
    TerminalPrintLine32("The CPU has stopped to prevent further corruption.", PANIC_MUTED);
    TerminalPrintLine32("Record the STOP CODE and fault address, then reboot.", PANIC_MUTED);
    TerminalPrintLine32("No disk writes are attempted while this screen is active.", PANIC_MUTED);
    TerminalPrintLine32("Use the address above to locate the failing kernel path.", PANIC_MUTED);
    TerminalNewLine32(PANIC_BACKGROUND);
    TerminalPrintLine32("DEBUG MODE: interrupts disabled | execution halted", PANIC_ACCENT);

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}
