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
global Ring3Enter
global Ring3Int80
extern Ring3SyscallHandler
Ring3Enter:
    cli
    mov rax, rsp
    push qword 0x3B
    push rsi
    pushfq
    or qword [rsp], 0x200
    push qword 0x43
    push rdi
    iretq
Ring3Int80:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    mov r8, rcx
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    call Ring3SyscallHandler
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
