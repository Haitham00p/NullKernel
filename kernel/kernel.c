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

#include <stdint.h>
#include "drivers/video_framebuffer/framebuffer.h"
#include "kernel/terminal/terminal.h"
#include "arch/x86_64/interrupts/idt.h"
#include "arch/x86_64/interrupts/isr.h"
#include "arch/x86_64/interrupts/irq.h"
#include "drivers/input/keyboard/keyboard.h"
#include "arch/x86_64/interrupts/pic.h"
#include "arch/x86_64/cpu/io.h"
#include "kernel/debug/debug.h"
#include "shell/shell.h"
#include "include/diskfs.h"
#include "include/nullboot.h"
#include <stddef.h>
#include "mm/heap/heap.h"
#include "kernel/panic/panic.h"
#include "include/drivers.h"
#include "drivers/storage/ide/ide.h"
#include "arch/x86_64/power/pwr.h"
#include "../drivers/timer/PIT.h"
#include "libc/stdlib.h"
#include "libc/stdio.h"
#include "bootmonitor/bootmonitor.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/acpi/acpi.h"
#include "arch/x86_64/pci/pci.h"
#include "kernel/task/task.h"
#include "kernel/process/process.h"
#include "kernel/syscall/syscall.h"
#include "kernel/sched/scheduler.h"
#include "kernel/registry/registry.h"
#include "drivers/network/net.h"

volatile NullBootInfo *NullBoot = NULL;

