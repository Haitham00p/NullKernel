BITS 16
ORG 0x8000

STAGE2_LOAD equ 0x8000
MAILBOX_BUF equ 0x0600
MAILBOX_SECTOR equ 2

start:
    jmp 0x0000:stage2_main

stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7000
    sti

    mov [boot_drive], dl
    call clear_screen

    ; If the previous menu selection requested a mode, the machine has
    ; rebooted back through NullL. Consume the one-shot mailbox and continue
    ; directly to the old boot record. The 64-bit kernel will consume mode
    ; 2/3 later and enter Assembly/NPB automatically.
    call check_boot_mailbox
    jc mailbox_error
    cmp al, 0
    jne mailbox_boot

menu:
    mov si, title
    call puts
    mov si, rule
    call puts
    mov si, subtitle
    call puts
    mov si, normal
    call puts
    mov si, assembly
    call puts
    mov si, npb
    call puts
    mov si, old_boot
    call puts
    mov si, rule2
    call puts
    mov si, prompt
    call puts

.wait:
    xor ah, ah
    int 0x16
    cmp al, '1'
    je select_normal
    cmp al, '2'
    je select_asm
    cmp al, '3'
    je select_npb
    cmp al, '0'
    je old_record
    jmp .wait

select_normal:
    mov al, 1
    jmp select_mode

select_asm:
    mov al, 2
    jmp select_mode

select_npb:
    mov al, 3

select_mode:
    call write_boot_mailbox
    jc mailbox_error
    ; Do not reset through the keyboard controller. Chainload the saved
    ; Limine MBR immediately, preserving the mailbox for the 64-bit kernel.
    mov si, chainloading
    call puts
    call old_record
    jmp $

mailbox_boot:
    call old_record
    jmp $

mailbox_error:
    mov si, mailbox_fail
    call puts
    cli
.hang:
    hlt
    jmp .hang

; AL = selected mode. Write NLM1 + mode to FAT32 reserved sector 2.
write_boot_mailbox:
    push ax
    mov di, MAILBOX_BUF
    xor ax, ax
    mov cx, 256
    rep stosw
    mov byte [MAILBOX_BUF+0], 'N'
    mov byte [MAILBOX_BUF+1], 'L'
    mov byte [MAILBOX_BUF+2], 'M'
    mov byte [MAILBOX_BUF+3], '1'
    pop ax
    mov [MAILBOX_BUF+4], al

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
    add eax, MAILBOX_SECTOR
    mov dword [mailbox_dap.lba_low], eax
    mov dword [mailbox_dap.lba_high], 0
    mov word [mailbox_dap.count], 1
    mov word [mailbox_dap.offset], MAILBOX_BUF
    mov word [mailbox_dap.segment], 0

    mov si, mailbox_dap
    mov dl, [boot_drive]
    mov ah, 0x43
    mov al, 0
    int 0x13
    ret

; Return AL=mode (0 when no request). Leave a valid mailbox intact;
; the 64-bit kernel consumes and clears it after the legacy chainload.
check_boot_mailbox:
    mov si, 0x7C00 + 0x1BE
    mov cx, 4
.find_active_mailbox:
    cmp byte [si], 0x80
    je .active_found_mailbox
    add si, 16
    loop .find_active_mailbox
    mov si, 0x7C00 + 0x1BE
.active_found_mailbox:
    add si, 8
    mov eax, [si]
    add eax, MAILBOX_SECTOR
    mov dword [mailbox_dap.lba_low], eax
    mov dword [mailbox_dap.lba_high], 0
    mov word [mailbox_dap.count], 1
    mov word [mailbox_dap.offset], MAILBOX_BUF
    mov word [mailbox_dap.segment], 0

    mov si, mailbox_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .fail

    cmp dword [MAILBOX_BUF], 0x314D4C4E ; "NLM1" little-endian
    jne .none
    mov al, [MAILBOX_BUF+4]
    cmp al, 3
    ja .none

    ; Do not clear the mailbox here. The legacy Limine chainload must
    ; carry the mode request into the 64-bit kernel. BootMonitorReadMode()
    ; is the single consumer and clears the mailbox after reading it.
    clc
    ret

.none:
    xor al, al
    clc
    ret

.fail:
    stc
    ret

old_record:
    mov si, loading_old
    call puts

    mov word [old_dap.count], 1
    mov word [old_dap.offset], 0x7C00
    mov word [old_dap.segment], 0

    ; Old Boot Record is stored immediately before the active partition.
    mov si, 0x7C00 + 0x1BE
    mov cx, 4
.find_active_old:
    cmp byte [si], 0x80
    je .active_found_old
    add si, 16
    loop .find_active_old
    mov si, 0x7C00 + 0x1BE
.active_found_old:
    add si, 8
    mov eax, [si]
    sub eax, 1
    mov dword [old_dap.lba_low], eax
    mov dword [old_dap.lba_high], 0

    mov si, old_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc old_error

    ; Never jump into a random sector: the saved record must be a
    ; real boot record with the BIOS 0xAA55 signature.
    cmp word [0x7C00 + 510], 0xAA55
    jne old_error
    mov dl, [boot_drive]
    jmp 0x0000:0x7C00

old_error:
    mov si, old_fail
    call puts
    cli
    hlt
    jmp $

reboot:
    cli
.wait:
    in al, 0x64
    test al, 2
    jnz .wait
    mov al, 0xFE
    out 0x64, al
    jmp $

clear_screen:
    mov ax, 0x0003
    int 0x10
    ret

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

title db 13,10
      db "NULLLOADER - BOOT CONTROL UNIT",13,10
      db "Select operating mode:",13,10,0
rule db 0
subtitle db 0
normal db "  [1] Normal-Mode",13,10,0
assembly db "  [2] Assembly-Mode",13,10,0
npb db "  [3] NullOS Pre-Boot",13,10,0
old_boot db "  [0] Old Boot Record",13,10,0
rule2 db 0
prompt db "Command: ",0
chainloading db 13,10,"   CHAINLOADING LEGACY BOOT RECORD...",13,10,0
loading_old db 13,10,"   LOADING LEGACY BOOT RECORD...",13,10,0
old_fail db "   ERROR: LEGACY BOOT RECORD READ FAILED.",13,10,0
mailbox_fail db 13,10,"   ERROR: BOOT MODE STORAGE FAILED.",13,10,0

align 4, db 0
old_dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0

align 4, db 0
mailbox_dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
