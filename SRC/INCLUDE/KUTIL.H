#ifndef KUTIL_H
#define KUTIL_H

/**
 * @file kutil.h
 * @brief Freestanding Kernel Utility Library
 *
 * Provides kernel-safe, freestanding helper functions for character inspection,
 * string operations, base-10 and base-16 number-to-string & string-to-number
 * conversions, terminal formatted output, and shell argument parsing.
 *
 * No libc, no malloc, no printf, no host OS APIs.
 */

#include "kchar.h"
#include "kstring.h"
#include "kterminal.h"
#include "kshell.h"

#endif /* KUTIL_H */
