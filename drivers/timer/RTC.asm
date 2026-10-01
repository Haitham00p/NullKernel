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
default rel

section .text

global RTCReadSeconds

RTCReadSeconds:
.wait:
    mov dx, 0x70
    mov al, 0x0A
    out dx, al
    mov dx, 0x71
    in al, dx
    test al, 0x80
    jnz .wait

    mov dx, 0x70
    mov al, 0x00
    out dx, al
    mov dx, 0x71
    in al, dx
    ret
