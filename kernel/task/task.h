#ifndef NULLOS_TASK_H
#define NULLOS_TASK_H
#include <stdint.h>
typedef struct NullTask NullTask;
void TaskSystemInitialize(void);
NullTask *TaskCreate(void (*Entry)(void));
void TaskYield(void);
void TaskRunDemo(void);
NullTask *TaskCurrent(void);
uint64_t TaskCount(void);
#endif
