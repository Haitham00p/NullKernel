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
#include <stdlib.h>
#include <stdlib.h>

static unsigned long Fact(unsigned long n)
{
    if (n <= 1)
    {
        return 1;
    }
    return n * Fact(n - 1);
}

void main(void)
{
    char arg[64];
    unsigned long n = 10;

    if (ShellArgCount() >= 1)
    {
        ShellArgGet(0, arg, (uint32_t)sizeof(arg));
        n = strtoul(arg, 0, 10);
    }
    printf("%lu! = %lu\n", n, Fact(n));
}
