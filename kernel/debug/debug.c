#include "debug.h"

#include <stdbool.h>

#include "kernel/terminal/terminal.h"

#define DBG_BACKGROUND 0x00000000
#define DBG_BORDER     0x00314A70U
#define DBG_TITLE      0x00D7E8FFU
#define DBG_OK         0x004BE38AU
#define DBG_INFO       0x005AA9FFU
#define DBG_WARN       0x00FFC857U
#define DBG_ERROR      0x00FF5C77U
#define DBG_TEXT       0x00E9F1FFU

static void DbgDivider(void)
{
    TerminalPrintLine32("--------------------------------------------------------", DBG_BORDER);
}

static void DbgAddress(uintptr_t Address)
{
    TerminalWrite32(" @ ", DBG_BORDER);
    TerminalPrintHex64Compact(Address);
    TerminalPutChar32('\n', DBG_TEXT);
}

static void DbgStatus(const char *Tag, uint32_t TagColor,
                      const char *Message, uintptr_t Address)
{
    TerminalWrite32("[", DBG_BORDER);
    TerminalWrite32(Tag, TagColor);
    TerminalWrite32("] ", DBG_BORDER);
    TerminalWrite32(Message, DBG_TEXT);
    DbgAddress(Address);
}

void DbgBoot(const char *Message, uintptr_t Address)
{
    static bool BannerShown = false;

    if (!BannerShown)
    {
        TerminalClear32(DBG_BACKGROUND);
        DbgDivider();
        TerminalPrintLine32("                  N U L L O S   K E R N E L", DBG_TITLE);
        TerminalPrintLine32("                    EARLY BOOT TELEMETRY", DBG_INFO);
        DbgDivider();
        TerminalPrintLine32(" ARCHITECTURE  : x86_64 long mode", DBG_TEXT);
        TerminalPrintLine32(" BOOT PROTOCOL: Limine framebuffer path", DBG_TEXT);
        TerminalPrintLine32(" CONSOLE      : graphical terminal online", DBG_TEXT);
        TerminalPrintLine32(" TRACE        : early kernel diagnostics enabled", DBG_TEXT);
        TerminalPrintLine32(" MEMORY       : awaiting usable memory map", DBG_TEXT);
        TerminalPrintLine32(" POLICY       : halt safely on unrecoverable fault", DBG_TEXT);
        DbgDivider();
        BannerShown = true;
    }
    DbgStatus("BOOT", DBG_INFO, Message, Address);
}

void DbgInfo(const char *Message, uintptr_t Address)
{
    DbgStatus(" OK ", DBG_OK, Message, Address);
}

void DbgErr(const char *Message, uintptr_t Address)
{
    DbgStatus("FAIL", DBG_ERROR, Message, Address);
}

void DbgWarn(const char *Message, uintptr_t Address)
{
    DbgStatus("WARN", DBG_WARN, Message, Address);
}
