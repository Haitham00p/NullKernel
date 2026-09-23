BITS 64
section .text
global BootAsmRun

; void BootAsmRun(void *code, BootAsmRegs *regs)
; SysV: RDI=code, RSI=regs
; The generated instruction must not modify RSP. The monitor appends RET.
BootAsmRun:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    push rdi
    push rsi

    ; stack:
    ; [rsp+00] = regs
    ; [rsp+08] = code
    ; [rsp+16] = old r15
    ; [rsp+24] = old r14
    ; [rsp+32] = old r13
    ; [rsp+40] = old r12
    ; [rsp+48] = old rbp
    ; [rsp+56] = old rbx
    ; [rsp+64] = return address

    mov r10, [rsp+0]

    mov rax, [r10+0]
    mov rbx, [r10+8]
    mov rcx, [r10+16]
    mov rdx, [r10+24]
    mov rsi, [r10+32]
    mov rdi, [r10+40]
    mov rbp, [r10+48]
    mov r8,  [r10+56]
    mov r9,  [r10+64]
    mov r10, [r10+72]
    mov r11, [rsp+0]
    mov r11, [r11+80]
    mov r12, [rsp+0]
    mov r12, [r12+88]
    mov r13, [rsp+0]
    mov r13, [r13+96]
    mov r14, [rsp+0]
    mov r14, [r14+104]
    mov r15, [rsp+0]
    mov r15, [r15+112]

    call [rsp+8]

    push rax
    push r10
    push r11

    mov rax, [rsp+24]

    mov r10, [rsp+16]
    mov [rax+0], r10
    mov [rax+8], rbx
    mov [rax+16], rcx
    mov [rax+24], rdx
    mov [rax+32], rsi
    mov [rax+40], rdi
    mov [rax+48], rbp
    mov [rax+56], r8
    mov [rax+64], r9
    mov r10, [rsp+8]
    mov [rax+72], r10
    mov r10, [rsp+0]
    mov [rax+80], r10
    mov [rax+88], r12
    mov [rax+96], r13
    mov [rax+104], r14
    mov [rax+112], r15

    pop r11
    pop r10
    pop rax

    mov r15, [rsp+16]
    mov r14, [rsp+24]
    mov r13, [rsp+32]
    mov r12, [rsp+40]
    mov rbp, [rsp+48]
    mov rbx, [rsp+56]

    add rsp, 8
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
