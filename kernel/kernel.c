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
#include "vendor/limine/limine.h"
#include <stddef.h>
#include "mm/heap/heap.h"
#include "kernel/panic/panic.h"
#include "include/drivers.h"
#include "../fs/ISO9660/iso9660.h"
#include "drivers/storage/ide/ide.h"
#include "../Kernel/drivers/timer/PIT.h"
#include "installer/installer.h"
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

extern struct limine_memmap_request MemMapReq;

void kernel_main(void)
{
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

    if (MemMapReq.response == NULL)
        PanicKernel("Memory Map Not Found", (uintptr_t)kernel_main);

    DbgBoot("Kernel entry point reached", (uintptr_t)kernel_main);

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

    struct limine_memmap_response *Response = MemMapReq.response;
    struct limine_memmap_entry *Entry;
    uintptr_t HeapBase = 0;
    uint64_t HeapLength = 0;
    uint64_t HeapType = 0;

    for (uint32_t i = 0; i < Response->entry_count; i++){
        Entry = Response->entries[i];
        if (Entry->type != LIMINE_MEMMAP_USABLE) continue;
        if (Entry->length <= HeapLength) continue;
        HeapBase = Entry->base;
        HeapLength = Entry->length;
        HeapType = Entry->type;
    }

    if (HeapBase < 0x400000){
        uint64_t Drop = 0x400000 - HeapBase;
        if (Drop >= HeapLength) HeapLength = 0;
        else {
            HeapBase += Drop;
            HeapLength -= Drop;
        }
    }

    if (HeapBase == 0)
        PanicKernel("No Usable Entry Found For Heap", (uintptr_t)HeapInitialize);

    PmmInitialize(Response);
    PmmReserveRange(0x100000, 0x100000);
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
    ISOInitialize();
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
    TerminalPrintLine32("Enabling Interrupt System ...", 0xFFFFFFFF);

    __asm__ volatile("sti");

    TerminalPrintLine32("InterruptSys online", 0xFFFFFFFF);

    DriversInitAll();

    if (ISOIsReady())
        InstallerRun();

    TerminalClear32(BackSpaceColor);
    DiskFsMount();

    /*
     * reboot-c stores a one-shot BM1 request on the installed FAT32 disk.
     * The request must be consumed before the normal shell starts.
     */
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
