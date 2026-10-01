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
