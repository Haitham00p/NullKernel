bits 64

section .text
global start
extern TerminalPrintLine32
extern KernelSleep
start:
    mov rdi, msg0
    mov esi, 0xFF00FFFF
    xor eax, eax
    call TerminalPrintLine32
    mov rdi, 1000
    call KernelSleep
    mov rdi, msg1
    mov esi, 0xFFFF00FF
    xor eax, eax
    call TerminalPrintLine32
    ret

section .data
msg0: db "Hello Again!", 0
msg1: db "This is asm!", 0
