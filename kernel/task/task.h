#ifndef NULLOS_TASK_H
#define NULLOS_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "arch/x86_64/interrupts/irq.h"

typedef struct NullTask NullTask;

void TaskSystemInitialize(void);
NullTask *TaskCreate(void (*Entry)(void));
void TaskYield(void);
void TaskExit(void);
void TaskRunDemo(void);

/* Called by the timer IRQ after the full register frame was saved. */
IRQContext *TaskScheduleFromInterrupt(IRQContext *CurrentFrame);

/* Used by the software yield interrupt (int 0x81). */
IRQContext *TaskYieldFromInterrupt(IRQContext *CurrentFrame);

NullTask *TaskCurrent(void);
uint64_t TaskCount(void);
uint64_t TaskCurrentId(void);
uint64_t TaskCurrentCr3(void);

#endif
