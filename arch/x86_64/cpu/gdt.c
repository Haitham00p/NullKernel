#include "gdt.h"
#include <stdint.h>
#include <stdbool.h>
typedef struct __attribute__((packed)){uint16_t limit;uint64_t base;} GDTR;
typedef struct __attribute__((packed)){uint16_t lo,base0;uint8_t base1,access,gran,base2; } Desc;
typedef struct __attribute__((packed)){uint32_t rsv0;uint64_t rsp0,rsp1,rsp2;uint64_t rsv1;uint64_t ist1,ist2,ist3,ist4,ist5,ist6,ist7;uint64_t rsv2,rsv3;uint16_t iomap;} TSS;
static Desc Gdt[9]; static uint64_t TssDesc[2]; static TSS Tss __attribute__((aligned(16))); static uint8_t Stack[16384] __attribute__((aligned(16)));
static void desc(int n,uint32_t base,uint32_t limit,uint8_t access,uint8_t gran){Gdt[n].lo=limit;Gdt[n].base0=base;Gdt[n].base1=base>>16;Gdt[n].access=access;Gdt[n].gran=(limit>>16)&0x0F;Gdt[n].gran|=gran;Gdt[n].base2=base>>24;}
static void tssdesc(void){uint64_t b=(uint64_t)&Tss,l=sizeof(Tss)-1;TssDesc[0]=(l&0xFFFF)|((b&0xFFFFFF)<<16)|(0x89ULL<<40)|((l>>16&0xF)<<48)|((b>>24&0xFF)<<56);TssDesc[1]=b>>32;}
void GdtSetRsp0(uint64_t rsp){Tss.rsp0=rsp;}
void GdtInitialize(void){
    desc(0,0,0,0,0);desc(1,0,0xFFFFF,0x9A,0xA0);desc(2,0,0xFFFFF,0x92,0x80);desc(3,0,0xFFFFF,0xB2,0x80);desc(4,0,0xFFFFF,0xBA,0xA0);desc(5,0,0xFFFFF,0xD2,0x80);desc(6,0,0xFFFFF,0xDA,0xA0);desc(7,0,0xFFFFF,0xF2,0x80);desc(8,0,0xFFFFF,0xFA,0xA0);
    Tss.rsp0=(uint64_t)(Stack+sizeof(Stack));Tss.iomap=sizeof(TSS);tssdesc();
    static uint8_t Raw[sizeof(Gdt)+sizeof(TssDesc)];for(uint32_t i=0;i<sizeof(Gdt);i++)Raw[i]=((uint8_t*)Gdt)[i];for(uint32_t i=0;i<sizeof(TssDesc);i++)Raw[sizeof(Gdt)+i]=((uint8_t*)TssDesc)[i];
    GDTR gdtr={(uint16_t)(sizeof(Raw)-1),(uint64_t)Raw};__asm__ volatile("lgdt %0"::"m"(gdtr));__asm__ volatile("pushq $0x08;lea 1f(%%rip),%%rax;pushq %%rax;lretq;1:":::"rax","memory");
    __asm__ volatile("mov $0x10,%%ax;mov %%ax,%%ds;mov %%ax,%%es;mov %%ax,%%ss;mov %%ax,%%fs;mov %%ax,%%gs;mov $0x48,%%ax;ltr %%ax":::"rax","memory");
}
uint16_t GdtKernelCode(void){return 0x08;} uint16_t GdtKernelData(void){return 0x10;} uint16_t GdtRing1Data(void){return 0x18|1;} uint16_t GdtRing1Code(void){return 0x20|1;}\nuint16_t GdtRing2Data(void){return 0x28|2;} uint16_t GdtRing2Code(void){return 0x30|2;}\nuint16_t GdtUserData(void){return 0x38|3;} uint16_t GdtUserCode(void){return 0x40|3;}
