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

    if (ShellArgCount() < 1)
    {
        puts("usage: touch <file>");
        return;
    }
    ShellArgGet(0, name, (uint32_t)sizeof(name));
    if (DiskFsTouch(name))
    {
        printf("touch: %s\n", name);
    }
    else
    {
        printf("touch: %s failed\n", name);
    }
}
