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

#include "pci.h"
#include "arch/x86_64/cpu/io.h"
#include "kernel/debug/debug.h"
#include <stdint.h>
static uint32_t addr(uint8_t b,uint8_t d,uint8_t f,uint8_t o){return 0x80000000U|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(o&0xFC);}
uint32_t PciConfigRead32(uint8_t b,uint8_t d,uint8_t f,uint8_t o){Outd(0xCF8,addr(b,d,f,o));return Ind(0xCFC);}
void PciConfigWrite32(uint8_t b,uint8_t d,uint8_t f,uint8_t o,uint32_t v){Outd(0xCF8,addr(b,d,f,o));Outd(0xCFC,v);}
void PciConfigWrite16(uint8_t b,uint8_t d,uint8_t f,uint8_t o,uint16_t v){Outd(0xCF8,addr(b,d,f,o));Outw(0xCFC+(o&2),v);}
uint16_t PciVendor(uint8_t b,uint8_t d,uint8_t f){return (uint16_t)PciConfigRead32(b,d,f,0);}
void PciInitialize(void){uint32_t count=0;for(uint32_t b=0;b<256;b++)for(uint32_t d=0;d<32;d++){uint16_t v=PciVendor((uint8_t)b,(uint8_t)d,0);if(v==0xFFFF)continue;uint32_t id=PciConfigRead32((uint8_t)b,(uint8_t)d,0,0);DbgSerialWrite("[pci] device\n");(void)id;count++;}DbgInfo("PCI bus enumerated",(uintptr_t)count);}
