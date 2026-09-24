#include "syscall.h"
#include "kernel/task/task.h"
#include "kernel/process/process.h"
#include "kernel/terminal/terminal.h"
#include <stdint.h>
static uint64_t Count;
enum{SYS_NULL=0,SYS_WRITE=1,SYS_GETPID=2,SYS_YIELD=3};
void SyscallInitialize(void){Count=0;}
uint64_t SyscallDispatch(uint64_t n,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4){(void)a2;(void)a3;(void)a4;Count++;switch(n){case SYS_NULL:return 0;case SYS_WRITE:if(a1)TerminalWrite32((const char*)(uintptr_t)a1,0xFFFFFFFF);return 0;case SYS_GETPID:return ProcessCurrentId();case SYS_YIELD:TaskYield();return 0;default:return(uint64_t)-1;}}
uint64_t SyscallCount(void){return Count;}
