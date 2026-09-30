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
#include "../Kernel/drivers/timer/PIT.h"
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
     * Network initialization is deliberately disabled during boot.
     * Enable interrupts only after all synchronous boot/shell state is ready.
     */
    /*
     * Diagnostic isolation: leave PIT IRQ0 masked while enabling keyboard
     * interrupts. If the shell now appears, the fault is definitively in
     * the timer/preemption/render path rather than filesystem/network boot.
     */
    /*
     * Absolute interrupt-path isolation:
     * mask BOTH hardware IRQs that are enabled by IRQInitialize().
     * STI therefore cannot dispatch either PIT or keyboard while we test
     * the post-shell path. This rules out an immediate IRQ1 fault as well
     * as the already-isolated IRQ0/PIT path.
     */
    PICMaskIRQ(IRQ_TIMER);
    PICMaskIRQ(IRQ_KEYBOARD);

    DbgSerialWrite("[DIAG] before sti\n");
    __asm__ volatile("sti");
    DbgSerialWrite("[DIAG] after sti\n");
    TerminalPrintLine32("InterruptSys online (HW IRQs masked)", 0xFFFFFFFF);
    DbgInfo("Reached post-STI checkpoint", (uintptr_t)kernel_main);

    /*
     * PIT is masked during this diagnostic, so the normal IRQ0-driven
     * framebuffer present never runs. Present the terminal explicitly.
     * If the screen appears after this call, the "black screen" was only
     * a missing framebuffer flush, not a kernel crash.
     */
    DbgSerialWrite("[DIAG] before explicit framebuffer present\\n");
    FbPresent32();
    DbgSerialWrite("[DIAG] after explicit framebuffer present\\n");

    /*
     * Do not touch the FAT32 filesystem from this diagnostic path.
     * The prompt is already rendered by ShellInitialize(); the boot-log
     * flush can only obscure whether execution actually reaches ShellUpdate.
     */
    DbgSerialWrite("[DIAG] entering shell loop\n");

    while (1)
    {
        DbgSerialWrite("[DIAG] shell update\n");
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}
