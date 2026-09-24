#include "isr.h"

#include <stdint.h>

#include "arch/x86_64/interrupts/idt.h"
#include "include/diskfs.h"
#include "kernel/debug/debug.h"


static const char *ExceptionName(uint32_t Vector)
{
    switch (Vector)
    {
        case 0:  return "DIVIDE BY ZERO";
        case 6:  return "INVALID OPCODE";
        case 8:  return "DOUBLE FAULT";
        case 13: return "GENERAL PROTECTION FAULT";
        case 14: return "PAGE FAULT";
        default: return "EXCEPTION";
    }
}


/* Re-entrancy guard: if we fault while handling a fault (e.g. the diagnostic
   itself touches bad memory), do not let it escalate into a triple fault;
   just stop. */
static volatile bool g_InException = false;


static void PfPutChar(char **Cursor, char Character)
{
    **Cursor = Character;
    (*Cursor)++;
}


static void PfPutStr(char **Cursor, const char *String)
{
    while (*String != '\0')
    {
        PfPutChar(Cursor, *String);
        String++;
    }
}


static void PfPutHex64(char **Cursor, uint64_t Value)
{
    static const char HexDigits[] = "0123456789ABCDEF";
    int i;

    for (i = 15; i >= 0; i--)
    {
        PfPutChar(Cursor, HexDigits[(Value >> (i * 4)) & 0xF]);
    }
}


void ExceptionHandler(interrupt_frame_t *frame)
{
    uint64_t Cr2 = 0;

    uint64_t RspFault;
    static char Report[512];
    char *Cursor;
    uint32_t Len;

    DbgSerialWrite(">>> EXCEPTION HANDLER ENTERED <<<\n");

    if (g_InException)
    {
        DbgSerialWrite("RECURSIVE EXCEPTION - halting\n");
        for (;;)
        {
            __asm__ volatile("cli; hlt");
        }
    }
    g_InException = true;

    if (frame->vector == 14)
    {
        __asm__ volatile("mov %%cr2, %0" : "=r"(Cr2));
    }

    /* frame->rbx holds the faulting context's stack pointer (captured before
       the handler switched to its dedicated exception stack). */
    RspFault = frame->rbx;

    Cursor = Report;
    if (frame->vector == 14)
    {
        PfPutStr(&Cursor, "=== PAGE FAULT ===\n");
    }
    else
    {
        PfPutStr(&Cursor, "=== ");
        PfPutStr(&Cursor, ExceptionName(frame->vector));
        PfPutStr(&Cursor, " ===\n");
    }
    PfPutStr(&Cursor, "RIP   = ");
    PfPutHex64(&Cursor, frame->rip);
    PfPutChar(&Cursor, '\n');
    PfPutStr(&Cursor, "RSP   = ");
    PfPutHex64(&Cursor, RspFault);
    PfPutChar(&Cursor, '\n');
    PfPutStr(&Cursor, "RBP   = ");
    PfPutHex64(&Cursor, frame->rbp);
    PfPutChar(&Cursor, '\n');
    PfPutStr(&Cursor, "CR2   = ");
    PfPutHex64(&Cursor, Cr2);
    PfPutChar(&Cursor, '\n');
    PfPutStr(&Cursor, "ERROR = ");
    PfPutHex64(&Cursor, frame->error_code);
    PfPutChar(&Cursor, '\n');
    Len = (uint32_t)(Cursor - Report);

    /* Always emit the short version to the serial console so the fault is
       diagnosable even if the filesystem cannot be touched from here. */
    DbgSerialWrite("=== NULLCONSOLE FAULT REPORT ===\n");
    DbgSerialWrite(Report);

    /* Frame-pointer backtrace: each frame's [rbp] holds the caller's rbp and
       [rbp+8] the return address. Walk it so any fault prints its call chain. */
    DbgSerialWrite("=== BACKTRACE ===\n");
    {
        uint64_t rbp = frame->rbp;
        for (int i = 0; i < 16 && rbp > 0x1000; i++)
        {
            uint64_t *fr = (uint64_t *)rbp;
            uint64_t ret = fr[1];
            uint64_t prev = fr[0];
            char h[17];
            h[16] = '\0';
            for (int k = 15; k >= 0; k--)
            {
                uint8_t n = (ret >> (k * 4)) & 0xFU;
                h[15 - k] = (n < 10) ? (char)('0' + n) : (char)('A' + n - 10);
            }
            DbgSerialWrite(h);
            DbgSerialWrite("\n");
            if (prev < 0x1000)
                break;
            rbp = prev;
        }
    }

    /* Best-effort persistent copy under /jor using the existing disk write
       mechanism, guarded by the mount check and the re-entrancy flag above
       so we never corrupt the filesystem or escalate to a triple fault. */
    if (DiskFsIsMounted())
    {
        DiskFsMkdirAll("/jor");
        DiskFsWriteInPath("/jor", "pagefault.log", Report, Len);
    }

    for (;;)
    {
        __asm__ volatile("cli; hlt");
    }
}


extern void ISR0(void);
extern void ISR1(void);
extern void ISR2(void);
extern void ISR3(void);
extern void ISR4(void);
extern void ISR5(void);
extern void ISR6(void);
extern void ISR7(void);
extern void ISR8(void);
extern void ISR9(void);
extern void ISR10(void);
extern void ISR11(void);
extern void ISR12(void);
extern void ISR13(void);
extern void ISR14(void);
extern void ISR15(void);
extern void ISR16(void);
extern void ISR17(void);
extern void ISR18(void);
extern void ISR19(void);
extern void ISR20(void);
extern void ISR21(void);
extern void ISR22(void);
extern void ISR23(void);
extern void ISR24(void);
extern void ISR25(void);
extern void ISR26(void);
extern void ISR27(void);
extern void ISR28(void);
extern void ISR29(void);
extern void ISR30(void);
extern void ISR31(void);

static void (*const g_IsrStubs[32])(void) = {
    ISR0,  ISR1,  ISR2,  ISR3,  ISR4,  ISR5,  ISR6,  ISR7,
    ISR8,  ISR9,  ISR10, ISR11, ISR12, ISR13, ISR14, ISR15,
    ISR16, ISR17, ISR18, ISR19, ISR20, ISR21, ISR22, ISR23,
    ISR24, ISR25, ISR26, ISR27, ISR28, ISR29, ISR30, ISR31
};

void ISRInitialize(void)
{
    uint8_t Vector;

    for (Vector = 0; Vector < 32; Vector++)
    {
        IDTSetGate(Vector,
                   (uint64_t)g_IsrStubs[Vector],
                   0x08,
                   0x8E);
    }
}
