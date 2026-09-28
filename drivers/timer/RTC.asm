bits 64
default rel

section .text

global RTCReadSeconds

RTCReadSeconds:
    xor rax, rax
    mov al, 0x00
    mov dx, 0x70
    out dx, al
    mov dx, 0x71
    in al, dx
    ret
