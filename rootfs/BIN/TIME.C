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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

void main(void)
{
    time_t t;
    clock_t a, b;

    srand(137);
    time(&t);
    printf("up: %u seconds\n", (unsigned int)t);

    a = clock();
    KernelSleep(200);
    b = clock();
    printf("sleep delta: %u ms\n", (unsigned int)(b - a));

    printf("rand: %d\n", rand() % 100);
}