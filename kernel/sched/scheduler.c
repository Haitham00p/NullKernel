#include "scheduler.h"
#include "task/task.h"
#include "drivers/timer/PIT.h"
#include "kernel/debug/debug.h"
static uint64_t Ticks;
void SchedulerInitialize(void){Ticks=0;DbgInfo("Round-robin scheduler online",(uintptr_t)SchedulerInitialize);}
void SchedulerTick(void){Ticks++;if((Ticks%10)==0)TaskYield();}
uint64_t SchedulerTicks(void){return Ticks;}
