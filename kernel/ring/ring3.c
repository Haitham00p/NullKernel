#include "ring3.h"
#include "arch/x86_64/cpu/gdt.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "kernel/terminal/terminal.h"
#include <stdint.h>
extern void Ring3Enter(uint64_t Rip,uint64_t Rsp);
static volatile uint64_t Syscalls;
void Ring3SyscallHandler(void){Syscalls++;TerminalPrintLine32("[ring3] int 0x80 reached kernel",0xFFFFFFFF);}
uint64_t Ring3SyscallCount(void){return Syscalls;}
static const uint8_t Code[]={0xB8,0x01,0,0,0,0xCD,0x80,0xEB,0xFE};
void Ring3Initialize(void){ }
void Ring3Demo(void){
    uint64_t code=0x400000,stack=0x700000,p=VmmGetPhysical(code);
    if(!p){if(!VmmMapUserPage(code,VMM_W|VMM_U))return;p=VmmGetPhysical(code);}
    uint64_t sp=VmmGetPhysical(stack);
    if(!sp){if(!VmmMapUserPage(stack,VMM_W|VMM_U))return;}
    for(uint32_t i=0;i<sizeof(Code);i++)((uint8_t*)(uintptr_t)code)[i]=Code[i];
    TerminalPrintLine32("[ring3] entering CPL3; user code will call int 0x80",0xFFFFFFFF);
    Ring3Enter(code,stack+4096-16);
}
