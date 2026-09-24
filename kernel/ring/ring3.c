#include "ring3.h"
#include "arch/x86_64/cpu/gdt.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "kernel/terminal/terminal.h"
#include "kernel/syscall/syscall.h"
#include <stdint.h>
extern void Ring3Enter(uint64_t Rip,uint64_t Rsp);

uint64_t Ring3SyscallHandler(uint64_t n,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4){return SyscallDispatch(n,a1,a2,a3,a4);}
uint64_t Ring3SyscallCount(void){return SyscallCount();}
static const uint8_t Code[]={0xB8,0x01,0,0,0,0xCD,0x80,0xEB,0xFE};
void Ring3Initialize(void){ }
void Ring3Demo(void){
    uint64_t code=0x8000000000ULL,stack=0x8000001000ULL,p=VmmGetPhysical(code);
    if(!p){if(!VmmMapUserPage(code,VMM_W|VMM_U))return;p=VmmGetPhysical(code);}
    uint64_t sp=VmmGetPhysical(stack);
    if(!sp){if(!VmmMapUserPage(stack,VMM_W|VMM_U))return;}
    for(uint32_t i=0;i<sizeof(Code);i++)((uint8_t*)(uintptr_t)code)[i]=Code[i];
    TerminalPrintLine32("[ring3] entering CPL3; user code will call int 0x80",0xFFFFFFFF);
    Ring3Enter(code,stack+4096-16);
}
