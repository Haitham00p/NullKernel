#ifndef NULLOS_SCHEDULER_H
#define NULLOS_SCHEDULER_H
#include <stdint.h>
void SchedulerInitialize(void);
void SchedulerTick(void);
uint64_t SchedulerTicks(void);
#endif
