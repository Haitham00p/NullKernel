bits 64

section .data
    fmt_in:  db "%ld", 0
    fmt_out: db "You Entered: %ld", 10, 0

section .bss
    val: resq 1

section .text

global main
extern scanf
extern printf

main:
    ; align the stack to 16 bytes. n64a's push only takes an immediate and
    ; does not support `and rsp, -16` (negative immediate), so we use sub/add.
    ; RunProgramOnStack enters us with rsp % 16 == 8; sub rsp,8 -> 0 (aligned).
    sub rsp, 8

    ; scanf("%ld", &val)
    lea rdi, [rel fmt_in]
    lea rsi, [rel val]
    xor eax, eax
    call scanf

    ; printf("You Entered: %ld\n", val)
    lea rdi, [rel fmt_out]
    mov rsi, [rel val]      ; <-- value, not address
    xor eax, eax
    call printf

    add rsp, 8              ; restore stack so ret reads the real return addr
    xor eax, eax
    ret
