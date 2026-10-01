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

#include "io.h"

uint8_t Inb(uint16_t port)
{
    uint8_t ret;

    __asm__ volatile(
        "inb %1, %0"
        : "=a"(ret)
        : "dN"(port)
    );

    return ret;
}


void Outb(uint16_t port, uint8_t value)
{
    __asm__ volatile(
        "outb %0, %1"
        :
        : "a"(value), "dN"(port)
    );
}

uint16_t Inw(uint16_t Port)
{
    uint16_t Result;

    __asm__ volatile(
        "inw %1, %0"
        : "=a"(Result)
        : "Nd"(Port)
    );

    return Result;
}

void Outw(uint16_t Port, uint16_t Value)
{
    __asm__ volatile(
        "outw %0, %1"
        :
        : "a"(Value), "Nd"(Port)
    );
}

uint32_t Ind(uint16_t Port)
{
    uint32_t Result;
    __asm__ volatile(
        "inl %1, %0"
        : "=a"(Result)
        : "Nd"(Port)
    );
    return Result;
}

void Outd(uint16_t Port, uint32_t Value)
{
    __asm__ volatile(
        "outl %0, %1"
        :
        : "a"(Value), "Nd"(Port)
    );
}