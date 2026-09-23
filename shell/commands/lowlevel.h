#ifndef NULL_LOWLEVEL_H
#define NULL_LOWLEVEL_H

#include "parser.h"

void LL_CpuInfo(void);
void LL_Cpuid(const PARSED_COMMAND *Command);
void LL_CpuidMax(void);
void LL_Flags(void);
void LL_Regs(void);
void LL_Segs(void);
void LL_Rsp(void);
void LL_Rbp(void);
void LL_Rip(void);
void LL_Rdtscp(void);
void LL_Xgetbv(const PARSED_COMMAND *Command);
void LL_CrInfo(void);
void LL_Stack(const PARSED_COMMAND *Command);
void LL_Bench(const PARSED_COMMAND *Command);
void LL_Pause(void);
void LL_Cmos(const PARSED_COMMAND *Command);
void LL_Rtc(void);
void LL_Pci(const PARSED_COMMAND *Command);
void LL_PciScan(const PARSED_COMMAND *Command);
void LL_PciDump(const PARSED_COMMAND *Command);
void LL_Pit(void);
void LL_Pic(void);
void LL_IoDelay(void);
void LL_E9(const PARSED_COMMAND *Command);
void LL_PortWatch(const PARSED_COMMAND *Command);
void LL_MemScan(const PARSED_COMMAND *Command);
void LL_MemFill(const PARSED_COMMAND *Command);
void LL_MemXor(const PARSED_COMMAND *Command);
void LL_Bits(const PARSED_COMMAND *Command);
void LL_Endian(const PARSED_COMMAND *Command);
void LL_Align(const PARSED_COMMAND *Command);
void LL_CpuidBits(void);
void LL_Hypervisor(void);
void LL_TscInfo(void);
void LL_CacheInfo(void);
void LL_FpuInfo(void);
void LL_SimdInfo(void);
void LL_IoPorts(void);
void LL_PageInfo(void);
void LL_DbgRegs(void);
void LL_GdtIdt(void);
void LL_Serial(const PARSED_COMMAND *Command);

#endif
