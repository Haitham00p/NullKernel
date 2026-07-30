#include "terminal.h"
#include "../framebuffer/framebuffer.h"
#include <stdint.h>
#include <stdbool.h>
#include "../font/font.h"

#define DefaultCursXPos 0
#define DefaultCursYPos 0

static uint32_t CursX = 0;
static uint32_t CursY = 0;

void TerminalInit32(void)
{
    CursX = DefaultCursXPos;
    CursY = DefaultCursYPos;
}

void TerminalPutChar32(char c, uint32_t color)
{
    if (c == '\n')
    {
        TerminalNewLine32(color);
        return;
    }

    if (c == '\b')
    {
    TerminalBackspace32(color);
    return;
    }

    FbDrawChar32(c, CursX, CursY, color);

    CursX += FONT_WIDTH;

    if (CursX + FONT_WIDTH > FbGetWidth32())
    {
        TerminalNewLine32(color);
    }
}

void TerminalWrite32(const char *str, uint32_t color)
{
    while (*str != '\0')
    {
        TerminalPutChar32(*str, color);
        str++;
    }
}

void TerminalClear32(uint32_t color)
{
    FbCleanScreen32(color);

    CursX = DefaultCursXPos;
    CursY = DefaultCursYPos;
}

void TerminalSetCursor32(uint32_t x, uint32_t y)
{
    CursX = x;
    CursY = y;
}

void TerminalNewLine32(uint32_t Color)
{
    CursX = DefaultCursXPos;
    CursY += FONT_HEIGHT;

    if (CursY + FONT_HEIGHT > FbGetHeight32())
    {
        TerminalScroll32();
    }
}

void TerminalGetCursor32(uint32_t *x, uint32_t *y)
{
    *x = CursX;
    *y = CursY;
}

void TerminalPrintLine32(const char *str, uint32_t color)
{
    TerminalWrite32(str, color);
    TerminalNewLine32(color);
}

void TerminalBackspace32(uint32_t Color)
{
    if (CursX == DefaultCursXPos)
    {
        return;
    }

    CursX -= FONT_WIDTH;

    FbFillRect32(
        CursX,
        CursY,
        FONT_WIDTH,
        FONT_HEIGHT,
        Color);
}

void TerminalScroll32(void)
{
    FbCopyRect32(
        0,
        FONT_HEIGHT,
        0,
        0,
        FbGetWidth32(),
        FbGetHeight32() - FONT_HEIGHT);

    /* Fill the new bottom line with black (default background) */
    FbFillRect32(
        0,
        FbGetHeight32() - FONT_HEIGHT,
        FbGetWidth32(),
        FONT_HEIGHT,
        0x00000000);

    CursX = DefaultCursXPos;
    CursY = FbGetHeight32() - FONT_HEIGHT;
}

void TerminalPrintHex64(uint64_t Value)
{
    TerminalWrite32("0x", 0xFFFFFFFF);

    for (int i = 60; i >= 0; i -= 4)
    {
        uint8_t Digit = (Value >> i) & 0xF;

        if (Digit < 10)
        {
            TerminalPutChar32('0' + Digit, 0xFFFFFFFF);
        }
        else
        {
            TerminalPutChar32('A' + (Digit - 10), 0xFFFFFFFF);
        }
    }



    TerminalPutChar32('\n', 0xFFFFFFFF);
}

void TerminalPrintHex64Compact(uintptr_t Value)
{
    TerminalWrite32("0x", 0xFFFFFFFF);

    bool Started = false;

    for (int i = 60; i >= 0; i -= 4)
    {
        uint8_t Digit = (Value >> i) & 0xF;

        if (!Started)
        {
            if (Digit == 0)
            {
                continue;
            }

            Started = true;
        }

        if (Digit < 10)
        {
            TerminalPutChar32('0' + Digit, 0xFFFFFFFF);
        }
        else
        {
            TerminalPutChar32('A' + (Digit - 10), 0xFFFFFFFF);
        }
    }

    if (!Started)
    {
        TerminalPutChar32('0', 0xFFFFFFFF);
    }
}