void kernel_main(NullBootInfo *BootInfo)
{
    NullBoot = BootInfo;
    if (BootInfo == NULL || BootInfo->Magic != NULLBOOT_MAGIC ||
        BootInfo->Version != NULLBOOT_VERSION)
    {
        for (;;) __asm__ volatile("cli; hlt");
    }
    {
        uint64_t cr0, cr4;
        __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
        __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
        cr0 |= (1ULL << 1) | (1ULL << 5);
        cr0 &= ~(1ULL << 2);
        cr4 |= (1ULL << 9) | (1ULL << 10);
        __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
        __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
    }

    DbgSerialInit();
    FbInit32();

    if (BootInfo->MemoryMap == 0 || BootInfo->MemoryMapCount == 0)
        PanicKernel("NullBoot memory map missing", (uintptr_t)kernel_main);

    DbgBoot("NullL native kernel entry", (uintptr_t)kernel_main);

    GdtInitialize();
    DbgInfo("GDT/TSS and ring descriptors online", (uintptr_t)GdtInitialize);

    IDTInitialize();
    DbgInfo("Interrupt descriptor table online", (uintptr_t)IDTInitialize);
    DbgIntr("Interrupt descriptor table online", (uintptr_t)IDTInitialize);

    ISRInitialize();
    DbgInfo("Exception handlers installed", (uintptr_t)ISRInitialize);
    DbgIntr("Exception handlers installed (ISRs 0-31)", (uintptr_t)ISRInitialize);

    IRQInitialize();
    DbgInfo("Hardware IRQ routing armed", (uintptr_t)IRQInitialize);
    DbgIntr("Hardware IRQ routing armed (IRQs 0-15)", (uintptr_t)IRQInitialize);

    KbdInitialize();
    DbgInfo("Keyboard driver ready", (uintptr_t)KbdInitialize);
    DbgIntr("Keyboard driver ready (IRQ1)", (uintptr_t)KbdInitialize);

    const NullBootMemoryEntry *MemoryMap =
        (const NullBootMemoryEntry *)(uintptr_t)BootInfo->MemoryMap;
    uintptr_t HeapBase = 0;
    uint64_t HeapLength = 0;
    uint64_t HeapType = 0;

    for (uint32_t i = 0; i < BootInfo->MemoryMapCount; i++)
    {
        const NullBootMemoryEntry *Entry = &MemoryMap[i];
        if (Entry->Type != NULLBOOT_MEM_USABLE)
            continue;
        if (Entry->Length <= HeapLength)
            continue;
        HeapBase = (uintptr_t)Entry->Base;
        HeapLength = Entry->Length;
        HeapType = Entry->Type;
    }

    /*
     * The heap must start above the kernel image itself. A 1 MiB floor alone
     * is not enough: the kernel's own .bss extends past 1 MiB, so a heap based
     * there lets the first large allocation (the framebuffer back-buffer)
     * overwrite live kernel data.
     */
    uintptr_t HeapFloor = 0x400000;
    if (BootInfo->KernelPhysicalEnd > HeapFloor)
    {
        HeapFloor = ((uintptr_t)BootInfo->KernelPhysicalEnd + 0xFFF) &
                    ~(uintptr_t)0xFFF;
    }

    if (HeapBase < HeapFloor)
    {
        uint64_t Drop = HeapFloor - HeapBase;
        if (Drop >= HeapLength)
            HeapLength = 0;
        else
        {
            HeapBase += Drop;
            HeapLength -= Drop;
        }
    }

    if (HeapBase == 0)
        PanicKernel("No Usable Entry Found For Heap", (uintptr_t)HeapInitialize);

    PmmInitialize(BootInfo);
    PmmReserveRange(BootInfo->KernelPhysicalBase,
                    BootInfo->KernelPhysicalEnd - BootInfo->KernelPhysicalBase);
    PmmReserveRange(BootInfo->LoaderBase,
                    BootInfo->LoaderEnd - BootInfo->LoaderBase);
    PmmReserveRange(0x5000, 0x7000);
    if (BootInfo->Framebuffer != 0)
    {
        const NullBootFramebuffer *Frame =
            (const NullBootFramebuffer *)(uintptr_t)BootInfo->Framebuffer;
        if (Frame->PhysicalAddress != 0 && Frame->Size != 0)
            PmmReserveRange(Frame->PhysicalAddress, Frame->Size);
    }
    PmmReserveRange(HeapBase, HeapLength);
    VmmInitialize();
    DbgInfo("Physical/virtual memory managers online", (uintptr_t)VmmInitialize);

    HeapInitialize(HeapBase, HeapLength);
    TaskSystemInitialize();
    ProcessSystemInitialize();
    SyscallInitialize();
    SchedulerInitialize();
    AcpiInitialize();
    PciInitialize();
    FbInitDoubleBuffer();
    TerminalInit32();
    TerminalClear32(0x00000000);
    PITInitialize(Freq);
    DbgInfo("PIT is online", (uintptr_t)PITInitialize);
    DbgInfo("Kernel heap online", (uintptr_t)HeapInitialize);

    TerminalWrite32("Heap Base : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact(HeapBase);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("Heap Length : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact(HeapLength);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("Heap Type : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact(HeapType);
    TerminalNewLine32(0x00000000);
    TerminalPrintLine32("Interrupt System: deferred until core drivers are ready", 0xFFFFFFFF);

    /*
     * Keep IRQs disabled while the remaining driver registry is brought up.
     * In particular, PIT IRQ0 can arrive immediately after STI and enter the
     * scheduler before the storage/driver layer and boot path are ready.
     */
    DbgInfo("Entering driver registry", (uintptr_t)DriversInitAll);
    DriversInitAll();
    DbgInfo("Driver registry complete", (uintptr_t)DriversInitAll);

    /*
     * Do not enable IRQ0 while the storage, boot-monitor, registry and shell
     * bring-up code is still running.  PITTick() immediately feeds the
     * scheduler, and IRQHandler() can call TaskScheduleFromInterrupt().
     * Until the first shell loop is ready, that is an asynchronous entry
     * point into partially initialized state.
     */
    TerminalClear32(BackSpaceColor);
    DbgInfo("Mounting filesystem", (uintptr_t)DiskFsMount);
    DiskFsMount();
    DbgInfo("Filesystem mount complete", (uintptr_t)DiskFsMount);

    /*
     * reboot-c stores a one-shot BM1 request on the installed FAT32 disk.
     * The request must be consumed before the normal shell starts.
     */
    {
        uint8_t NullLMode = BootMonitorReadMode();
        if (NullLMode == BOOTMONITOR_MODE_ASM ||
            NullLMode == BOOTMONITOR_MODE_NPB)
        {
            BootMonitorRunMode(NullLMode);
        }
    }

    if (BootMonitorShouldEnter())
        BootMonitorRun();

    DbgInfo("Initializing registry", (uintptr_t)RegistryInitialize);
    RegistryInitialize();
    DbgInfo("Registry initialized", (uintptr_t)RegistryInitialize);
    { uint64_t q = 0; if (RegistryReadU64("/REG/SCHED/QUANTUM.V", &q) && q != 0) SchedulerSetQuantum(q); }
    ShellApplyRegistry();
    ShellBootRunROB();
    ShellApplyDefDir();

    DbgInfo("Initializing shell", (uintptr_t)ShellInitialize);
    ShellInitialize();
    DbgInfo("Shell initialized; enabling interrupts", (uintptr_t)ShellInitialize);

    /*
     * Network bring-up, stage 2. The controller and the protocol stack are
     * already up from the driver registry; only DHCP needs the wire, and
     * this point is safe for a bounded blocking exchange because the
     * prompt is already on screen and nothing else depends on it yet.
     */
    if (NetLinkIsUp())
    {
        if (NetBringUp())
            TerminalPrintLine32("Network ready", 0x0000FF00);
        else
            TerminalPrintLine32("Network link up, no DHCP lease", 0x00FFBF00);
    }

    /*
     * Hand IRQ0 back to the PIT now that storage, the registry and the
     * shell are initialized: PITTick drives the scheduler and the
     * framebuffer present, so masking it here leaves the display frozen
     * and the clock dead. IRQ1 (keyboard) is already unmasked by
     * IRQInitialize().
     */
    PICUnmaskIRQ(IRQ_TIMER);

    __asm__ volatile("sti");
    TerminalPrintLine32("InterruptSys online", 0xFFFFFFFF);
    DbgInfo("Reached post-STI checkpoint", (uintptr_t)kernel_main);

    while (1)
    {
        ShellUpdate();
        /*
         * The NIC is polled rather than interrupt driven, so drain its
         * receive ring from the idle path. hlt parks the CPU until the
         * next PIT tick, which brings us back here ~1000 times a second.
         */
        NetService();
        __asm__ volatile("hlt");
    }
}
