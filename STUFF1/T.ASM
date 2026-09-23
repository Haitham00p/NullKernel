bits 64
section .text
global main
extern printf
main:
    sub rsp, 8
    lea rdi, [rel msg]
    xor eax, eax
    call printf
    add rsp, 8
    xor eax, eax
    ret
section .data
msg: db "HELLO FROM ASM", 10, 0
