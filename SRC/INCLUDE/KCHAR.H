#ifndef KCHAR_H
#define KCHAR_H

#include <stdbool.h>

/**
 * @brief Checks if a character is a decimal digit ('0' - '9').
 * @param C Character to check.
 * @return true if C is a decimal digit, false otherwise.
 */
bool IsDigit(char C);

/**
 * @brief Checks if a character is an alphabetic character ('a'-'z', 'A'-'Z').
 * @param C Character to check.
 * @return true if C is an alphabetic character, false otherwise.
 */
bool IsAlpha(char C);

/**
 * @brief Checks if a character is a hexadecimal digit ('0'-'9', 'a'-'f', 'A'-'F').
 * @param C Character to check.
 * @return true if C is a hex digit, false otherwise.
 */
bool IsHexDigit(char C);

/**
 * @brief Checks if a character is a whitespace character (' ', '\t', '\n', '\r', '\f', '\v').
 * @param C Character to check.
 * @return true if C is whitespace, false otherwise.
 */
bool IsSpace(char C);

#endif /* KCHAR_H */
