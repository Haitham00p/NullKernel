#ifndef NULLOS_SCHEDULER_H
#define NULLOS_SCHEDULER_H
#include <stdint.h>
#include <stdbool.h>
void SchedulerInitialize(void);
void SchedulerTick(void);
uint64_t SchedulerTicks(void);
void SchedulerSetQuantum(uint64_t ticks);
uint64_t SchedulerQuantum(void);
bool SchedulerShouldSwitch(void);
#endif
