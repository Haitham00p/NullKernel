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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef unsigned int uint32_t;

typedef struct
{
    const char *Name;
    const char *Path;
    const char *Content;
} NccHeaderVault;

typedef struct
{
    const char *Name;
    unsigned long long Addr;
} NccSymbolVault;

void *kmalloc(unsigned long long Size)
{
    return malloc(Size ? Size : 1);
}

void kfree(void *Ptr)
{
    free(Ptr);
}

void TerminalWrite32(const char *Str, unsigned int Color)
{
    (void)Color;
    fputs(Str, stderr);
}

void TerminalPrintLine32(const char *Str, unsigned int Color)
{
    (void)Color;
    fprintf(stderr, "%s\n", Str);
}

int DiskFsIsMounted(void) { return 0; }
int DiskFsMount(void) { return 0; }
int DiskFsRead(const char *Name, void *Buffer, unsigned int MaxSize, unsigned int *BytesRead)
{
    (void)Name; (void)Buffer; (void)MaxSize; (void)BytesRead;
    return 0;
}

