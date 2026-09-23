bits 64
section .text
global main
extern printf
main:
    push rbp
    mov rbp, rsp
    sub rsp, 0x20
    lea rdi, [rel msg]
    xor eax, eax
    call printf
    add rsp, 0x20
    pop rbp
    xor eax, eax
    ret
section .data
msg: db "HELLO FROM ASM", 10, 0
