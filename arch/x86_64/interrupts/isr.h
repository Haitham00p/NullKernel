#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* Uniform layout captured by ISRCommon in isr.asm. For vectors that the CPU
   does not push an error code for, a zero is synthesized. RSP at fault time
   is (uintptr_t)frame + 160. */
typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} interrupt_frame_t;

void ISRInitialize(void);

void ExceptionHandler(interrupt_frame_t *frame);

#endif
