BITS 16
ORG 0x8000

OLD_MBR_LBA equ 0 ; resolved from the active MBR partition entry

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9C00
    sti

    mov [boot_drive], dl
    call clear_screen

menu:
    mov si, title
    call puts
    mov si, old_boot
    call puts
    mov si, normal
    call puts
    mov si, assembly
    call puts
    mov si, npb
    call puts
    mov si, prompt
    call puts

.wait:
    xor ah, ah
    int 0x16
    cmp al, '0'
    je old_record
    cmp al, '1'
    je not_ready_normal
    cmp al, '2'
    je not_ready_asm
    cmp al, '3'
    je not_ready_npb
    jmp .wait

old_record:
    mov si, loading_old
    call puts

    mov word [old_dap.count], 1
    mov word [old_dap.offset], 0x7C00
    mov word [old_dap.segment], 0

    ; Old Boot Record is stored immediately before the active partition.
    mov si, 0x7C00 + 0x1BE + 8
    mov eax, [si]
    sub eax, 1
    mov dword [old_dap.lba_low], eax
    mov dword [old_dap.lba_high], 0

    mov si, old_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc old_error

    ; The saved record is a complete 512-byte MBR. Restore BIOS's drive
    ; number and transfer control exactly as a normal BIOS boot would.
    mov dl, [boot_drive]
    jmp 0x0000:0x7C00

old_error:
    mov si, old_fail
    call puts
    jmp wait_key

not_ready_normal:
    mov si, normal_msg
    call puts
    jmp wait_key

not_ready_asm:
    mov si, asm_msg
    call puts
    jmp wait_key

not_ready_npb:
    mov si, npb_msg
    call puts
    jmp wait_key

wait_key:
    mov si, press_key
    call puts
    xor ah, ah
    int 0x16
    call clear_screen
    jmp menu

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

title db 13,10,"================================",13,10
      db "          NullLoader",13,10
      db "================================",13,10,13,10,0
old_boot db "  0: Old Boot Record",13,10,0
normal db "  1: Normal-Mode",13,10,0
assembly db "  2: Assembly-Mode",13,10,0
npb db "  3: NBP",13,10,13,10,0
prompt db "Select [0-3]: ",0
loading_old db 13,10,"Loading Old Boot Record...",13,10,0
old_fail db "Old Boot Record read failed.",13,10,0
normal_msg db 13,10,"Normal-Mode is the next NullL stage.",13,10,0
asm_msg db 13,10,"Assembly-Mode is the next NullL stage.",13,10,0
npb_msg db 13,10,"NPB is moving into NullLoader.",13,10,0
press_key db "Press any key...",13,10,0

old_dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
