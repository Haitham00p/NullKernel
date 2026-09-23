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
; Common IRQ stub
;
; The CPU pushed (ring-0 interrupt, no privilege change) RFLAGS, CS, RIP
; onto the *currently active* stack. We switch to a dedicated kernel IRQ
; stack, save ALL volatile (caller-saved) registers plus the XMM state, call
; the C handler, then restore everything and resume the interrupted thread.
;
; Without saving the volatile registers the C handler would clobber them and
; the resumed thread would observe corrupted values (e.g. a pointer argument
; passed in rdi), which is exactly what caused the installer's NULL-pointer
; faults. r12 (callee-saved) carries the guest iret-frame pointer across the
; call because C functions preserve it.
; =====================================================

%macro IRQ_STUB 1
IRQ%1:
    cli

    mov r12, rsp
    mov rsp, IrqStackTop

    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    fxsave [IrqFxSave]

    push qword 0
    push qword [r12 + 24]
    push qword [r12 + 16]
    push qword [r12 + 8]
    push qword [r12 + 0]

    mov rsi, rsp
    mov rdi, %1
    call IRQHandler

    mov rax, [rsp + 0]
    mov [r12 + 0],  rax
    mov rax, [rsp + 8]
    mov [r12 + 8],  rax
    mov rax, [rsp + 16]
    mov [r12 + 16], rax

    add rsp, 40

    fxrstor [IrqFxSave]

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    mov rsp, r12
    iretq
%endmacro

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15


section .bss
align 16
IrqFxSave:
    resb 512
IrqStack:
    resb 65536
IrqStackTop:
