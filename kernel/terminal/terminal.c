#include "terminal.h"
#include "drivers/video_framebuffer/framebuffer.h"
#include "drivers/video_font/font.h"
#include "drivers/timer/PIT.h"
#include "include/cursor.h"
#include "kernel/debug/debug.h"
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

static void TerminalRenderWithDivider(void);

#define TERMINAL_MAX_PANES 2U
static bool g_split = false;
static uint32_t g_active_pane = 0;
static uint32_t g_pane_col[TERMINAL_MAX_PANES] = {0, 0};
static uint32_t g_pane_row[TERMINAL_MAX_PANES] = {0, 0};

static uint32_t TerminalPaneStartCol(uint32_t pane)
{
    if (!g_split || pane == 0U) return 0U;
    return (g_screen.cols / 2U) + 1U;
}

static uint32_t TerminalPaneCols(uint32_t pane)
{
    if (!g_split) return g_screen.cols;
    uint32_t half = g_screen.cols / 2U;
    if (pane == 0U) return half;
    return g_screen.cols - half - 1U;
}

static void TerminalSyncCursor(void)
{
    uint32_t abs_col = TerminalPaneStartCol(g_active_pane) + CurCol;
    CursorMoveTo(&g_screen, abs_col, CurRow);
}

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
    g_pane_col[0] = g_pane_col[1] = 0;
    g_pane_row[0] = g_pane_row[1] = 0;
    g_active_pane = 0;
    g_split = false;
    CursorMoveTo(&g_screen, CurCol, CurRow);

    g_initialized = true;
    TerminalRenderWithDivider();
}

static void TerminalRenderWithDivider(void)
{
    TerminalRender(&g_screen);
    if (!g_split || g_screen.cols == 0U || g_screen.rows == 0U) return;

    uint32_t divider_col = g_screen.cols / 2U;
    if (divider_col >= g_screen.cols) return;

    uint32_t x = divider_col * FONT_WIDTH;
    if (x > 0U) x--;

    FrDrawLine32(x, 0U, x, g_screen.fb_height - 1U, 0x00707070U);
}

static uint32_t TerminalContrastColor(uint32_t fg, uint32_t bg)
{
    uint32_t fr = (fg >> 16) & 0xFFU;
    uint32_t fg2 = (fg >> 8) & 0xFFU;
    uint32_t fb = fg & 0xFFU;
    uint32_t br = (bg >> 16) & 0xFFU;
    uint32_t bg2 = (bg >> 8) & 0xFFU;
    uint32_t bb = bg & 0xFFU;
    int32_t dr = (int32_t)fr - (int32_t)br;
    int32_t dg = (int32_t)fg2 - (int32_t)bg2;
    int32_t db = (int32_t)fb - (int32_t)bb;
    int32_t dist = dr * dr + dg * dg + db * db;

    if (dist < (56 * 56 * 3)) {
        uint32_t lum = br * 30U + bg2 * 59U + bb * 11U;
        return (lum > 14000U) ? 0x00000000U : 0x00FFFFFFU;
    }
    return fg;
}

void TerminalPutChar32(char c, uint32_t color) {
    if (!g_initialized) return;
    uint32_t pane_cols = TerminalPaneCols(g_active_pane);
    uint32_t pane_start = TerminalPaneStartCol(g_active_pane);

    /* Mirror the console to the serial port so output is observable over
       a serial connection as well as on the framebuffer. */
    if (c == '\n') {
        DbgSerialWrite("\r\n");
    } else if (c != '\b') {
        char sc[2] = { c, '\0' };
        DbgSerialWrite(sc);
    }

    if (c == '\n') {
        TerminalNewLine32(color);
        return;
    }

    if (c == '\b') {
        TerminalBackspace32(color);
        return;
    }

    if (CurCol >= pane_cols) {
        TerminalNewLine32(color);
    }

    uint32_t abs_col = pane_start + CurCol;
    term_cell_t *cell = &g_screen.lines[CurRow].cells[abs_col];
    cell->codepoint = (uint8_t)c;
    cell->fg_color = TerminalContrastColor(color, CurBGColor);
    cell->bg_color = CurBGColor;
    cell->attributes |= TERM_ATTR_DIRTY;

    TerminalInvalidateCell(&g_screen, abs_col, CurRow);

    CurCol++;
    g_pane_col[g_active_pane] = CurCol;
    g_pane_row[g_active_pane] = CurRow;
    TerminalSyncCursor();

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
    uint32_t start = TerminalPaneStartCol(g_active_pane);
    uint32_t cols = TerminalPaneCols(g_active_pane);
    for (uint32_t r = 0; r < g_screen.rows; r++) {
        g_term_lines[r].is_dirty = true;
        g_term_lines[r].first_dirty = 0;
        g_term_lines[r].last_dirty = (uint16_t)(g_screen.cols - 1);
        for (uint32_t c = start; c < start + cols; c++) {
            g_term_cells[r][c].codepoint = ' ';
            g_term_cells[r][c].fg_color = 0xFFFFFFFF;
            g_term_cells[r][c].bg_color = color;
            g_term_cells[r][c].attributes |= TERM_ATTR_DIRTY;
        }
    }

    CurCol = 0;
    CurRow = 0;
    g_pane_col[g_active_pane] = 0;
    g_pane_row[g_active_pane] = 0;
    TerminalSyncCursor();
    TerminalRender(&g_screen);
}

