#ifndef KSHELL_H
#define KSHELL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Parses an unsigned 64-bit integer shell argument.
 * @param Arg Command argument string (e.g. Command->Args[0]).
 * @param OutValue Output pointer to store the parsed uint64_t value.
 * @return true on success (valid unsigned integer), false if Arg is NULL or parsing fails.
 */
bool ShellArgToUInt64(const char *Arg, uint64_t *OutValue);

/**
 * @brief Parses a signed 64-bit integer shell argument.
 * @param Arg Command argument string (e.g. Command->Args[0]).
 * @param OutValue Output pointer to store the parsed int64_t value.
 * @return true on success (valid signed integer), false if Arg is NULL or parsing fails.
 */
bool ShellArgToInt64(const char *Arg, int64_t *OutValue);

#endif /* KSHELL_H */
