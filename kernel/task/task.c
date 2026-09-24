#include "task.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "kernel/debug/debug.h"
#include <stdint.h>
#define MAX_TASKS 32
struct NullTask{uint64_t id,rsp,rip,cr3;uint8_t *stack;uint64_t state;};
static struct NullTask Tasks[MAX_TASKS];static uint64_t Count,Current;
void TaskSystemInitialize(void){Count=0;Current=0;DbgInfo("Task system online",(uintptr_t)TaskSystemInitialize);}
NullTask *TaskCreate(void (*Entry)(void)){if(!Entry||Count>=MAX_TASKS)return 0;uint64_t p=PmmAllocPage();if(!p)return 0;struct NullTask*t=&Tasks[Count];t->id=Count+1;t->stack=(uint8_t*)(uintptr_t)p;t->rsp=p+4096-16;t->rip=(uint64_t)Entry;t->cr3=VmmCurrentRoot();t->state=1;Count++;return t;}
void TaskYield(void){if(Count>1)Current=(Current+1)%Count;}
NullTask *TaskCurrent(void){return Count?&Tasks[Current]:0;}uint64_t TaskCount(void){return Count;}
