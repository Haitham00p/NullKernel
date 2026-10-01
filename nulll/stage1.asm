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
BITS 16
ORG 0x7C00

; NullL stage-1: MBR-resident loader.
; Loads stage2 from FAT32 reserved sectors at partition_start + 7.
STAGE2_SECTORS equ 48
STAGE2_LOAD    equ 0x8000

start:
    jmp 0x0000:stage1_main

stage1_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc disk_error
    cmp bx, 0xAA55
    jne disk_error
    test cx, 1
    jz disk_error

    mov word [dap.count], STAGE2_SECTORS
    mov word [dap.offset], STAGE2_LOAD
    mov word [dap.segment], 0

    mov si, 0x7C00 + 0x1BE
    mov cx, 4
.find_active:
    cmp byte [si], 0x80
    je .active_found
    add si, 16
    loop .find_active
    mov si, 0x7C00 + 0x1BE
.active_found:
    add si, 8
    mov eax, [si]
    add eax, 7
    mov dword [dap.lba_low], eax
    mov dword [dap.lba_high], 0

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov dl, [boot_drive]
    jmp 0x0000:STAGE2_LOAD

disk_error:
    mov si, msg_disk_error
    call puts
    cli
.hang:
    hlt
    jmp .hang

puts:
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .next
.done:
    ret

boot_drive db 0
msg_disk_error db 13,10,"NullL: stage2 disk read failed",13,10,0

align 4, db 0
dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0

times 446-($-$$) db 0
