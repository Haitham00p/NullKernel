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

#include "process.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include <stdint.h>
#include <stdbool.h>
#define MAX_PROCESSES 32
struct NullProcess{uint64_t id,cr3;bool alive;};
static struct NullProcess P[MAX_PROCESSES];static uint64_t N,Cur;
void ProcessSystemInitialize(void){N=0;Cur=0;}
uint64_t ProcessCreateAddressSpace(void){uint64_t root=PmmAllocPage();if(!root)return 0;uint64_t*dst=(uint64_t*)(uintptr_t)root;uint64_t*src=(uint64_t*)(uintptr_t)VmmCurrentRoot();for(int i=0;i<512;i++)dst[i]=0;/* keep kernel half mappings */for(int i=256;i<512;i++)dst[i]=src[i];return root;}
NullProcess*ProcessCreate(void){if(N>=MAX_PROCESSES)return 0;uint64_t cr3=ProcessCreateAddressSpace();if(!cr3)return 0;NullProcess*p=&P[N];p->id=N+1;p->cr3=cr3;p->alive=true;Cur=N++;return p;}
void ProcessDestroy(NullProcess*p){if(!p||!p->alive)return;p->alive=false;if(p->cr3)PmmFreePage(p->cr3);}
NullProcess*ProcessCurrent(void){return N?&P[Cur]:0;}uint64_t ProcessCount(void){return N;}uint64_t ProcessCurrentId(void){return N?P[Cur].id:0;}uint64_t ProcessCr3(const NullProcess*p){return p?p->cr3:0;}
bool ProcessSwitchAddressSpace(NullProcess*p){if(!p||!p->alive)return false;Cur=(uint64_t)(p-P);VmmLoadRoot(p->cr3);return true;}
