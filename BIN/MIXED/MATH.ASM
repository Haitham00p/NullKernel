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
; MIXED project — x86-64 assembly for n64a (the NullOS assembler).
;
; System V AMD64 calling convention, integer subset:
;   args: rdi, rsi, rdx, rcx, r8, r9      return: rax
; Kernel symbols (puts, printf, ...) are far away, so use movabs to
; load their addresses — n64a emits an ABS64 relocation for those.

bits 64

section .text

global asm_add
global asm_mul
global asm_banner
extern puts
extern TerminalPrintLine32

; int asm_add(int a, int b) { return a + b; }
asm_add:
    push rbp
    mov  rbp, rsp
    lea  eax, [rdi + rsi]
    pop  rbp
    ret

; int asm_mul(int a, int b) { return a * b; }
asm_mul:
    push rbp
    mov  rbp, rsp
    mov  eax, edi
    imul eax, esi
    pop  rbp
    ret

; void asm_banner(void)
asm_banner:
    push rbp
    mov  rbp, rsp
    movabs rdi, banner_msg
    mov    esi, 0xFF00FF55      ; green
    xor    eax, eax
    call   TerminalPrintLine32
    pop    rbp
    ret

section .data
banner_msg: db "hello from n64a assembly!", 0
