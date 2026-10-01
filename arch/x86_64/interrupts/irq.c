/* Copyright (C) 2026 Haitham Al-Nasser
 * This file is part of NullOS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NullOS. If not, see <https://www.gnu.org/licenses/>.
 */

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
#include "mm/vmm.h"

IRQContext *IRQHandler(uint32_t IRQ, IRQContext *Frame)
{
    if (IRQ == IRQ_TIMER)
    {
        PITTick();
        SchedulerTick();
        TerminalBlinkUpdate();

        /*
         * IRQ0 may interrupt code running with a process/task CR3.
         * FbPresent32() writes to the kernel's HHDM framebuffer mapping,
         * so enter the kernel address space for the present operation.
         * Restore the interrupted CR3 before scheduling/returning.
         */
        uint64_t interrupted_root = VmmCurrentRoot();
        uint64_t kernel_root = VmmKernelRoot();
        if (kernel_root != 0 && interrupted_root != kernel_root)
            VmmLoadRoot(kernel_root);

        FbPresent32();

        if (kernel_root != 0 && interrupted_root != kernel_root)
            VmmLoadRoot(interrupted_root);

        if (RunGuestActive && KbdAbortRequested())
        {
            Frame->RIP = (uint64_t)RunAbortResume;
            Frame->RSP = RunSavedRsp;
            KbdAbortClear();
        }

        IRQContext *Next = TaskScheduleFromInterrupt(Frame);
        PICSendEOI((uint8_t)IRQ);
        return Next;
    }

    if (IRQ == IRQ_KEYBOARD)
        KeyboardInterrupt();

    PICSendEOI((uint8_t)IRQ);
    return Frame;
}

void IRQInitialize(void)
{
    PICRemap(32, 40);

    IDTSetGate(32, (uint64_t)IRQ0, 0x08, 0x8E);
    IDTSetGate(33, (uint64_t)IRQ1, 0x08, 0x8E);
    IDTSetGate(34, (uint64_t)IRQ2, 0x08, 0x8E);
    IDTSetGate(35, (uint64_t)IRQ3, 0x08, 0x8E);
    IDTSetGate(36, (uint64_t)IRQ4, 0x08, 0x8E);
    IDTSetGate(37, (uint64_t)IRQ5, 0x08, 0x8E);
    IDTSetGate(38, (uint64_t)IRQ6, 0x08, 0x8E);
    IDTSetGate(39, (uint64_t)IRQ7, 0x08, 0x8E);
    IDTSetGate(40, (uint64_t)IRQ8, 0x08, 0x8E);
    IDTSetGate(41, (uint64_t)IRQ9, 0x08, 0x8E);
    IDTSetGate(42, (uint64_t)IRQ10, 0x08, 0x8E);
    IDTSetGate(43, (uint64_t)IRQ11, 0x08, 0x8E);
    IDTSetGate(44, (uint64_t)IRQ12, 0x08, 0x8E);
    IDTSetGate(45, (uint64_t)IRQ13, 0x08, 0x8E);
    IDTSetGate(46, (uint64_t)IRQ14, 0x08, 0x8E);
    IDTSetGate(47, (uint64_t)IRQ15, 0x08, 0x8E);

    /*
     * Keep PIT IRQ0 masked until kernel_main has finished synchronous
     * storage/registry/shell initialization.  STI alone must not start
     * preemption while the scheduler's consumers are still coming online.
     */
    PICMaskIRQ(IRQ_TIMER);
    PICUnmaskIRQ(IRQ_KEYBOARD);
}
