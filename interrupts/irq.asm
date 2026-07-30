[BITS 64]

global IRQ0
global IRQ1
global IRQ2
global IRQ3
global IRQ4
global IRQ5
global IRQ6
global IRQ7
global IRQ8
global IRQ9
global IRQ10
global IRQ11
global IRQ12
global IRQ13
global IRQ14
global IRQ15

extern IRQHandler


; =====================================================
; IRQ1 Keyboard
; =====================================================

IRQ1:

    cli

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp

    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15


    mov rdi, 1


    ; align stack for C function call
    sub rsp, 8

    call IRQHandler

    add rsp, 8


    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8

    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax


    sti
    iretq



; =====================================================
; Empty IRQs
; =====================================================

%macro IRQ_EMPTY 1

IRQ%1:

    cli


    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp

    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15


    mov rdi, %1


    sub rsp, 8

    call IRQHandler

    add rsp, 8



    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8


    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax


    sti
    iretq


%endmacro



IRQ_EMPTY 0
IRQ_EMPTY 2
IRQ_EMPTY 3
IRQ_EMPTY 4
IRQ_EMPTY 5
IRQ_EMPTY 6
IRQ_EMPTY 7

IRQ_EMPTY 8
IRQ_EMPTY 9
IRQ_EMPTY 10
IRQ_EMPTY 11
IRQ_EMPTY 12
IRQ_EMPTY 13
IRQ_EMPTY 14
IRQ_EMPTY 15