#include "task.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "kernel/debug/debug.h"
#include "kernel/terminal/terminal.h"
#include "kernel/sched/scheduler.h"
#include "arch/x86_64/cpu/gdt.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_TASKS 32

typedef enum { TASK_FREE=0, TASK_READY=1, TASK_RUNNING=2, TASK_DEAD=3 } TaskState;

struct NullTask
{
    uint64_t id, cr3;
    uint8_t *stack;
    IRQContext *context;
    void (*entry)(void);
    TaskState state;
};

static struct NullTask Tasks[MAX_TASKS];
static uint64_t Count, Current;

extern void TaskStartFirst(IRQContext *Frame);

static void DemoA(void);
static void DemoB(void);

static void TaskBootstrap(void)
{
    NullTask *t = &Tasks[Current];
    if (t->entry)
        t->entry();
    TaskExit();
    for (;;) __asm__ volatile("hlt");
}

static int NextReady(void)
{
    for (uint32_t n=1; n<=MAX_TASKS; n++)
    {
        uint64_t i=(Current+n)%MAX_TASKS;
        if (Tasks[i].state==TASK_READY) return (int)i;
    }
    return -1;
}

void TaskSystemInitialize(void)
{
    for (uint32_t i=0;i<MAX_TASKS;i++) Tasks[i].state=TASK_FREE;
    Count=0; Current=0;
    DbgInfo("Task system online",(uintptr_t)TaskSystemInitialize);
}

NullTask *TaskCreate(void (*Entry)(void))
{
    if (!Entry || Count>=MAX_TASKS) return 0;

    /* One physically allocated page is the stack. This avoids assuming
       consecutive physical pages from a bitmap allocator. */
    uint64_t page=PmmAllocPage();
    if (!page) return 0;

    uint64_t top=(page+4096ULL)&~0xFULL;
    top-=sizeof(IRQContext);
    IRQContext *frame=(IRQContext *)(uintptr_t)top;

    for (uint32_t i=0;i<sizeof(*frame)/sizeof(uint64_t);i++)
        ((uint64_t*)frame)[i]=0;

    frame->RIP=(uint64_t)TaskBootstrap;
    frame->CS=GdtKernelCode();
    frame->RFLAGS=0x202;
    frame->RSP=top+sizeof(IRQContext);
    frame->SS=GdtKernelData();

    struct NullTask *t=&Tasks[Count];
    t->id=Count+1;
    t->cr3=VmmCurrentRoot();
    t->stack=(uint8_t *)(uintptr_t)page;
    t->context=frame;
    t->entry=Entry;
    t->state=TASK_READY;
    Count++;
    return t;
}

void TaskYield(void)
{
    __asm__ volatile("int $0x81");
}

void TaskExit(void)
{
    Tasks[Current].state=TASK_DEAD;
    __asm__ volatile("int $0x81");
    for (;;) __asm__ volatile("hlt");
}

static IRQContext *Schedule(IRQContext *CurrentFrame)
{
    if (Count<2) return CurrentFrame;

    Tasks[Current].context=CurrentFrame;
    int next=NextReady();
    if (next<0) return CurrentFrame;

    if (Tasks[Current].state==TASK_RUNNING)
        Tasks[Current].state=TASK_READY;

    Tasks[next].state=TASK_RUNNING;
    Current=(uint64_t)next;

    if (Tasks[next].cr3!=VmmCurrentRoot())
        VmmLoadRoot(Tasks[next].cr3);

    return Tasks[next].context;
}

IRQContext *TaskScheduleFromInterrupt(IRQContext *CurrentFrame)
{
    if (!SchedulerShouldSwitch()) return CurrentFrame;
    return Schedule(CurrentFrame);
}

IRQContext *TaskYieldFromInterrupt(IRQContext *CurrentFrame)
{
    return Schedule(CurrentFrame);
}

void TaskRunDemo(void)
{
    if (Count!=0) return;

    if (!TaskCreate(DemoA) || !TaskCreate(DemoB)) return;

    Tasks[0].state=TASK_RUNNING;
    Tasks[1].state=TASK_READY;
    Current=0;
    TaskStartFirst(Tasks[0].context);
}

static void DemoA(void)
{
    for (;;)
    {
        TerminalWrite32("[task A] running\n",0xFFFFFFFF);
        for (volatile uint64_t i=0;i<3000000;i++);
    }
}

static void DemoB(void)
{
    for (;;)
    {
        TerminalWrite32("[task B] running\n",0xFFFFFFFF);
        for (volatile uint64_t i=0;i<3000000;i++);
    }
}

NullTask *TaskCurrent(void){return Count?&Tasks[Current]:0;}
uint64_t TaskCount(void){return Count;}
uint64_t TaskCurrentId(void){return Count?Tasks[Current].id:0;}
uint64_t TaskCurrentCr3(void){return Count?Tasks[Current].cr3:VmmCurrentRoot();}
