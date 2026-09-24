#include "vmm.h"
#include "pmm.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define PS 4096ULL
#define MASK 0x000FFFFFFFFFF000ULL
static uint64_t Root;
static uint64_t KernelRoot;
static inline uint64_t rdcr3(void){uint64_t x;__asm__ volatile("mov %%cr3,%0":"=r"(x));return x&MASK;}
static inline void wrcr3(uint64_t x){__asm__ volatile("mov %0,%%cr3"::"r"(x):"memory");}
static uint64_t *table(uint64_t p){return (uint64_t*)(uintptr_t)p;}
static uint64_t *next(uint64_t *t,uint64_t i,bool create){
    if(t[i]&VMM_P) return table(t[i]&MASK);
    if(!create) return 0;
    uint64_t p=PmmAllocPage();if(!p)return 0;t[i]=p|VMM_P|VMM_W|VMM_U;return table(p);
}
void VmmInitialize(void){
    uint64_t old=rdcr3(), p=PmmAllocPage(); if(!p)return;
    memcpy((void*)(uintptr_t)p,(void*)(uintptr_t)old,PS);
    Root=p;
    KernelRoot=p;
    wrcr3(Root);
}
bool VmmMapPage(uint64_t v,uint64_t phys,uint64_t flags){
    if((v&(PS-1))||(phys&(PS-1)))return false;
    uint64_t i4=(v>>39)&511,i3=(v>>30)&511,i2=(v>>21)&511,i1=(v>>12)&511;
    uint64_t *pml4=table(Root),*pdpt=next(pml4,i4,true),*pd=next(pdpt,i3,true),*pt=next(pd,i2,true);
    if(!pdpt||!pd||!pt)return false;
    pt[i1]=(phys&MASK)|(flags&0xFFFULL)|VMM_P;
    if(flags&VMM_NX)pt[i1]|=VMM_NX;
    __asm__ volatile("invlpg (%0)"::"r"((void*)(uintptr_t)v):"memory");return true;
}
bool VmmUnmapPage(uint64_t v,bool freep){
    uint64_t *pml4=table(Root),*pdpt=next(pml4,(v>>39)&511,false),*pd=next(pdpt,(v>>30)&511,false),*pt=next(pd,(v>>21)&511,false);
    if(!pt)return false;uint64_t i=(v>>12)&511,e=pt[i];if(!(e&VMM_P))return false;pt[i]=0;
    __asm__ volatile("invlpg (%0)"::"r"((void*)(uintptr_t)v):"memory");if(freep)PmmFreePage(e&MASK);return true;
}
uint64_t VmmGetPhysical(uint64_t v){
    uint64_t *pml4=table(Root),*pdpt=next(pml4,(v>>39)&511,false),*pd=next(pdpt,(v>>30)&511,false),*pt=next(pd,(v>>21)&511,false);if(!pt)return 0;uint64_t e=pt[(v>>12)&511];return (e&VMM_P)?((e&MASK)|(v&0xFFF)):0;
}
bool VmmMapUserPage(uint64_t v,uint64_t flags){uint64_t p=PmmAllocPage();return p && VmmMapPage(v,p,flags|VMM_U);}
uint64_t VmmCurrentRoot(void){return Root;}
uint64_t VmmKernelRoot(void){return KernelRoot;}
void VmmLoadRoot(uint64_t p){Root=p&MASK;wrcr3(Root);}
