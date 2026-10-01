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
#include <stdint.h>

static unsigned char Cells[1000];
static char code[4096];

void main(void)
{
    int i;
    int pc;
    int depth;

    if (ShellArgCount() < 1)
    {
        puts("usage: bf <brainfuck-program>");
        puts("example: bf ++++++++++[>++++++<-]>++++++.");
        return;
    }
    ShellArgGet(0, code, (uint32_t)sizeof(code));

    i = 0;
    pc = 0;
    while (code[pc] != '\0')
    {
        if (code[pc] == '>')
        {
            i++;
        }
        else if (code[pc] == '<')
        {
            i--;
        }
        else if (code[pc] == '+')
        {
            Cells[i] = Cells[i] + 1;
        }
        else if (code[pc] == '-')
        {
            Cells[i] = Cells[i] - 1;
        }
        else if (code[pc] == '.')
        {
            putchar((int)Cells[i]);
        }
        else if (code[pc] == '[')
        {
            if (Cells[i] == 0)
            {
                depth = 1;
                while (depth > 0)
                {
                    pc++;
                    if (code[pc] == '[')
                    {
                        depth++;
                    }
                    else if (code[pc] == ']')
                    {
                        depth--;
                    }
                }
            }
        }
        else if (code[pc] == ']')
        {
            if (Cells[i] != 0)
            {
                depth = 1;
                while (depth > 0)
                {
                    pc--;
                    if (code[pc] == ']')
                    {
                        depth++;
                    }
                    else if (code[pc] == '[')
                    {
                        depth--;
                    }
                }
            }
        }
        pc++;
    }
    putchar('\n');
}
