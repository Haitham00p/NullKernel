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

static unsigned int dice_state;

static unsigned int DiceNext(void)
{
    dice_state = dice_state * 1103515245u + 12345u;
    return (dice_state >> 16) & 0x7FFFu;
}

void main(void)
{
    char arg[64];
    int sides = 6;
    int value;

    dice_state = (unsigned int)PITGetMilliseconds() ^ 0x9E3779B9u;
    if (ShellArgCount() >= 1)
    {
        ShellArgGet(0, arg, (uint32_t)sizeof(arg));
        sides = atoi(arg);
    }
    if (sides < 2)
    {
        sides = 6;
    }
    value = (int)(DiceNext() % (unsigned int)sides) + 1;
    printf("d%d = %d\n", sides, value);
}
