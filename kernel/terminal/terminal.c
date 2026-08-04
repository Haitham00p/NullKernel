#include "terminal.h"
#include "drivers/video_framebuffer/framebuffer.h"
#include "drivers/video_font/font.h"
#include "drivers/timer/PIT.h"
#include "include/cursor.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_TERM_ROWS 128
#define MAX_TERM_COLS 256

static term_line_t g_term_lines[MAX_TERM_ROWS];
static term_cell_t g_term_cells[MAX_TERM_ROWS][MAX_TERM_COLS];
static term_screen_t g_screen;
static bool g_initialized = false;

static uint32_t CurCol = 0;
static uint32_t CurRow = 0;
static uint32_t CurBGColor = 0x00000000;

term_screen_t *TerminalGetScreen(void) {
    return &g_screen;
}

void TerminalInit32(void) {
    uint32_t fb_w = FbGetWidth32();
    uint32_t fb_h = FbGetHeight32();
    uint32_t *fb_addr = FbGetAddress32();
    uint32_t fb_pitch = FbGetPitchPixels32();

    uint32_t cols = fb_w / FONT_WIDTH;
    uint32_t rows = fb_h / FONT_HEIGHT;

    if (cols > MAX_TERM_COLS) cols = MAX_TERM_COLS;
    if (rows > MAX_TERM_ROWS) rows = MAX_TERM_ROWS;

    for (uint32_t r = 0; r < rows; r++) {
        g_term_lines[r].cells = g_term_cells[r];
        g_term_lines[r].length = 0;
        g_term_lines[r].is_dirty = true;
        g_term_lines[r].first_dirty = 0;
        g_term_lines[r].last_dirty = (uint16_t)(cols - 1);

        for (uint32_t c = 0; c < cols; c++) {
            g_term_cells[r][c].codepoint = ' ';
            g_term_cells[r][c].fg_color = 0xFFFFFFFF;
            g_term_cells[r][c].bg_color = CurBGColor;
            g_term_cells[r][c].attributes = TERM_ATTR_DIRTY;
            g_term_cells[r][c].link_id = 0;
            g_term_cells[r][c].reserved = 0;
        }
    }

    g_screen.lines = g_term_lines;
    TerminalInitialize(&g_screen, cols, rows, fb_addr, fb_w, fb_h, fb_pitch,
                       (const uint8_t *)font8x16, FONT_WIDTH, FONT_HEIGHT);

    CurCol = 0;
    CurRow = 0;
    CursorMoveTo(&g_screen, CurCol, CurRow);

    g_initialized = true;
    TerminalRender(&g_screen);
}

void TerminalPutChar32(char c, uint32_t color) {
    if (!g_initialized) return;

    if (c == '\n') {
        TerminalNewLine32(color);
        return;
    }

    if (c == '\b') {
        TerminalBackspace32(color);
        return;
    }

    if (CurCol >= g_screen.cols) {
        TerminalNewLine32(color);
    }

    term_cell_t *cell = &g_screen.lines[CurRow].cells[CurCol];
    cell->codepoint = (uint8_t)c;
    cell->fg_color = color;
    cell->bg_color = CurBGColor;
    cell->attributes |= TERM_ATTR_DIRTY;

    TerminalInvalidateCell(&g_screen, CurCol, CurRow);

    CurCol++;
    if (CurCol >= g_screen.cols) {
        TerminalNewLine32(color);
    } else {
        CursorMoveTo(&g_screen, CurCol, CurRow);
    }

    TerminalRender(&g_screen);
}

void TerminalWrite32(const char *str, uint32_t color) {
    if (!str) return;
    while (*str != '\0') {
        TerminalPutChar32(*str, color);
        str++;
    }
}

void TerminalIntWrite32(uint64_t Int, uint32_t color) {
    (void)Int;
    (void)color;
}

void TerminalClear32(uint32_t color) {
    if (!g_initialized) return;

    CurBGColor = color;
    for (uint32_t r = 0; r < g_screen.rows; r++) {
        g_term_lines[r].is_dirty = true;
        g_term_lines[r].first_dirty = 0;
        g_term_lines[r].last_dirty = (uint16_t)(g_screen.cols - 1);
        for (uint32_t c = 0; c < g_screen.cols; c++) {
            g_term_cells[r][c].codepoint = ' ';
            g_term_cells[r][c].fg_color = 0xFFFFFFFF;
            g_term_cells[r][c].bg_color = color;
            g_term_cells[r][c].attributes |= TERM_ATTR_DIRTY;
        }
    }

    CurCol = 0;
    CurRow = 0;
    CursorMoveTo(&g_screen, CurCol, CurRow);
    TerminalRender(&g_screen);
}

