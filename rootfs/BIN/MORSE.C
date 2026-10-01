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
#include <stdint.h>

static void MorseCode(const char *p)
{
    uint32_t i;

    for (i = 0; p[i] != '\0'; i++)
    {
        putchar(p[i]);
    }
    putchar(' ');
}

static void MorseChar(char c)
{
    if (c == ' ')
    {
        putchar('/');
        putchar(' ');
        return;
    }
    if (c >= 'A' && c <= 'Z')
    {
        c = (char)(c + 32);
    }
    if (c == 'a')
    {
        MorseCode(".-");
    }
    else if (c == 'b')
    {
        MorseCode("-...");
    }
    else if (c == 'c')
    {
        MorseCode("-.-.");
    }
    else if (c == 'd')
    {
        MorseCode("-..");
    }
    else if (c == 'e')
    {
        MorseCode(".");
    }
    else if (c == 'f')
    {
        MorseCode("..-.");
    }
    else if (c == 'g')
    {
        MorseCode("--.");
    }
    else if (c == 'h')
    {
        MorseCode("....");
    }
    else if (c == 'i')
    {
        MorseCode("..");
    }
    else if (c == 'j')
    {
        MorseCode(".---");
    }
    else if (c == 'k')
    {
        MorseCode("-.-");
    }
    else if (c == 'l')
    {
        MorseCode(".-..");
    }
    else if (c == 'm')
    {
        MorseCode("--");
    }
    else if (c == 'n')
    {
        MorseCode("-.");
    }
    else if (c == 'o')
    {
        MorseCode("---");
    }
    else if (c == 'p')
    {
        MorseCode(".--.");
    }
    else if (c == 'q')
    {
        MorseCode("--.-");
    }
    else if (c == 'r')
    {
        MorseCode(".-.");
    }
    else if (c == 's')
    {
        MorseCode("...");
    }
    else if (c == 't')
    {
        MorseCode("-");
    }
    else if (c == 'u')
    {
        MorseCode("..-");
    }
    else if (c == 'v')
    {
        MorseCode("...-");
    }
    else if (c == 'w')
    {
        MorseCode(".--");
    }
    else if (c == 'x')
    {
        MorseCode("-..-");
    }
    else if (c == 'y')
    {
        MorseCode("-.--");
    }
    else if (c == 'z')
    {
        MorseCode("--..");
    }
    else
    {
        putchar('?');
    }
    putchar(' ');
}

void main(void)
{
    char arg[256];
    uint32_t i;

    if (ShellArgCount() < 1)
    {
        puts("usage: morse <text>  (e.g. morse SOS)");
        return;
    }
    ShellArgGet(0, arg, (uint32_t)sizeof(arg));
    for (i = 0; arg[i] != '\0'; i++)
    {
        MorseChar(arg[i]);
    }
    putchar('\n');
}