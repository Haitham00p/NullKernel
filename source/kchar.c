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

#include "include/kchar.h"

bool IsDigit(char C)
{
    return (C >= '0' && C <= '9');
}

bool IsAlpha(char C)
{
    return ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z'));
}

bool IsHexDigit(char C)
{
    return (IsDigit(C) || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F'));
}

bool IsSpace(char C)
{
    return (C == ' ' || C == '\t' || C == '\n' || C == '\r' || C == '\f' || C == '\v');
}
