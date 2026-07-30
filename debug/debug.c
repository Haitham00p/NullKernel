#include "debug.h"
#include <stdint.h>
#include <stdbool.h>
#include "../terminal/terminal.h"

void DbgBoot(const char *str, uintptr_t *API)
{   
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalWrite32("[BOOT] ", 0x0080C0FF);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintLine32(str, 0xFFFFFFFF);
    TerminalWrite32("Addr ", 0xFFFFFFFF);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact((uint64_t) API);
    TerminalPrintLine32("", 0x00000000);
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalPrintLine32("", 0x00000000);

}

void DbgInfo(const char *str, uintptr_t *API)
{
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalWrite32("[INFO] ", 0x0000FFFF);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintLine32(str, 0xFFFFFFFF);
    TerminalWrite32("Addr ", 0xFFFFFFFF);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact((uint64_t) API);
    TerminalPrintLine32("", 0x00000000);
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalPrintLine32("", 0x00000000);

}

void DbgErr(const char *str, uintptr_t *API){
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalWrite32("[ERR] ", 0x00FF0000);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintLine32(str, 0xFFFFFFFF);
    TerminalWrite32("Addr", 0xFFFFFFFF);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact((uint64_t) API);
    TerminalPrintLine32("", 0x00000000);
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalPrintLine32("", 0x00000000);



}

void DbgWarn(const char *str, uintptr_t *API){
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalWrite32("[WARN] ", 0x00FFFF00);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintLine32(str, 0xFFFFFFFF);
    TerminalWrite32("Addr", 0xFFFFFFFF);
    TerminalWrite32(" : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact((uint64_t) API);
    TerminalPrintLine32("", 0x00000000);
    TerminalPrintLine32("========================================", 0xFFFFFFFF);
    TerminalPrintLine32("", 0x00000000);

}

