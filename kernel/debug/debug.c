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

#include "debug.h"

#include <stdbool.h>

#include "kernel/terminal/terminal.h"
#include "arch/x86_64/cpu/io.h"

/* Per-category ring buffers of boot telemetry, flushed to /jor/<name>.log so
   the last boot's diagnostics survive into the running OS. */
#define DBG_LOG_MAX 16384U

static const char *g_CatFile[DBG_CAT_COUNT] = {
    "boot.log",
    "nccBoot.log",
    "intr.log",
    "reg.log"
};

static char g_Log[DBG_CAT_COUNT][DBG_LOG_MAX];
static uint32_t g_LogLen[DBG_CAT_COUNT];

static void DbgCatAppend(DBG_CATEGORY Cat, const char *S)
{
    uint32_t Len;
    char *Buf;

    if (S == NULL || (uint32_t)Cat >= (uint32_t)DBG_CAT_COUNT)
    {
        return;
    }
    Len = g_LogLen[Cat];
    Buf = g_Log[Cat];
    while (*S != '\0' && Len < (DBG_LOG_MAX - 1U))
    {
        Buf[Len++] = *S++;
    }
    g_LogLen[Cat] = Len;
}

const char *DbgCatName(DBG_CATEGORY Cat)
{
    if ((uint32_t)Cat >= (uint32_t)DBG_CAT_COUNT)
    {
        return "boot.log";
    }
    return g_CatFile[Cat];
}

const char *DbgCatBuffer(DBG_CATEGORY Cat)
{
    if ((uint32_t)Cat >= (uint32_t)DBG_CAT_COUNT)
    {
        return g_Log[DBG_CAT_BOOT];
    }
    return g_Log[Cat];
}

uint32_t DbgCatLength(DBG_CATEGORY Cat)
{
    if ((uint32_t)Cat >= (uint32_t)DBG_CAT_COUNT)
    {
        return g_LogLen[DBG_CAT_BOOT];
    }
    return g_LogLen[Cat];
}

const char *DbgBootLogBuffer(void)
{
    return g_Log[DBG_CAT_BOOT];
}

uint32_t DbgBootLogLength(void)
{
    return g_LogLen[DBG_CAT_BOOT];
}

#define COM1_BASE 0x3F8U

static bool g_SerialReady = false;

void DbgSerialInit(void)
{
    Outb(COM1_BASE + 1, 0x00);
    Outb(COM1_BASE + 3, 0x80);
    Outb(COM1_BASE + 0, 0x01);
    Outb(COM1_BASE + 1, 0x00);
    Outb(COM1_BASE + 3, 0x03);
    Outb(COM1_BASE + 2, 0xC7);
    Outb(COM1_BASE + 4, 0x0B);
    g_SerialReady = true;
}

void DbgSerialPutChar(char Character)
{
    if (!g_SerialReady)
    {
        return;
    }
    while ((Inb(COM1_BASE + 5) & 0x20) == 0)
    {
    }
    Outb(COM1_BASE, (uint8_t)Character);
}

void DbgSerialWrite(const char *String)
{
    if (String == NULL)
    {
        return;
    }
    while (*String != '\0')
    {
        DbgSerialPutChar(*String);
        if (*String == '\n')
        {
            DbgSerialPutChar('\r');
        }
        String++;
    }
}

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

static void DbgEmit(DBG_CATEGORY Cat, uint32_t TagColor,
                    const char *Tag, const char *Message, uintptr_t Address)
{
    char Hex[17];
    uintptr_t Tmp = Address;

    for (int i = 15; i >= 0; i--)
    {
        Hex[i] = "0123456789ABCDEF"[Tmp & 0xF];
        Tmp >>= 4;
    }
    Hex[16] = '\0';

    DbgCatAppend(Cat, "[");
    DbgCatAppend(Cat, Tag);
    DbgCatAppend(Cat, "] ");
    DbgCatAppend(Cat, Message);
    DbgCatAppend(Cat, " @ ");
    DbgCatAppend(Cat, Hex);
    DbgCatAppend(Cat, "\n");

    DbgSerialWrite("[");
    DbgSerialWrite(Tag);
    DbgSerialWrite("] ");
    DbgSerialWrite(Message);
    DbgSerialWrite(" @ ");
    DbgSerialWrite(Hex);
    DbgSerialWrite("\n");

    if (Cat == DBG_CAT_BOOT)
    {
        TerminalWrite32("[", DBG_BORDER);
        TerminalWrite32(Tag, TagColor);
        TerminalWrite32("] ", DBG_BORDER);
        TerminalWrite32(Message, DBG_TEXT);
        DbgAddress(Address);
    }
}

static void DbgStatus(const char *Tag, uint32_t TagColor,
                      const char *Message, uintptr_t Address)
{
    DbgEmit(DBG_CAT_BOOT, TagColor, Tag, Message, Address);
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

void DbgLogCat(DBG_CATEGORY Cat, const char *Tag, const char *Message, uintptr_t Address)
{
    DbgEmit(Cat, 0U, Tag, Message, Address);
}

void DbgNcc(const char *Message, uintptr_t Address)
{
    DbgEmit(DBG_CAT_NCC, 0U, "NCC", Message, Address);
}

void DbgIntr(const char *Message, uintptr_t Address)
{
    DbgEmit(DBG_CAT_INTR, 0U, "INTR", Message, Address);
}

void DbgReg(const char *Message, uintptr_t Address)
{
    DbgEmit(DBG_CAT_REG, 0U, "REG", Message, Address);
}
