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
