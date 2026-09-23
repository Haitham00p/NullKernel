#ifndef NULLOS_BOOTASM_H
#define NULLOS_BOOTASM_H

#include <stdint.h>

typedef struct
{
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
} BootAsmRegs;

/* Executes one already-assembled x86-64 instruction with the supplied
 * register state. RSP/RIP are owned by the monitor and are not exposed. */
void BootAsmRun(void *code, BootAsmRegs *regs);

#endif
