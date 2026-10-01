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
#include <drivers/timer/PIT.h>

void main(void)
{
    unsigned int picks[6];
    unsigned int i;
    unsigned int j;
    unsigned int draw;
    unsigned int ms;

    ms = (unsigned int)PITGetMilliseconds();
    for (i = 0; i < 6; i++)
    {
        ms = ms * 1103515245u + 12345u;
        draw = ((ms >> 16) % 49u) + 1u;
        for (j = 0; j < i; j++)
        {
            if (picks[j] == draw)
            {
                draw = draw % 49u + 1u;
                j = 0;
            }
        }
        picks[i] = draw;
    }
    printf("lotto numbers:");
    for (i = 0; i < 6; i++)
    {
        printf(" %u", picks[i]);
    }
    putchar('\n');
}
