BITS 64

global LTSM_ReadRFLAGS
global LTSM_ReadCR0
global LTSM_ReadCR2
global LTSM_ReadCR3
global LTSM_ReadCR4
global LTSM_ReadRSP
global LTSM_ReadRBP
global LTSM_ReadCS
global LTSM_ReadSS

section .text

LTSM_ReadRFLAGS:
    pushfq
    pop rax
    ret

LTSM_ReadCR0:
    mov rax, cr0
    ret

LTSM_ReadCR2:
    mov rax, cr2
    ret

LTSM_ReadCR3:
    mov rax, cr3
    ret

LTSM_ReadCR4:
    mov rax, cr4
    ret

LTSM_ReadRSP:
    mov rax, rsp
    ret

LTSM_ReadRBP:
    mov rax, rbp
    ret

LTSM_ReadCS:
    xor eax, eax
    mov ax, cs
    ret

LTSM_ReadSS:
    xor eax, eax
    mov ax, ss
    ret
