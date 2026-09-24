#ifndef NULLOS_SYSCALL_H
#define NULLOS_SYSCALL_H
#include <stdint.h>
uint64_t SyscallDispatch(uint64_t Number,uint64_t A1,uint64_t A2,uint64_t A3,uint64_t A4);
void SyscallInitialize(void);
uint64_t SyscallCount(void);
#endif
