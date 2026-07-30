#include <stdint.h>
#include "framebuffer.h"
#include "terminal.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "keyboard.h"
#include "pic.h"
#include "io.h"
#include "../debug/debug.h"
#include "../shell/shell.h"
#include "limine.h"
#include <stddef.h>
#include "../memory/heap.h"
#include "../panic/panic.h"

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
    
    DbgBoot("Running NullKernel Main API", (uintptr_t *)kernel_main);

    IDTInitialize();
    DbgInfo("IDT System Initialized", (uintptr_t *)IDTInitialize);


    ISRInitialize();
    DbgInfo("ISR System Initialized", (uintptr_t *)ISRInitialize);


    IRQInitialize();
    DbgInfo("IRQ System Initialized", (uintptr_t *)IRQInitialize);


    KbdInitialize();
    DbgInfo("Keyboard Driver Initialized", (uintptr_t *)KbdInitialize);
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
    DbgInfo("Heap Initialized", (uintptr_t *)HeapInitialize);
    TerminalWrite32("Heap Base : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact(HeapBase);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("Heap Length : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact(HeapLength);
    TerminalNewLine32(0x00000000);
    TerminalWrite32("Heap Type : ", 0xFFFFFFFF);
    TerminalPrintHex64Compact(HeapType);
    TerminalNewLine32(0x00000000);
    TerminalPrintLine32("Enabling Interrupt System ...", 0x00FF00FF);

    __asm__ volatile("sti");


    TerminalPrintLine32("INTERRUPTS ENABLED", 0x00FF00FF);


    TerminalPrintLine32("Press Any Button To Continue . . . ", 0x000000FF);

    while(!KbdAvailable())
    {
        __asm__ volatile("hlt");
    }
    KbdFlushBuffer();
    TerminalClear32(0x00000000);
    ShellInitialize();

    /* Kernel Main Loop*/

    while (1)
    {
        ShellUpdate();
        __asm__ volatile("hlt");
    }
}
