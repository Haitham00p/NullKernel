BITS 64

global TaskSwitch
global TaskStartFirst
global TaskYieldTrap

extern TaskYieldFromInterrupt

; Legacy cooperative context switch kept as a low-level primitive.
TaskSwitch:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, [rsi]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

; Start a task from a prebuilt IRQContext.
TaskStartFirst:
    mov rsp, rdi

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq

; Software yield path. It has the exact same frame format as a timer IRQ.
TaskYieldTrap:
    cli

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call TaskYieldFromInterrupt

    mov r12, rax
    mov rsp, r12

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq
