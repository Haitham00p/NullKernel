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
