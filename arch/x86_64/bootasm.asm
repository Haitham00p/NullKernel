; Copyright (C) 2026 Haitham Al-Nasser
; This file is part of NullOS.
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
; See the GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with NullOS. If not, see <https://www.gnu.org/licenses/>.
;
BITS 64
section .text
global BootAsmRun
global kernel_entry
extern kernel_main

; ELF entry point. NullL supplies RDI=NullBootInfo* and a temporary stack.
; Normalize the stack for a normal SysV call into C, then transfer control.
kernel_entry:
    cli
    and rsp, -16
    mov dx, 0x3F8
    mov al, 'K'
    out dx, al
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt


; void BootAsmRun(void *code, BootAsmRegs *regs)
; SysV: RDI=code, RSI=regs
; The generated instruction must not modify RSP. The monitor appends RET.
BootAsmRun:
    ; Save the real caller state. The values in BootAsmRegs are a
    ; separate "virtual" register state used by the monitor.
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    push rdi
    push rsi

    ; stack:
    ; [rsp+00] = original RSI = regs
    ; [rsp+08] = original RDI = code
    ; [rsp+16] = old rbp
    ; [rsp+24] = old r15
    ; [rsp+32] = old r14
    ; [rsp+40] = old r13
    ; [rsp+48] = old r12
    ; [rsp+56] = old rbx
    ; [rsp+64] = return address

    ; Load the virtual register state.
    mov r10, [rsp+0]       ; regs

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

    ; Execute exactly one generated instruction.
    ; The monitor-generated code ends with RET, so this CALL returns here.
    call [rsp+8]

    ; Save RAX/R10/R11 temporarily because RAX is also used as the
    ; pointer to BootAsmRegs while writing the result back.
    push rax
    push r10
    push r11

    ; After the three pushes:
    ; [rsp+00] = result R11
    ; [rsp+08] = result R10
    ; [rsp+16] = result RAX
    ; [rsp+24] = regs
    ; [rsp+32] = code
    ; [rsp+40] = old rbp
    ; [rsp+48] = old r15
    ; [rsp+56] = old r14
    ; [rsp+64] = old r13
    ; [rsp+72] = old r12
    ; [rsp+80] = old rbx
    ; [rsp+88] = return address

    mov rax, [rsp+24]      ; rax = regs

    mov r10, [rsp+16]      ; result RAX
    mov [rax+0], r10

    mov [rax+8], rbx       ; RBX
    mov [rax+16], rcx      ; RCX
    mov [rax+24], rdx      ; RDX
    mov [rax+32], rsi      ; RSI
    mov [rax+40], rdi      ; RDI
    mov [rax+48], rbp      ; RBP
    mov [rax+56], r8       ; R8
    mov [rax+64], r9       ; R9

    mov r10, [rsp+8]       ; result R10
    mov [rax+72], r10

    mov r10, [rsp+0]       ; result R11
    mov [rax+80], r10

    mov [rax+88], r12      ; R12
    mov [rax+96], r13      ; R13
    mov [rax+104], r14     ; R14
    mov [rax+112], r15     ; R15

    pop r11
    pop r10
    pop rax

    ; Restore the real caller state and the original stack exactly.
    ; Do NOT use the virtual register values here.
    pop rsi
    pop rdi
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
