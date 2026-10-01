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
#include <drivers/timer/PIT.h>

static unsigned int rnd;

static unsigned int RndNext(void)
{
    rnd = rnd * 1103515245u + 12345u;
    return rnd;
}

void main(void)
{
    char arg[64];
    unsigned int limit = 100u;
    unsigned int value;

    rnd = (unsigned int)PITGetMilliseconds() ^ 0x85EBCA6Bu;
    if (ShellArgCount() >= 1)
    {
        ShellArgGet(0, arg, (uint32_t)sizeof(arg));
        limit = (unsigned int)atoi(arg);
    }
    if (limit == 0u)
    {
        limit = 100u;
    }
    value = RndNext() % limit;
    printf("%u\n", value);
}
