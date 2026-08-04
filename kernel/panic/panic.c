#include "panic.h"
#include "kernel/time/time.h"

#include "drivers/video_framebuffer/framebuffer.h"
#include "kernel/terminal/terminal.h"
#include "include/kutil.h"

#include "arch/x86_64/power/pwr.h"
#include "arch/x86_64/interrupts/pic.h"
#include "arch/x86_64/interrupts/irq.h"

#include "../../lib/sound/sndbeep.h"


/* Ominous High-Tech Dark Blue / Cyan Palette */
#define COLOR_VOID_BG        0x00020614U  /* Deep Abyssal Blue-Black */
#define COLOR_BLUE_HEADER    0x000F2B5CU  /* Deep Cobalt Blue */
#define COLOR_BLUE_BORDER    0x000066CCU  /* Royal Cyber Blue */
#define COLOR_BLUE_GLOW      0x0000D9FFU  /* Electric Cyan / Neon Blue */
#define COLOR_TEXT_BRIGHT    0x00F0F8FFU  /* Crisp Ice White */
#define COLOR_TEXT_MUTED     0x004B6B94U  /* Muted Slate Blue */
#define COLOR_TEXT_WARN      0x0038BDF8U  /* Warning Electric Blue */
#define COLOR_HEX_VAL        0x00CBD5E1U  /* Pale Silver-Blue Address */
#define COLOR_LABEL          0x0060A5FAU  /* Bright Cyber Blue Label */

static uint32_t RngState = 0xDEADBEEFU;

static uint32_t PanicRandom(void)
{
    RngState = (RngState * 1664525U + 1013904223U);
    return RngState;
}

static void DrawGlitchNoise(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t count)
{
    if (w == 0 || h == 0) return;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t rx = x + (PanicRandom() % w);
        uint32_t ry = y + (PanicRandom() % h);
        uint32_t maxW = w - (rx - x);
        if (maxW == 0) continue;
        uint32_t rw = 2 + (PanicRandom() % 6);
        if (rw > maxW) rw = maxW;
        uint32_t col = (PanicRandom() % 2 == 0) ? COLOR_BLUE_GLOW : COLOR_BLUE_BORDER;
        FbFillRect32(rx, ry, rw, 1, col);
    }
}

static void DrawCornerBrackets(uint32_t screenW, uint32_t screenH)
{
    if (screenW < 30 || screenH < 30) return;
    uint32_t c = COLOR_BLUE_BORDER;
    /* Top Left */
    FbFillRect32(4, 4, 18, 2, c);
    FbFillRect32(4, 4, 2, 18, c);

    /* Top Right */
    FbFillRect32(screenW - 22, 4, 18, 2, c);
    FbFillRect32(screenW - 6, 4, 2, 18, c);

    /* Bottom Left */
    FbFillRect32(4, screenH - 6, 18, 2, c);
    FbFillRect32(4, screenH - 22, 2, 18, c);

    /* Bottom Right */
    FbFillRect32(screenW - 22, screenH - 6, 18, 2, c);
    FbFillRect32(screenW - 6, screenH - 24, 2, 18, c);
}

static void PanicPrintHex64Padded(uint64_t Value, uint32_t Color)
{
    static const char HexDigits[] = "0123456789ABCDEF";
    char Buf[19];
    Buf[0] = '0';
    Buf[1] = 'x';
    for (int i = 15; i >= 0; i--)
    {
        Buf[2 + (15 - i)] = HexDigits[(Value >> (i * 4)) & 0xFULL];
    }
    Buf[18] = '\0';
    TerminalWrite32(Buf, Color);
}

static void PanicPrintRegPair(const char *N1, uint64_t V1, const char *N2, uint64_t V2)
{
    TerminalWrite32("  ", COLOR_VOID_BG);
    TerminalWrite32(N1, COLOR_LABEL);
    TerminalWrite32(" : ", COLOR_BLUE_BORDER);
    PanicPrintHex64Padded(V1, COLOR_HEX_VAL);

    TerminalWrite32("    ", COLOR_VOID_BG);
    TerminalWrite32(N2, COLOR_LABEL);
    TerminalWrite32(" : ", COLOR_BLUE_BORDER);
    PanicPrintHex64Padded(V2, COLOR_HEX_VAL);
    TerminalPutChar32('\n', COLOR_VOID_BG);
}

