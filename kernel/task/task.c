#include "task.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "kernel/debug/debug.h"
#include "kernel/terminal/terminal.h"
#include <stdint.h>
#define MAX_TASKS 32
typedef enum { TASK_FREE=0, TASK_READY=1, TASK_RUNNING=2, TASK_DEAD=3 } TaskState;
struct NullTask{uint64_t id,rsp,rip,cr3;uint8_t *stack;void (*entry)(void);TaskState state;};
static struct NullTask Tasks[MAX_TASKS];static uint64_t Count,Current;
extern void TaskSwitch(uint64_t *OldRsp,const uint64_t *NewRsp);
static void TaskBootstrap(void){
    NullTask *t=&Tasks[Current];
    if(t->entry)t->entry();
    t->state=TASK_DEAD;
    for(;;)TaskYield();
}
static int NextReady(void){
    for(uint32_t n=1;n<=MAX_TASKS;n++){uint64_t i=(Current+n)%MAX_TASKS;if(Tasks[i].state==TASK_READY)return (int)i;}
    return -1;
}
void TaskSystemInitialize(void){for(uint32_t i=0;i<MAX_TASKS;i++)Tasks[i].state=TASK_FREE;Count=0;Current=0;DbgInfo("Task system online",(uintptr_t)TaskSystemInitialize);}
NullTask *TaskCreate(void (*Entry)(void)){
    if(!Entry||Count>=MAX_TASKS)return 0;
    uint64_t p=PmmAllocPage();if(!p)return 0;
    uint64_t *sp=(uint64_t*)(uintptr_t)(p+4096);
    *--sp=(uint64_t)TaskBootstrap;
    *--sp=0;*--sp=0;*--sp=0;*--sp=0;*--sp=0;*--sp=0;
    struct NullTask*t=&Tasks[Count];t->id=Count+1;t->stack=(uint8_t*)(uintptr_t)p;t->rsp=(uint64_t)sp;t->rip=(uint64_t)Entry;t->cr3=VmmCurrentRoot();t->entry=Entry;t->state=TASK_READY;Count++;return t;
}
void TaskYield(void){
    int next=NextReady();if(next<0)return;
    uint64_t old=Current;Tasks[old].state=(Tasks[old].state==TASK_RUNNING)?TASK_READY:Tasks[old].state;
    Tasks[next].state=TASK_RUNNING;Current=(uint64_t)next;
    TaskSwitch(&Tasks[old].rsp,&Tasks[next].rsp);
}
NullTask *TaskCurrent(void){return Count?&Tasks[Current]:0;}uint64_t TaskCount(void){return Count;}
static void DemoA(void){for(int i=0;i<5;i++){TerminalWrite32("[task A] yield\n",0xFFFFFFFF);TaskYield();}}
static void DemoB(void){for(int i=0;i<5;i++){TerminalWrite32("[task B] yield\n",0xFFFFFFFF);TaskYield();}}
void TaskRunDemo(void){TaskCreate(DemoA);TaskCreate(DemoB);TaskYield();}
