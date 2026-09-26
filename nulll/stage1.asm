BITS 16
ORG 0x7C00

STAGE2_LBA     equ 2055
STAGE2_SECTORS equ 24
STAGE2_LOAD    equ 0x8000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Verify BIOS EDD/LBA extensions before using INT 13h AH=42.
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
    mov dword [dap.lba_low], STAGE2_LBA
    mov dword [dap.lba_high], 0

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

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

dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0

; The installer merges this 446-byte boot code with the existing MBR
; partition table and signature.
times 446-($-$$) db 0
