#ifndef KTERMINAL_H
#define KTERMINAL_H

#include <stdint.h>

/**
 * @brief Writes an unsigned 64-bit integer to the terminal in specified color.
 * @details Internally uses UInt64ToString conversion API.
 * @param Value Unsigned 64-bit integer to write.
 * @param Color 32-bit ARGB/RGB color value.
 */
void TerminalWriteUInt64(uint64_t Value, uint32_t Color);

/**
 * @brief Writes a signed 64-bit integer to the terminal in specified color.
 * @details Internally uses Int64ToString conversion API.
 * @param Value Signed 64-bit integer to write.
 * @param Color 32-bit ARGB/RGB color value.
 */
void TerminalWriteInt64(int64_t Value, uint32_t Color);

/**
 * @brief Writes a 64-bit hex value ("0x...") to the terminal in specified color.
 * @details Internally uses UInt64ToHexString conversion API.
 * @param Value Unsigned 64-bit integer value.
 * @param Color 32-bit ARGB/RGB color value.
 */
void TerminalWriteHex64(uint64_t Value, uint32_t Color);

/**
 * @brief Writes a pointer address ("0x...") to the terminal in specified color.
 * @details Internally uses UInt64ToHexString conversion API.
 * @param Ptr Pointer address to display.
 * @param Color 32-bit ARGB/RGB color value.
 */
void TerminalWritePointer(const void *Ptr, uint32_t Color);

#endif /* KTERMINAL_H */
