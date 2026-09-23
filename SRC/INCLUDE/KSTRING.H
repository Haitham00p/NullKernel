#ifndef KSTRING_H
#define KSTRING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ========================================================================= */
/* String Comparison & Parsing Helpers                                       */
/* ========================================================================= */

/**
 * @brief Checks if two null-terminated strings are identical.
 * @param A First string.
 * @param B Second string.
 * @return true if both strings are equal, false otherwise.
 */
bool StringEquals(const char *A, const char *B);

/**
 * @brief Compares two null-terminated strings lexicographically.
 * @param A First string.
 * @param B Second string.
 * @return 0 if equal, <0 if A < B, >0 if A > B.
 */
int StringCompare(const char *A, const char *B);

/**
 * @brief Checks if a string starts with a specified prefix.
 * @param String Target null-terminated string.
 * @param Prefix Prefix to search for.
 * @return true if String starts with Prefix, false otherwise.
 */
bool StringStartsWith(const char *String, const char *Prefix);

/**
 * @brief Checks if a string ends with a specified suffix.
 * @param String Target null-terminated string.
 * @param Suffix Suffix to search for.
 * @return true if String ends with Suffix, false otherwise.
 */
bool StringEndsWith(const char *String, const char *Suffix);

/* ========================================================================= */
/* String to Number Conversions                                             */
/* ========================================================================= */

/**
 * @brief Parses an unsigned 64-bit integer from a string.
 * @details Ignores leading spaces. Supports optional '+'.
 *          Stops parsing on the first invalid character.
 * @param Str Input string to parse.
 * @return Parsed uint64_t value. Returns 0 if Str is NULL or starts with invalid char.
 */
uint64_t StringToUInt64(const char *Str);

/**
 * @brief Parses a signed 64-bit integer from a string.
 * @details Ignores leading spaces. Supports optional '+' and '-'.
 *          Stops parsing on the first invalid character.
 * @param Str Input string to parse.
 * @return Parsed int64_t value. Returns 0 if Str is NULL or starts with invalid char.
 */
int64_t StringToInt64(const char *Str);

/* ========================================================================= */
/* Number to String Conversions                                             */
/* ========================================================================= */

/**
 * @brief Converts an unsigned 64-bit integer to a base-10 string.
 * @param Value Unsigned 64-bit integer to convert.
 * @param Buffer Destination buffer (must be at least 21 bytes long).
 * @return Pointer to Buffer, or NULL if Buffer is NULL.
 */
char *UInt64ToString(uint64_t Value, char *Buffer);

/**
 * @brief Converts a signed 64-bit integer to a base-10 string.
 * @param Value Signed 64-bit integer to convert.
 * @param Buffer Destination buffer (must be at least 22 bytes long).
 * @return Pointer to Buffer, or NULL if Buffer is NULL.
 */
char *Int64ToString(int64_t Value, char *Buffer);

/**
 * @brief Converts an unsigned 64-bit integer to a hex string with "0x" prefix.
 * @param Value Unsigned 64-bit integer to convert.
 * @param Buffer Destination buffer (must be at least 19 bytes long).
 * @return Pointer to Buffer, or NULL if Buffer is NULL.
 */
char *UInt64ToHexString(uint64_t Value, char *Buffer);

#endif /* KSTRING_H */
