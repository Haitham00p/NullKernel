#ifndef TERMINAL_H
#define TERMINAL_H
#include <stdint.h>
void TerminalInit32(void);
void TerminalPutChar32(char c, uint32_t color);
void TerminalWrite32(const char *str, uint32_t color);
void TerminalClear32(uint32_t color);
void TerminalSetCursor32(uint32_t x, uint32_t y);
void TerminalGetCursor32(uint32_t *x, uint32_t *y);
void TerminalPrintLine32(const char *str, uint32_t color);
void TerminalNewLine32(uint32_t Color);
void TerminalBackspace32(uint32_t Color);
void TerminalScroll32(void);
void TerminalPrintHex64(uint64_t Value);
void TerminalPrintHex64Compact(uintptr_t Value);
#endif