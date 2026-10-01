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
bits 64

section .text
global start
extern TerminalPrintLine32
extern KernelSleep
start:
    mov rdi, msg0
    mov esi, 0xFF00FFFF
    xor eax, eax
    call TerminalPrintLine32
    mov rdi, 1000
    call KernelSleep
    mov rdi, msg1
    mov esi, 0xFFFF00FF
    xor eax, eax
    call TerminalPrintLine32
    ret

section .data
msg0: db "Hello Again!", 0
msg1: db "This is asm!", 0
