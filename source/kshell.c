/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "include/kshell.h"
#include "include/kstring.h"
#include "include/kchar.h"

bool ShellArgToUInt64(const char *Arg, uint64_t *OutValue)
{
    if (Arg == NULL || OutValue == NULL)
    {
        return false;
    }

    const char *Ptr = Arg;
    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr == '+')
    {
        Ptr++;
    }

    if (!IsDigit(*Ptr))
    {
        return false;
    }

    uint64_t Value = StringToUInt64(Arg);

    while (IsDigit(*Ptr))
    {
        Ptr++;
    }

    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr != '\0')
    {
        return false;
    }

    *OutValue = Value;
    return true;
}

bool ShellArgToInt64(const char *Arg, int64_t *OutValue)
{
    if (Arg == NULL || OutValue == NULL)
    {
        return false;
    }

    const char *Ptr = Arg;
    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr == '-' || *Ptr == '+')
    {
        Ptr++;
    }

    if (!IsDigit(*Ptr))
    {
        return false;
    }

    int64_t Value = StringToInt64(Arg);

    while (IsDigit(*Ptr))
    {
        Ptr++;
    }

    while (IsSpace(*Ptr))
    {
        Ptr++;
    }

    if (*Ptr != '\0')
    {
        return false;
    }

    *OutValue = Value;
    return true;
}
