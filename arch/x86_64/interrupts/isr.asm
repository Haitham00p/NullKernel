[BITS 64]

global ISR0
global ISR1
global ISR2
global ISR3
global ISR4
global ISR5
global ISR6
global ISR7
global ISR8
global ISR9
global ISR10
global ISR11
global ISR12
global ISR13
global ISR14
global ISR15
global ISR16
global ISR17
global ISR18
global ISR19
global ISR20
global ISR21
global ISR22
global ISR23
global ISR24
global ISR25
global ISR26
global ISR27
global ISR28
global ISR29
global ISR30
global ISR31

extern ExceptionHandler

; Push general registers in the order expected by interrupt_frame_t so that
; rax ends up at the lowest address (rsp+0). We push r15 first ... rax last.
%macro PUSH_REGS 0
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
%endmacro


; Vectors that the CPU pushes an error code for. For the others we synthesize
; a zero error code so the saved frame layout is uniform across all vectors.
%macro ISR_NOERR 1
ISR%1:
    cli
    push 0
    push %1
    jmp ISRCommon
%endmacro

%macro ISR_ERR 1
ISR%1:
    cli
    push %1
    jmp ISRCommon
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31


ISRCommon:
    ; At this point the faulting stack holds, from low to high:
    ;   [rsp+0   ] vector (pushed by ISR stub)
    ;   [rsp+8   ] error code (real for ERR vectors, 0 for NOERR)
    ;   [rsp+16  ] RIP        (pushed by the CPU)
    ;   [rsp+24  ] CS
    ;   [rsp+32  ] RFLAGS
    ; Stash the old stack pointer and switch to the dedicated exception stack
    ; so saving the frame can never fault due to the faulting context.
    mov rbx, rsp

    ; Preserve the first exception before switching stacks or entering C.
    ; Record: vector, error code, faulting RIP, original RSP, magic.
    cmp dword [abs 0x6E60], 0x49535231
    je .record_done
    mov rax, [rbx+0]
    mov [abs 0x6E40], rax
    mov rax, [rbx+8]
    mov [abs 0x6E48], rax
    mov rax, [rbx+16]
    mov [abs 0x6E50], rax
    mov [abs 0x6E58], rbx
    mov dword [abs 0x6E60], 0x49535231
.record_done:
    ; RIP-relative LEA is required here: an absolute immediate can resolve to
    ; the .bss section offset instead of its final higher-half virtual address.
    lea rsp, [rel ExcStackTop]

    ; Rebuild the uniform frame on the exception stack, with rax at rsp+0:
    ;   push rflags, cs, rip, error_code, vector  (vector ends up lowest)
    ; then PUSH_REGS (rax last -> rax at rsp+0)
    push qword [rbx+32]
    push qword [rbx+24]
    push qword [rbx+16]
    push qword [rbx+8]
    push qword [rbx+0]

    PUSH_REGS

    ; rdi = pointer to the uniform interrupt_frame_t laid out below us:
    ;   [rsp+0    .. +112] general registers (rax..r15, rax at +0)
    ;   [rsp+120]         vector number
    ;   [rsp+128]         error code (real for ERR vectors, 0 for NOERR)
    ;   [rsp+136]         RIP
    ;   [rsp+144]         CS
    ;   [rsp+152]         RFLAGS
    mov rdi, rsp
    call ExceptionHandler

.halt:
    cli
    hlt
    jmp .halt


; Dedicated kernel stack for exception handlers. The handler must NOT rely on
; the faulting context's stack (a guest/ring-0 stack can be exhausted or
; corrupt, which would fault again while saving the frame and triple-fault).
; Placed in .bss (after the .text above) so the handler code stays executable.
section .bss
align 16
ExcStack:
    resb 8192
ExcStackTop:

