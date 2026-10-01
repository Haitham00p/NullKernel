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

global IRQ0
global IRQ1
global IRQ2
global IRQ3
global IRQ4
global IRQ5
global IRQ6
global IRQ7
global IRQ8
global IRQ9
global IRQ10
global IRQ11
global IRQ12
global IRQ13
global IRQ14
global IRQ15

extern IRQHandler

; Saved frame:
;   rax..r15 = 15 qwords
;   RIP, CS, RFLAGS, RSP, SS = 5 qwords
;
; The CPU already pushed the iret frame before entering us. We simply put
; every GPR in front of it. IRQHandler may return a DIFFERENT IRQContext*
; in RAX. That is the next task's context.

%macro IRQ_STUB 1
IRQ%1:
    cli

    ; Push in reverse order so RAX is at [RSP+0].
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rsi, rsp
    mov edi, %1
    call IRQHandler

    ; IRQHandler returns the context to resume in RAX.
    mov r12, rax
    mov rsp, r12

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq
%endmacro

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15
