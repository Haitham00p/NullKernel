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
#include "drivers/timer/PIT.h"
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
#include "kernel/elf/elf_loader.h"
#include "arch/x86_64/program/run.h"

volatile NullBootInfo *NullBoot = NULL;

void kernel_main(NullBootInfo *BootInfo)
{
    /* Initialize serial before validating the handoff so even an ABI failure
       leaves a reliable trace proving that the kernel entry was reached. */
    DbgSerialInit();
    DbgSerialWrite("NullOS: kernel_main entered\n");
    NullBoot = BootInfo;
    if (BootInfo == NULL || BootInfo->Magic != NULLBOOT_MAGIC ||
        BootInfo->Version != NULLBOOT_VERSION ||
        BootInfo->Size < sizeof(NullBootInfo) ||
        BootInfo->MemoryMap == 0 || BootInfo->MemoryMapCount == 0 ||
        BootInfo->HHDMOffset == 0)
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

    /* Bring up the architectural exception path before touching devices,
       framebuffer state, or other complex kernel subsystems. */
    GdtInitialize();
    ISRInitialize();
    IDTInitialize();
    FbInit32();
    DbgSerialWrite("NullOS: GDT/TSS, IDT and exception handlers online\n");
    DbgIntr("Exception handlers installed (ISRs 0-31)", (uintptr_t)ISRInitialize);

    if (BootInfo->MemoryMap == 0 || BootInfo->MemoryMapCount == 0)
    {
        DbgSerialWrite("FATAL: NullBoot memory map missing\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }

    IRQInitialize();
    DbgInfo("Hardware IRQ routing armed", (uintptr_t)IRQInitialize);
    DbgIntr("Hardware IRQ routing armed (IRQs 0-15)", (uintptr_t)IRQInitialize);

    KbdInitialize();
    DbgInfo("Keyboard driver ready", (uintptr_t)KbdInitialize);
    DbgIntr("Keyboard driver ready (IRQ1)", (uintptr_t)KbdInitialize);

    const NullBootMemoryEntry *MemoryMap =
        (const NullBootMemoryEntry *)(uintptr_t)BootInfo->MemoryMap;
    const uint64_t IdentityMapLimit = 1ULL << 30;
    const uint64_t MinimumHeapBase = 0x400000ULL;
    const uint64_t MaximumHeapSize = 256ULL * 1024ULL * 1024ULL;
    uintptr_t HeapBase = 0;
    uint64_t HeapLength = 0;
    uint64_t HeapType = 0;
    uint64_t KernelEnd = BootInfo->KernelPhysicalEnd;

    /* Never let the heap overlap the loaded kernel/BSS, and only use RAM
       covered by the Limine compatibility identity map (first 1 GiB). */
    if (KernelEnd < MinimumHeapBase)
        KernelEnd = MinimumHeapBase;
    KernelEnd = (KernelEnd + 0xFFFULL) & ~0xFFFULL;

    for (uint32_t i = 0; i < BootInfo->MemoryMapCount; i++)
    {
        const NullBootMemoryEntry *Entry = &MemoryMap[i];
        if (Entry->Type != NULLBOOT_MEM_USABLE || Entry->Length == 0)
            continue;

        uint64_t EntryBase = Entry->Base;
        uint64_t EntryEnd = EntryBase + Entry->Length;
        if (EntryEnd < EntryBase)
            continue;
        if (EntryEnd > IdentityMapLimit)
            EntryEnd = IdentityMapLimit;
        if (EntryBase < KernelEnd)
            EntryBase = KernelEnd;
        if (EntryBase < MinimumHeapBase)
            EntryBase = MinimumHeapBase;

        EntryBase = (EntryBase + 0xFFFULL) & ~0xFFFULL;
        EntryEnd &= ~0xFFFULL;
        if (EntryBase >= EntryEnd)
            continue;

        /* Keep at least half of the candidate range available to the PMM,
           and cap the heap so VMM still has pages to allocate page tables. */
        uint64_t CandidateLength = (EntryEnd - EntryBase) / 2ULL;
        if (CandidateLength > MaximumHeapSize)
            CandidateLength = MaximumHeapSize;
        CandidateLength &= ~0xFFFULL;
        if (CandidateLength > HeapLength)
        {
            HeapBase = (uintptr_t)EntryBase;
            HeapLength = CandidateLength;
            HeapType = Entry->Type;
        }
    }

    if (HeapBase == 0 || HeapLength < 0x100000ULL)
        PanicKernel("No safe usable range for heap", (uintptr_t)HeapInitialize);

    PmmInitialize(BootInfo);
    /* NLD ET_EXEC images are linked at 1 MiB. Keep their fixed load slot
       reserved so the PMM/heap can never hand those pages to the kernel. */
    PmmReserveRange(ELF_PROGRAM_IMAGE_BASE,
                    ELF_PROGRAM_IMAGE_END - ELF_PROGRAM_IMAGE_BASE);
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
    if (VmmEnableNX())
        DbgSerialWrite("NullOS: CPU NX support enabled (EFER.NXE)\n");
    else
        DbgSerialWrite("NullOS: CPU has no NX; NX mappings will be rejected\n");
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
    DbgBoot("Limine boot entry", (uintptr_t)kernel_main);
    DbgInfo("GDT/TSS, IDT and exception handlers online", (uintptr_t)IDTInitialize);
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

    if (BootMonitorShouldEnter())
        BootMonitorRun();

    RegistryInitialize();
    { uint64_t q = 0; if (RegistryReadU64("/REG/SCHED/QUANTUM.V", &q) && q != 0) SchedulerSetQuantum(q); }
    ShellApplyRegistry();

    /*
     * Disk executables are linked against kernel API addresses. Rebuild
     * /BIN/*.C against this kernel's freshly generated Ncc symbol vault
     * before running any boot programs; copied/prebuilt ELFs may reference
     * stale kernel addresses after a kernel update.
     */
    DbgSerialWrite("NullOS: rebuilding disk executables against current kernel\n");
    if (RunProgramOnStack((uint64_t)(uintptr_t)ShellBootBuild) ==
        NULLOS_GUEST_ABORTED)
    {
        DbgSerialWrite("NullOS: self-host build was aborted\n");
    }

    ShellBootRunROB();
    ShellApplyDefDir();
    ShellInitialize();
    DbgSerialWrite("NullOS: shell ready\n");

    DbgBootLogFlush();

    while (1)
    {
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}
