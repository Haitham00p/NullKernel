#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stdbool.h>
#include "include/kterminal.h"
#include "include/cursor.h"

void TerminalInit32(void);
void TerminalPutChar32(char c, uint32_t color);
void TerminalWrite32(const char *str, uint32_t color);
/* Coalesce terminal mutations into one framebuffer render. */
void TerminalBeginBatch32(void);
void TerminalEndBatch32(void);
void TerminalIntWrite32(uint64_t Int, uint32_t color);
void TerminalClear32(uint32_t color);
void TerminalSetCursor32(uint32_t x, uint32_t y);
void TerminalGetCursor32(uint32_t *x, uint32_t *y);
void TerminalPrintLine32(const char *str, uint32_t color);
void TerminalNewLine32(uint32_t Color);
void TerminalBackspace32(uint32_t Color);
void TerminalScroll32(void);
void TerminalPrintHex64(uint64_t Value);
void TerminalPrintHex64Compact(uintptr_t Value);
void TerminalWriteScaled32(const char *str, uint32_t x, uint32_t y,
                           uint32_t scale, uint32_t color);
void TerminalBlinkUpdate(void);
void TerminalSetActivePane(uint32_t pane);
uint32_t TerminalGetActivePane(void);
bool TerminalIsSplit(void);
void TerminalSplitEnable(void);
void TerminalSplitDisablePromote(uint32_t pane);

term_screen_t *TerminalGetScreen(void);
uint32_t TerminalGetPaneCols32(void);

/*
 * Redraws the active shell input line in one terminal render pass.
 * Existing terminal APIs remain unchanged; this is an additive fast path
 * used by shell line editing.
 */
void TerminalRedrawInputLine32(const char *prompt, uint32_t prompt_color,
                               const char *command, uint32_t command_color,
                               uint32_t command_pos);

#endif /* TERMINAL_H */
