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