void PanicKernel(const char *Reason, uintptr_t FaultAddress)
{   

    StartBeep(300, 1000);
    
    

    /* Mask keyboard interrupt and enable PIT timer for sleep countdown */
    PICMaskIRQ(IRQ_KEYBOARD);
    __asm__ volatile("sti");

    /* Read CPU Registers for clear debugging */
    uint64_t rsp = 0, rbp = 0, cr0 = 0, cr2 = 0, cr3 = 0, cr4 = 0, rflags = 0;
    uint64_t cs = 0, ds = 0, ss = 0;

    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("pushfq; popq %0" : "=r"(rflags));
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));

    uint32_t screenW = FbGetWidth32();
    uint32_t screenH = FbGetHeight32();

    /* Render Void Background */
    TerminalClear32(COLOR_VOID_BG);

    /* Header Bar & Borders */
    if (screenW >= 100)
    {
        FbFillRect32(0, 0, screenW, 48, COLOR_BLUE_HEADER);
        FbFillRect32(0, 48, screenW, 3, COLOR_BLUE_BORDER);
        FbFillRect32(0, 51, screenW, 1, COLOR_BLUE_GLOW);

        if (screenH > 100)
        {
            FbFillRect32(0, screenH - 6, screenW, 6, COLOR_BLUE_HEADER);
            FbFillRect32(0, screenH - 2, screenW, 2, COLOR_BLUE_GLOW);
        }

        DrawCornerBrackets(screenW, screenH);
        DrawGlitchNoise(0, 0, screenW, 48, 15);
    }

    /* Header Title Text */
    TerminalSetCursor32(20, 10);
    TerminalPrintLine32("[!] KERNEL PANIC :: SYSTEM ANOMALY DETECTED [!]", COLOR_BLUE_GLOW);
    TerminalSetCursor32(20, 28);
    TerminalPrintLine32("The OS encountered a fatal error and halted execution to prevent corruption.", COLOR_TEXT_BRIGHT);

    /* Scaled Sad Face ;( rendered right after title, before debugging texts */
    TerminalWriteScaled32(";(", 20, 58, 3, COLOR_BLUE_GLOW);

    /* Diagnostic Section - Easy to Understand (Debugging Texts) */
    TerminalSetCursor32(20, 116);
    TerminalPrintLine32("=== CRASH SUMMARY ===", COLOR_BLUE_BORDER);

    TerminalSetCursor32(20, 134);
    TerminalWrite32("  CRASH REASON   : ", COLOR_LABEL);
    TerminalPrintLine32(Reason ? Reason : "UNKNOWN_MEMORY_VIOLATION", COLOR_TEXT_WARN);

    TerminalSetCursor32(20, 150);
    TerminalWrite32("  FAULT LOCATION : ", COLOR_LABEL);
    PanicPrintHex64Padded(FaultAddress, COLOR_HEX_VAL);
    TerminalWrite32("  (PANIC ENTRY: ", COLOR_TEXT_MUTED);
    PanicPrintHex64Padded((uintptr_t)PanicKernel, COLOR_TEXT_MUTED);
    TerminalPrintLine32(")", COLOR_TEXT_MUTED);

    TerminalNewLine32(COLOR_VOID_BG);
    TerminalPrintLine32("=== CPU REGISTERS FORENSICS ===", COLOR_BLUE_BORDER);

    PanicPrintRegPair("RIP (Instruction Ptr)", FaultAddress, "CR0 (Control Register)", cr0);
    PanicPrintRegPair("RSP (Stack Pointer)  ", rsp,          "CR2 (Page Fault Addr) ", cr2);
    PanicPrintRegPair("RBP (Base Pointer)   ", rbp,          "CR3 (Page Table Base) ", cr3);
    PanicPrintRegPair("RFL (Flags Register) ", rflags,       "CR4 (OS Extensions)   ", cr4);
    PanicPrintRegPair("CS  (Code Segment)   ", cs,           "DS  (Data Segment)    ", ds);

    TerminalNewLine32(COLOR_VOID_BG);
    TerminalPrintLine32("=== AUTOMATIC SYSTEM ACTION ===", COLOR_BLUE_BORDER);
    TerminalPrintLine32("  Hardware interrupts suspended. Memory state locked.", COLOR_TEXT_MUTED);
    TerminalPrintLine32("  Auto-reboot sequence initiated via hardware reset controller.", COLOR_TEXT_BRIGHT);

    TerminalNewLine32(COLOR_VOID_BG);
    TerminalWrite32("AUTO-REBOOTING SYSTEM IN: ", COLOR_BLUE_GLOW);
    StartBeep(100, 2000);
    KernelSleep(200);
    /* Use KernelSleep for countdown */
    for (uint16_t i = 10; i > 0; i--)
    {
        TerminalWriteUInt64(i, COLOR_TEXT_BRIGHT);
        TerminalWrite32(".. ", COLOR_BLUE_GLOW);
        KernelSleep(1000);
    }

    TerminalNewLine32(COLOR_VOID_BG);
    TerminalPrintLine32("[ REBOOTING NOW... ]", COLOR_BLUE_GLOW);
    KernelSleep(300);

    /* Trigger hardware reset via pwr.c Reboot() */
    Reboot();

    /* Fallback shutdown if reboot hardware port does not respond */
    Shutdown();

    while (1)
    {
        __asm__ volatile("cli; hlt");
    }
}