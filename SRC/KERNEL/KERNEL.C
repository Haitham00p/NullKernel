#include <stdint.h>
#include "drivers/video_framebuffer/framebuffer.h"
#include "kernel/terminal/terminal.h"
#include "arch/x86_64/interrupts/idt.h"
#include "arch/x86_64/interrupts/isr.h"
#include "arch/x86_64/interrupts/irq.h"
#include "drivers/input/keyboard/keyboard.h"
#include "drivers/input/mouse/mouse.h"
#include "arch/x86_64/interrupts/pic.h"
#include "arch/x86_64/cpu/io.h"
#include "kernel/debug/debug.h"
#include "shell/shell.h"
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

extern struct limine_memmap_request MemMapReq;


void kernel_main(void)
{
    /* Enable SSE (CR4.OSFXSR) so varargs prologues using MOVAPS execute
       instead of raising #UD. Also set CR0.MP=1, CR0.EM=0, CR0.NE=1. */
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
    {
        PanicKernel("Memory Map Not Found", (uintptr_t)kernel_main);
    }
    
    DbgBoot("Kernel entry point reached", (uintptr_t)kernel_main);

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

    MouseInitialize();
    DbgInfo("Mouse driver ready", (uintptr_t)MouseInitialize);
    DbgIntr("Mouse driver ready (IRQ12)", (uintptr_t)MouseInitialize);
    struct limine_memmap_response *Response;
    struct limine_memmap_entry *Entry;

    Response = MemMapReq.response;

    uintptr_t HeapBase = 0;
    uint64_t HeapLength = 0;
    uint64_t HeapType = 0;

    for (uint32_t i = 0; i < Response->entry_count; i++){

        Entry = Response->entries[i];
        if (Entry->type != LIMINE_MEMMAP_USABLE){
            continue;
        }
        if (Entry->length <= HeapLength){
            continue;
        }
        HeapBase = Entry->base;

        HeapLength = Entry->length;

        HeapType = Entry->type;

    
    }

    /* Keep the heap above the guest-program zone (ncc images at 0x100000
       and the guest stack at 0x300000). */
    if (HeapBase < 0x400000)
    {
        uint64_t Drop = 0x400000 - HeapBase;
        if (Drop >= HeapLength)
        {
            HeapLength = 0;
        }
        else
        {
            HeapBase += Drop;
            HeapLength -= Drop;
        }
    }

    if (HeapBase == 0){
        PanicKernel("No Usable Entry Found For Heap", (uintptr_t)HeapInitialize);
    }
    HeapInitialize(HeapBase, HeapLength);
    /* Double-buffer + terminal must come up AFTER the heap is live, since the
       back buffer is kmalloc()'d. TerminalInit32 captures the (now back)
       framebuffer address, so it must run after FbInitDoubleBuffer(). */
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


    TerminalPrintLine32("INTERRUPTS ENABLED", 0xFFFFFFFF);

    /* Run every auto-registered driver / component initializer.  New
       drivers register themselves with DRIVER_INIT(...) and need no
       further wiring here. */
    DriversInitAll();

    /* Only show the Try/Install prompt when the NullOS ISO actually mounted. */
    if (ISOIsReady())
    {
        InstallerRun();
    }

    /* Mount the installed hard disk (when present) and rebuild every
       /BIN source program into a /BIN executable so the OS only depends on
       what is stored on the disk itself. */
    TerminalClear32(BackSpaceColor);
    ShellBootBuild();

    ShellApplyRegistry();
    ShellBootRunRAB();

    /* Honour the boot-time default directory from /REG/DEFDIR.V before the
       first shell prompt is drawn. */
    ShellApplyDefDir();

    ShellInitialize();

    DbgBootLogFlush();

    /* Kernel Main Loop*/

    while (1)
    {
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}
