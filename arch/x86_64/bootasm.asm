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

    ; Install a temporary exception IDT before entering C. The permanent
    ; GDT/IDT are initialized by kernel_main, but any fault before that must
    ; be recorded rather than cascading into a triple fault.
    lea r8, [rel early_idt]
    lea r9, [rel early_fault_table]
    mov ecx, 32
.early_idt_loop:
    mov rax, [r9]
    mov word [r8], ax
    mov word [r8+2], 0x08
    mov byte [r8+4], 0
    mov byte [r8+5], 0x8E
    mov rdx, rax
    shr rdx, 16
    mov word [r8+6], dx
    mov rdx, rax
    shr rdx, 32
    mov dword [r8+8], edx
    mov dword [r8+12], 0
    add r8, 16
    add r9, 8
    loop .early_idt_loop

    mov word [rel early_idtr], (32*16)-1
    lea rax, [rel early_idt]
    mov [rel early_idtr+2], rax
    lidt [rel early_idtr]

    mov dx, 0x3F8
    mov al, 'K'
    out dx, al
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt

; Exception stubs push their vector number, then share a non-returning
; recorder. Record layout at physical 0x6E00: vector, CR2, RSP, stack top.
%macro EARLY_FAULT_STUB 1
early_fault_%1:
    mov eax, %1
    jmp early_fault_common
%endmacro

EARLY_FAULT_STUB 0
EARLY_FAULT_STUB 1
EARLY_FAULT_STUB 2
EARLY_FAULT_STUB 3
EARLY_FAULT_STUB 4
EARLY_FAULT_STUB 5
EARLY_FAULT_STUB 6
EARLY_FAULT_STUB 7
EARLY_FAULT_STUB 8
EARLY_FAULT_STUB 9
EARLY_FAULT_STUB 10
EARLY_FAULT_STUB 11
EARLY_FAULT_STUB 12
EARLY_FAULT_STUB 13
EARLY_FAULT_STUB 14
EARLY_FAULT_STUB 15
EARLY_FAULT_STUB 16
EARLY_FAULT_STUB 17
EARLY_FAULT_STUB 18
EARLY_FAULT_STUB 19
EARLY_FAULT_STUB 20
EARLY_FAULT_STUB 21
EARLY_FAULT_STUB 22
EARLY_FAULT_STUB 23
EARLY_FAULT_STUB 24
EARLY_FAULT_STUB 25
EARLY_FAULT_STUB 26
EARLY_FAULT_STUB 27
EARLY_FAULT_STUB 28
EARLY_FAULT_STUB 29
EARLY_FAULT_STUB 30
EARLY_FAULT_STUB 31

early_fault_common:
    cli
    mov [abs 0x6E00], rax
    mov rax, cr2
    mov [abs 0x6E08], rax
    mov [abs 0x6E10], rsp
    mov rax, [rsp+8]
    mov [abs 0x6E18], rax
.fault_halt:
    hlt
    jmp .fault_halt

align 8
early_fault_table:
    dq early_fault_0, early_fault_1, early_fault_2, early_fault_3
    dq early_fault_4, early_fault_5, early_fault_6, early_fault_7
    dq early_fault_8, early_fault_9, early_fault_10, early_fault_11
    dq early_fault_12, early_fault_13, early_fault_14, early_fault_15
    dq early_fault_16, early_fault_17, early_fault_18, early_fault_19
    dq early_fault_20, early_fault_21, early_fault_22, early_fault_23
    dq early_fault_24, early_fault_25, early_fault_26, early_fault_27
    dq early_fault_28, early_fault_29, early_fault_30, early_fault_31

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

section .bss
align 16
early_idt:  resb 32*16
early_idtr: resb 10
