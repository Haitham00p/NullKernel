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

    if (HeapBase < 0x400000)
    {
        uint64_t Drop = 0x400000 - HeapBase;
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
    DriversInitAll();

    __asm__ volatile("sti");
    TerminalPrintLine32("InterruptSys online", 0xFFFFFFFF);

    TerminalClear32(BackSpaceColor);
    DiskFsMount();

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

    RegistryInitialize();
    { uint64_t q = 0; if (RegistryReadU64("/REG/SCHED/QUANTUM.V", &q) && q != 0) SchedulerSetQuantum(q); }
    ShellApplyRegistry();
    ShellBootRunROB();
    ShellApplyDefDir();
    ShellInitialize();

    DbgBootLogFlush();

    while (1)
    {
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}
