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
#include "vendor/limine/limine.h"
#include <stddef.h>
#include "mm/heap/heap.h"
#include "kernel/panic/panic.h"
#include "fs/vfs/vfs.h"

extern struct limine_memmap_request MemMapReq;


void kernel_main(void)
{
    FbInit32();

    TerminalInit32();
    TerminalClear32(0x00000000);

    if (MemMapReq.response == NULL)
    {
        PanicKernel("Memory Map Not Found", (uintptr_t)kernel_main);
    }
    
    DbgBoot("Kernel entry point reached", (uintptr_t)kernel_main);

    IDTInitialize();
    DbgInfo("Interrupt descriptor table online", (uintptr_t)IDTInitialize);


    ISRInitialize();
    DbgInfo("Exception handlers installed", (uintptr_t)ISRInitialize);


    IRQInitialize();
    DbgInfo("Hardware IRQ routing armed", (uintptr_t)IRQInitialize);


    KbdInitialize();
    DbgInfo("Keyboard driver ready", (uintptr_t)KbdInitialize);
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
        if (Entry->base == 0){
            continue;
        }
        HeapBase = Entry->base;

        HeapLength = Entry->length;

        HeapType = Entry->type;
        break;
        
    
    }

    if (HeapBase == 0){
        PanicKernel("No Usable Entry Found For Heap", (uintptr_t)HeapInitialize);
    }
    HeapInitialize(HeapBase, HeapLength);
    VfsInitialize();
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


    TerminalPrintLine32("Press Any Button To Continue . . . ", 0x000000FF);

    while(!KbdAvailable())
    {
        __asm__ volatile("hlt");
    }
    KbdFlushBuffer();
    TerminalClear32(BackSpaceColor);
    ShellInitialize();

    /* Kernel Main Loop*/

    while (1)
    {
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}
