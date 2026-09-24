#ifndef NULLOS_TASK_H
#define NULLOS_TASK_H
#include <stdint.h>
#include <stdbool.h>
typedef struct NullTask NullTask;
void TaskSystemInitialize(void);
NullTask *TaskCreate(void (*Entry)(void));
void TaskYield(void);
void TaskExit(void);
bool TaskScheduleFromInterrupt(uint64_t *RspSlot);
void TaskRunDemo(void);
NullTask *TaskCurrent(void);
uint64_t TaskCount(void);
uint64_t TaskCurrentId(void);
uint64_t TaskCurrentCr3(void);
#endif
