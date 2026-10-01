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

#include <stdio.h>
#include <diskfs.h>
#include <stdlib.h>

void main(void)
{
    char name[256];
    char buf[16384];
    uint32_t got = 0;

    if (ShellArgCount() < 1)
    {
        puts("usage: hexdump <file>");
        return;
    }
    ShellArgGet(0, name, (uint32_t)sizeof(name));
    if (!DiskFsRead(name, buf, (uint32_t)sizeof(buf), &got))
    {
        printf("hexdump: %s: read failed\n", name);
        return;
    }
    printf("got=%u\n", got);
    {
        int i;
        for (i = 0; i < 32; i++)
        {
            printf("%02x ", (unsigned int)(unsigned char)buf[i]);
        }
        printf("\n");
    }
}
