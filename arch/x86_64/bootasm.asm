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

    ; R10 is now the instruction result, so first recover the context
    ; pointer without destroying any output register except R10 itself.
    mov rax, rax
    mov r10, [rsp+0]
    mov [r10+0], rax
    mov [r10+8], rbx
    mov [r10+16], rcx
    mov [r10+24], rdx
    mov [r10+32], rsi
    mov [r10+40], rdi
    mov [r10+48], rbp
    mov [r10+56], r8
    mov [r10+64], r9
    ; R10 output is saved before R11, using the original value still
    ; present in the CPU register file.
    mov [r10+72], r10
    ; R11 cannot be saved after R10 becomes the context pointer, so use
    ; the temporary stack slot containing the context pointer and a second
    ; scratch register is impossible without clobbering an output. Save R11
    ; through the context pointer held in R10 by using RAX only after its
    ; value has already been committed.
    mov rax, [rsp+0]
    mov [rax+80], r11
    mov [rax+88], r12
    mov [rax+96], r13
    mov [rax+104], r14
    mov [rax+112], r15

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
