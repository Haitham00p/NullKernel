#include "task.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "kernel/debug/debug.h"
#include "kernel/terminal/terminal.h"
#include <stdint.h>
#include <stdbool.h>
#define MAX_TASKS 32
typedef enum { TASK_FREE=0,TASK_READY=1,TASK_RUNNING=2,TASK_DEAD=3 } TaskState;
struct NullTask{uint64_t id,rsp,rip,cr3;uint8_t *stack;void (*entry)(void);TaskState state;};
static struct NullTask Tasks[MAX_TASKS];static uint64_t Count,Current;static bool InTaskWorld;
extern void TaskSwitch(uint64_t *OldRsp,const uint64_t *NewRsp);
static void TaskBootstrap(void){NullTask*t=&Tasks[Current];if(t->entry)t->entry();TaskExit();}
static int NextReady(void){for(uint32_t n=1;n<=MAX_TASKS;n++){uint64_t i=(Current+n)%MAX_TASKS;if(Tasks[i].state==TASK_READY)return(int)i;}return-1;}
void TaskSystemInitialize(void){for(uint32_t i=0;i<MAX_TASKS;i++)Tasks[i].state=TASK_FREE;Count=0;Current=0;InTaskWorld=false;DbgInfo("Task system online",(uintptr_t)TaskSystemInitialize);}
NullTask*TaskCreate(void(*Entry)(void)){if(!Entry||Count>=MAX_TASKS)return 0;uint64_t p=PmmAllocPage();if(!p)return 0;uint64_t*sp=(uint64_t*)(uintptr_t)(p+4096);*--sp=(uint64_t)TaskBootstrap;*--sp=0;*--sp=0;*--sp=0;*--sp=0;*--sp=0;*--sp=0;struct NullTask*t=&Tasks[Count];t->id=Count+1;t->stack=(uint8_t*)(uintptr_t)p;t->rsp=(uint64_t)sp;t->rip=(uint64_t)Entry;t->cr3=VmmCurrentRoot();t->entry=Entry;t->state=TASK_READY;Count++;return t;}
void TaskYield(void){if(!InTaskWorld)return;int next=NextReady();if(next<0){InTaskWorld=false;TaskSwitch(&Tasks[Current].rsp,&(uint64_t){0});return;}uint64_t old=Current;if(Tasks[old].state==TASK_RUNNING)Tasks[old].state=TASK_READY;Tasks[next].state=TASK_RUNNING;Current=(uint64_t)next;TaskSwitch(&Tasks[old].rsp,&Tasks[next].rsp);}
void TaskExit(void){Tasks[Current].state=TASK_DEAD;for(;;){int n=NextReady();if(n<0){InTaskWorld=false;return;}uint64_t old=Current;Current=(uint64_t)n;Tasks[Current].state=TASK_RUNNING;TaskSwitch(&Tasks[old].rsp,&Tasks[Current].rsp);}}
bool TaskScheduleFromInterrupt(uint64_t*RspSlot){if(Count<2)return false;int next=NextReady();if(next<0)return false;uint64_t old=Current;Tasks[old].rsp=*RspSlot;Tasks[old].state=TASK_READY;Tasks[next].state=TASK_RUNNING;Current=(uint64_t)next;*RspSlot=Tasks[next].rsp;VmmLoadRoot(Tasks[next].cr3);return true;}
NullTask*TaskCurrent(void){return Count?&Tasks[Current]:0;}uint64_t TaskCount(void){return Count;}uint64_t TaskCurrentId(void){return Count?Tasks[Current].id:0;}uint64_t TaskCurrentCr3(void){return Count?Tasks[Current].cr3:VmmCurrentRoot();}
static void DemoA(void){for(int i=0;i<5;i++){TerminalWrite32("[task A] yield\n",0xFFFFFFFF);TaskYield();}}
static void DemoB(void){for(int i=0;i<5;i++){TerminalWrite32("[task B] yield\n",0xFFFFFFFF);TaskYield();}}
void TaskRunDemo(void){if(InTaskWorld)return;Count=0;Current=0;if(!TaskCreate(DemoA)||!TaskCreate(DemoB))return;Tasks[0].state=TASK_RUNNING;Tasks[1].state=TASK_READY;Current=0;InTaskWorld=true;TaskSwitch(&Tasks[0].rsp,&Tasks[0].rsp);}
