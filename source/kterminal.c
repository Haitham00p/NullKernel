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

#include "include/kterminal.h"
#include "include/kstring.h"
#include "kernel/terminal/terminal.h"

void TerminalWriteUInt64(uint64_t Value, uint32_t Color)
{
    char Buffer[32];
    UInt64ToString(Value, Buffer);
    TerminalWrite32(Buffer, Color);
}

void TerminalWriteInt64(int64_t Value, uint32_t Color)
{
    char Buffer[32];
    Int64ToString(Value, Buffer);
    TerminalWrite32(Buffer, Color);
}

void TerminalWriteHex64(uint64_t Value, uint32_t Color)
{
    char Buffer[32];
    UInt64ToHexString(Value, Buffer);
    TerminalWrite32(Buffer, Color);
}

void TerminalWritePointer(const void *Ptr, uint32_t Color)
{
    char Buffer[32];
    UInt64ToHexString((uintptr_t)Ptr, Buffer);
    TerminalWrite32(Buffer, Color);
}