void TerminalSetCursor32(uint32_t x, uint32_t y) {
    if (!g_initialized) return;
    uint32_t col = x / FONT_WIDTH;
    uint32_t row = y / FONT_HEIGHT;
    uint32_t cols = TerminalPaneCols(g_active_pane);
    if (col >= cols) col = cols ? cols - 1U : 0U;
    CurCol = col;
    CurRow = row;
    g_pane_col[g_active_pane] = CurCol;
    g_pane_row[g_active_pane] = CurRow;
    TerminalSyncCursor();
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
        g_pane_col[g_active_pane] = CurCol;
        g_pane_row[g_active_pane] = CurRow;
        TerminalSyncCursor();
    }
    TerminalRender(&g_screen);
}

void TerminalBackspace32(uint32_t color) {
    (void)color;
    if (!g_initialized) return;

    if (CurCol == 0) return;

    CurCol--;
    uint32_t abs_col = TerminalPaneStartCol(g_active_pane) + CurCol;
    term_cell_t *cell = &g_screen.lines[CurRow].cells[abs_col];
    cell->codepoint = ' ';
    cell->bg_color = CurBGColor;
    cell->attributes |= TERM_ATTR_DIRTY;

    TerminalInvalidateCell(&g_screen, abs_col, CurRow);
    g_pane_col[g_active_pane] = CurCol;
    TerminalSyncCursor();
    TerminalRender(&g_screen);
}

void TerminalScroll32(void) {
    if (!g_initialized) return;

    uint32_t start = TerminalPaneStartCol(g_active_pane);
    uint32_t cols = TerminalPaneCols(g_active_pane);
    if (g_screen.rows == 0U || cols == 0U) return;

    for (uint32_t r = 0; r + 1U < g_screen.rows; r++) {
        uint32_t dst = r;
        uint32_t src = r + 1U;
        g_term_lines[dst].is_dirty = true;
        g_term_lines[dst].first_dirty = 0;
        g_term_lines[dst].last_dirty = (uint16_t)(g_screen.cols - 1);
        for (uint32_t c = start; c < start + cols; c++) {
            g_term_cells[dst][c] = g_term_cells[src][c];
            g_term_cells[dst][c].attributes |= TERM_ATTR_DIRTY;
        }
    }

    uint32_t last_r = g_screen.rows - 1U;
    g_term_lines[last_r].is_dirty = true;
    g_term_lines[last_r].first_dirty = 0;
    g_term_lines[last_r].last_dirty = (uint16_t)(g_screen.cols - 1);
    for (uint32_t c = start; c < start + cols; c++) {
        g_term_cells[last_r][c].codepoint = ' ';
        g_term_cells[last_r][c].fg_color = 0xFFFFFFFF;
        g_term_cells[last_r][c].bg_color = CurBGColor;
        g_term_cells[last_r][c].attributes = TERM_ATTR_DIRTY;
    }

    CurRow = g_screen.rows - 1U;
    g_pane_row[g_active_pane] = CurRow;
    TerminalSyncCursor();
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

void TerminalSetActivePane(uint32_t pane)
{
    if (!g_initialized || pane > 1U) return;
    if (!g_split && pane != 0U) return;
    g_pane_col[g_active_pane] = CurCol;
    g_pane_row[g_active_pane] = CurRow;
    g_active_pane = pane;
    CurCol = g_pane_col[pane];
    CurRow = g_pane_row[pane];
    if (CurCol >= TerminalPaneCols(pane))
        CurCol = TerminalPaneCols(pane) ? TerminalPaneCols(pane) - 1U : 0U;
    TerminalSyncCursor();
    TerminalRender(&g_screen);
}
uint32_t TerminalGetActivePane(void) { return g_active_pane; }
bool TerminalIsSplit(void) { return g_split; }

void TerminalSplitEnable(void)
{
    if (!g_initialized || g_split) return;
    uint32_t half = g_screen.cols / 2U;
    if (half < 2U || g_screen.cols < 5U) return;

    g_split = true;
    g_active_pane = 0;
    g_pane_col[0] = CurCol < half ? CurCol : half - 1U;
    g_pane_row[0] = CurRow;
    g_pane_col[1] = 0;
    g_pane_row[1] = 0;

    CurCol = g_pane_col[0];
    CurRow = g_pane_row[0];
    TerminalSyncCursor();
    TerminalInvalidateAll(&g_screen);
    TerminalRender(&g_screen);
}

void TerminalSplitDisablePromote(uint32_t pane)
{
    if (!g_initialized || !g_split || pane > 1U) return;
    uint32_t start = TerminalPaneStartCol(pane);
    uint32_t cols = TerminalPaneCols(pane);
    if (cols == 0U) return;

    for (uint32_t r = 0; r < g_screen.rows; r++) {
        for (uint32_t c = 0; c < g_screen.cols; c++) {
            if (c < cols) g_term_cells[r][c] = g_term_cells[r][start + c];
            else {
                g_term_cells[r][c].codepoint = ' ';
                g_term_cells[r][c].fg_color = 0xFFFFFFFFU;
                g_term_cells[r][c].bg_color = CurBGColor;
                g_term_cells[r][c].attributes = TERM_ATTR_DIRTY;
            }
            g_term_cells[r][c].attributes |= TERM_ATTR_DIRTY;
        }
        g_term_lines[r].is_dirty = true;
        g_term_lines[r].first_dirty = 0;
        g_term_lines[r].last_dirty = (uint16_t)(g_screen.cols - 1);
    }

    g_split = false;
    g_active_pane = 0;
    CurCol = g_pane_col[pane];
    CurRow = g_pane_row[pane];
    if (CurCol >= g_screen.cols) CurCol = g_screen.cols - 1U;
    if (CurRow >= g_screen.rows) CurRow = g_screen.rows - 1U;
    g_pane_col[0] = CurCol;
    g_pane_row[0] = CurRow;
    TerminalSyncCursor();
    TerminalInvalidateAll(&g_screen);
    TerminalRender(&g_screen);
}
