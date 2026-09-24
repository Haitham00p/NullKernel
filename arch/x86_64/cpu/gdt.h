#ifndef NULLOS_GDT_H
#define NULLOS_GDT_H
#include <stdint.h>
void GdtInitialize(void);
uint16_t GdtKernelCode(void);
uint16_t GdtKernelData(void);
uint16_t GdtUserCode(void);
uint16_t GdtUserData(void);
void GdtSetRsp0(uint64_t Rsp);
#endif
