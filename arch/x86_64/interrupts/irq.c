#include "irq.h"
#include "idt.h"
#include "pic.h"
#include "kernel/terminal/terminal.h"
#include "drivers/input/keyboard/keyboard.h"
#include "drivers/video_framebuffer/framebuffer.h"
#include "../../../drivers/timer/PIT.h"
#include "../program/run.h"
#include "kernel/sched/scheduler.h"
#include "kernel/task/task.h"

void IRQHandler(uint32_t IRQ, IRQContext *Frame)
{
    if (IRQ == IRQ_TIMER)
    {
        PITTick();
        SchedulerTick();
        TerminalBlinkUpdate();
        FbPresent32();

        if (RunGuestActive && KbdAbortRequested())
        {
            Frame->RIP = (uint64_t)RunAbortResume;
            Frame->RSP = RunSavedRsp;
            KbdAbortClear();
        }

        IRQContext *Next = TaskScheduleFromInterrupt(Frame);
        PICSendEOI((uint8_t)IRQ);
        return (void)(*(volatile uint64_t *)&Next);
    }

    if (IRQ == IRQ_KEYBOARD)
        KeyboardInterrupt();

    PICSendEOI((uint8_t)IRQ);
}