void TerminalSetCursor32(uint32_t x, uint32_t y) {
    if (!g_initialized) return;
    uint32_t col = x / FONT_WIDTH;
    uint32_t row = y / FONT_HEIGHT;
    CurCol = col;
    CurRow = row;
    CursorMoveTo(&g_screen, CurCol, CurRow);
    TerminalRender(&g_screen);
}

void TerminalGetCursor32(uint32_t *x, uint32_t *y) {
    if (x) *x = CurCol * FONT_WIDTH;
    if (y) *y = CurRow * FONT_HEIGHT;
}

void TerminalNewLine32(uint32_t color) {
    (void)color;
    if (!g_initialized) return;

    CurCol = 0;
    CurRow++;

    if (CurRow >= g_screen.rows) {
        TerminalScroll32();
    } else {
        CursorMoveTo(&g_screen, CurCol, CurRow);
    }
    TerminalRender(&g_screen);
}

void TerminalBackspace32(uint32_t color) {
    (void)color;
    if (!g_initialized) return;

    if (CurCol == 0) return;

    CurCol--;
    term_cell_t *cell = &g_screen.lines[CurRow].cells[CurCol];
    cell->codepoint = ' ';
    cell->bg_color = CurBGColor;
    cell->attributes |= TERM_ATTR_DIRTY;

    TerminalInvalidateCell(&g_screen, CurCol, CurRow);
    CursorMoveTo(&g_screen, CurCol, CurRow);
    TerminalRender(&g_screen);
}

void TerminalScroll32(void) {
    if (!g_initialized) return;

    for (uint32_t r = 0; r < g_screen.rows - 1; r++) {
        g_term_lines[r].is_dirty = true;
        g_term_lines[r].first_dirty = 0;
        g_term_lines[r].last_dirty = (uint16_t)(g_screen.cols - 1);

        for (uint32_t c = 0; c < g_screen.cols; c++) {
            g_term_cells[r][c] = g_term_cells[r + 1][c];
            g_term_cells[r][c].attributes |= TERM_ATTR_DIRTY;
        }
    }

    uint32_t last_r = g_screen.rows - 1;
    g_term_lines[last_r].is_dirty = true;
    g_term_lines[last_r].first_dirty = 0;
    g_term_lines[last_r].last_dirty = (uint16_t)(g_screen.cols - 1);

    for (uint32_t c = 0; c < g_screen.cols; c++) {
        g_term_cells[last_r][c].codepoint = ' ';
        g_term_cells[last_r][c].fg_color = 0xFFFFFFFF;
        g_term_cells[last_r][c].bg_color = CurBGColor;
        g_term_cells[last_r][c].attributes = TERM_ATTR_DIRTY;
    }

    CurRow = last_r;
    CursorMoveTo(&g_screen, CurCol, CurRow);
    TerminalInvalidateAll(&g_screen);
    TerminalRender(&g_screen);
}

void TerminalPrintLine32(const char *str, uint32_t color) {
    TerminalWrite32(str, color);
    TerminalNewLine32(color);
}

void TerminalPrintHex64(uint64_t Value) {
    TerminalWrite32("0x", 0xFFFFFFFF);
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t Digit = (Value >> i) & 0xF;
        if (Digit < 10) TerminalPutChar32('0' + Digit, 0xFFFFFFFF);
        else TerminalPutChar32('A' + (Digit - 10), 0xFFFFFFFF);
    }
    TerminalPutChar32('\n', 0xFFFFFFFF);
}

void TerminalPrintHex64Compact(uintptr_t Value) {
    TerminalWrite32("0x", 0xFFFFFFFF);
    bool Started = false;
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t Digit = (Value >> i) & 0xF;
        if (!Started) {
            if (Digit == 0) continue;
            Started = true;
        }
        if (Digit < 10) TerminalPutChar32('0' + Digit, 0xFFFFFFFF);
        else TerminalPutChar32('A' + (Digit - 10), 0xFFFFFFFF);
    }
    if (!Started) TerminalPutChar32('0', 0xFFFFFFFF);
}

void TerminalWriteScaled32(const char *str, uint32_t x, uint32_t y,
                           uint32_t scale, uint32_t color) {
    uint32_t StartX = x;
    if (str == 0 || scale == 0) return;
    while (*str != '\0') {
        if (*str == '\n') {
            x = StartX;
            y += FONT_HEIGHT * scale;
        } else {
            FbDrawCharScaled32((uint8_t)*str, x, y, scale, color);
            x += FONT_WIDTH * scale;
        }
        str++;
    }
}

void TerminalBlinkUpdate(void) {
    if (!g_initialized) return;
    CursorBlinkTick(&g_screen, PITGetMilliseconds());
    TerminalRender(&g_screen);
}
