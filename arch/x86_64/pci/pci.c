#include "pci.h"
#include "arch/x86_64/cpu/io.h"
#include "kernel/debug/debug.h"
#include <stdint.h>
static uint32_t addr(uint8_t b,uint8_t d,uint8_t f,uint8_t o){return 0x80000000U|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|(o&0xFC);}
uint32_t PciConfigRead32(uint8_t b,uint8_t d,uint8_t f,uint8_t o){Outd(0xCF8,addr(b,d,f,o));return Ind(0xCFC);}
uint16_t PciVendor(uint8_t b,uint8_t d,uint8_t f){return (uint16_t)PciConfigRead32(b,d,f,0);}
void PciInitialize(void){uint32_t count=0;for(uint32_t b=0;b<256;b++)for(uint32_t d=0;d<32;d++){uint16_t v=PciVendor((uint8_t)b,(uint8_t)d,0);if(v==0xFFFF)continue;uint32_t id=PciConfigRead32((uint8_t)b,(uint8_t)d,0,0);DbgSerialWrite("[pci] device\n");(void)id;count++;}DbgInfo("PCI bus enumerated",(uintptr_t)count);}
