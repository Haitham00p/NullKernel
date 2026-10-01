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
; ASMDEMO — a pure x86-64 assembly program, no C at all.
; Assembled by n64a and linked by nld into /BIN/ASMDEMO.EXE.
; With no main symbol the linker picks the first text label as entry.
;
; Colors: TerminalPrintLine32(str, argb) prints a line in an ARGB
; color.  Kernel symbols are far away, so n64a turns `call <extern>`
; into a movabs/call-r11 trampoline automatically.

bits 64
section .text
global start

extern puts
extern TerminalPrintLine32

; ARGB colors passed in esi
start:
    push rbp
    mov  rbp, rsp

    movabs rdi, title
    mov    esi, 0xFF00FFFF      ; cyan
    xor    eax, eax
    call   TerminalPrintLine32

    movabs rdi, line1
    mov    esi, 0xFF00FF55      ; green
    xor    eax, eax
    call   TerminalPrintLine32

    movabs rdi, line2
    mov    esi, 0xFF55FFFF      ; yellow
    xor    eax, eax
    call   TerminalPrintLine32

    movabs rdi, line3
    mov    esi, 0xFFAAAAAA      ; grey
    xor    eax, eax
    call   TerminalPrintLine32

    pop  rbp
    xor  eax, eax
    ret

section .data
title: db "** pure assembly program **", 0
line1: db "assembled by n64a, linked by nld", 0
line2: db "colors via TerminalPrintLine32", 0
line3: db "C and asm: one toolchain.", 0
