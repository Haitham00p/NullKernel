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

#include "acpi.h"
#include "include/nullboot.h"
#include "kernel/debug/debug.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define RSDP_MAGIC "RSD PTR "
typedef struct __attribute__((packed)){char sig[8];uint8_t checksum;char oem[6];uint8_t rev;uint32_t rsdt;uint32_t len;uint64_t xsdt;uint8_t ext;uint8_t rsv[3];} RSDP2;
typedef struct __attribute__((packed)){char sig[4];uint32_t len;uint8_t rev;uint8_t csum;char oem[6];char oemtab[8];uint32_t oemrev;uint32_t creator;uint32_t creatorrev;} SDTH;
static uint64_t XsdtAddr; static bool Ready;
static bool checksum(void *p,uint32_t n){uint8_t s=0,*b=p;for(uint32_t i=0;i<n;i++)s=(uint8_t)(s+b[i]);return s==0;}
void AcpiInitialize(void){Ready=false;XsdtAddr=0;if(NullBoot==NULL||NullBoot->RSDP==0)return;RSDP2*r=(RSDP2*)(uintptr_t)NullBoot->RSDP;if(!r||memcmp(r->sig,RSDP_MAGIC,8)!=0)return;if(!checksum(r,20)||r->rev>=2&&!checksum(r,r->len))return;if(r->rev>=2&&r->xsdt)XsdtAddr=r->xsdt;Ready=XsdtAddr!=0;if(Ready)DbgInfo("ACPI XSDT online",(uintptr_t)XsdtAddr);}
bool AcpiReady(void){return Ready;}uint64_t AcpiXsdt(void){return XsdtAddr;}
uint64_t AcpiFindTable(const char Sig[4]){if(!Ready)return 0;SDTH*h=(SDTH*)(uintptr_t)XsdtAddr;if(memcmp(h->sig,"XSDT",4)!=0)return 0;uint32_t n=(h->len-sizeof(SDTH))/8;uint64_t*e=(uint64_t*)((uint8_t*)h+sizeof(SDTH));for(uint32_t i=0;i<n;i++){SDTH*t=(SDTH*)(uintptr_t)e[i];if(t&&memcmp(t->sig,Sig,4)==0)return e[i];}return 0;}

uint64_t AcpiLocalApicAddress(void){
    typedef struct __attribute__((packed)){SDTH h;uint32_t lapic;uint32_t flags;} MADT;
    MADT*m=(MADT*)(uintptr_t)AcpiFindTable("APIC"); return m?m->lapic:0;
}
uint64_t AcpiCpuCount(void){
    typedef struct __attribute__((packed)){uint8_t type,len,acpi_id,apic_id;uint32_t flags;} LAPIC;
    typedef struct __attribute__((packed)){SDTH h;uint32_t lapic;uint32_t flags;} MADT;
    MADT*m=(MADT*)(uintptr_t)AcpiFindTable("APIC"); if(!m)return 0;
    uint64_t n=0;uint8_t*p=(uint8_t*)m+sizeof(MADT);uint8_t*end=(uint8_t*)m+m->h.len;
    while(p+2<=end&&p[1]>=2&&p+p[1]<=end){if(p[0]==0&&p[1]>=8){LAPIC*l=(LAPIC*)p;if(l->flags&1)n++;}p+=p[1];}return n;
}
