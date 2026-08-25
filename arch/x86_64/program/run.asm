; Runs a raw user-program entry point on a dedicated guest stack.
; The guest shares the kernel's ring-0 page tables (flat identity map),
; so it only gets its own stack; the caller-provided RSP is restored on
; return so a user program can never smash the kernel shell stack.
;
; Interrupts stay enabled during the guest run: the IRQ stubs push/pop
; every register and iretq, so PIT timing and the keyboard keep working
; even if a user program loops forever.  The guest stack (fixed,
; identity-mapped low RAM) is safe for the kernel handlers.
;
; Ctrl+C while a guest program is frozen is handled cooperatively:
; the keyboard driver sets its abort flag, and on the next PIT tick the
; IRQ stub replaces its iretq frame so the CPU resumes at RunAbortResume
; below with the saved kernel-shell stack.  RunProgramOnStack then
; "returns" to the shell with the NULLOS_GUEST_ABORTED sentinel in RAX.
;
; The caller RSP is kept in memory, not a register, because the guest is a
; normal C function that returns its result in RAX (among other things).
;
; uint64_t RunProgramOnStack(void (*Entry)(void), uint64_t StackTop);
;
;   rdi = entry point
;   rsi = top of the dedicated stack
;   returns the guest's RAX at exit (the program's return value),
;   or NULLOS_GUEST_ABORTED when the user pressed Ctrl+C
[bits 64]

section .text
global RunProgramOnStack

extern RunSavedRsp
extern RunGuestActive
; address used by the interrupt abort path - normally unreferenced
; (the IRQ stub writes this label's address directly into its iret frame)
global RunAbortResume

section .data
global RunGuestActive
RunGuestActive:
    dq 0
global RunAbortArmed
RunAbortArmed:
    dq 0

section .bss
global RunSavedRsp
RunSavedRsp:
    resq 1

section .text

RunProgramOnStack:
    mov qword [RunSavedRsp], rsp
    mov qword [RunAbortArmed], 0
    mov qword [RunGuestActive], 1
    mov rsp, rsi
    sti
    call rdi
full_switchless:
    mov qword [RunAbortArmed], 0
    mov qword [RunGuestActive], 0
    mov rsp, [RunSavedRsp]
    ret

RunAbortResume:
    mov qword [RunAbortArmed], 0
    mov qword [RunGuestActive], 0
    mov rax, 0x2BAD0B5E
    ret