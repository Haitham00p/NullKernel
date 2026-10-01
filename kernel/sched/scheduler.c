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

#include "scheduler.h"
#include "kernel/debug/debug.h"
#include <stdint.h>

static uint64_t Ticks;
static uint64_t Quantum = 10;

void SchedulerInitialize(void)
{
    Ticks = 0;
    Quantum = 10;
    DbgInfo("Preemptive round-robin scheduler online", (uintptr_t)SchedulerInitialize);
}

void SchedulerTick(void)
{
    Ticks++;
}

uint64_t SchedulerTicks(void)
{
    return Ticks;
}

void SchedulerSetQuantum(uint64_t ticks)
{
    if (ticks)
        Quantum = ticks;
}

uint64_t SchedulerQuantum(void)
{
    return Quantum;
}

bool SchedulerShouldSwitch(void)
{
    return Quantum && (Ticks % Quantum) == 0;
}
