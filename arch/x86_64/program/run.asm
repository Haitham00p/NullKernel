; Runs a raw user-program entry point on a dedicated guest stack.
; The guest shares the kernel's ring-0 page tables, but runs on a dedicated
; kernel-resident higher-half stack; the caller RSP is restored on return
; so a guest program can never smash the kernel shell stack.
;
; Interrupts stay enabled during the guest run: the IRQ stubs push/pop
; every register and iretq, so PIT timing and the keyboard keep working
; even if a user program loops forever.  The guest stack (fixed,
; higher-half BSS) is mapped and safe for the kernel handlers.
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

; RunSavedRsp and RunGuestActive are defined and exported in this file.
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

; Dedicated guest stack.  Lives inside the kernel image (higher-half,
; always mapped and writable), so a guest program can never depend on the
; low flat identity-map window reaching 0x300000.  Using a kernel-resident
; buffer guarantees the stack is valid on every machine regardless of how
; much low RAM the bootloader identity-maps.  Aligned to 16 so the ABI
; 16-byte stack alignment holds at the guest entry point.
alignb 16
GuestStack:
    resb 0x8000
GuestStackEnd:

section .text

RunProgramOnStack:
    ; Save the caller's (shell's) callee-saved registers on the shell stack
    ; before we switch to the guest stack. A guest that returns normally
    ; restores these itself (per the C ABI), but an aborted guest never runs
    ; its epilogue, so without stashing them here its live values would leak
    ; back into the shell and corrupt cursor/prompt state after Ctrl+C.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov qword [rel RunSavedRsp], rsp
    mov qword [rel RunAbortArmed], 0
    mov qword [rel RunGuestActive], 1
    lea rsp, [rel GuestStackEnd]
    sti
    call rdi
full_switchless:
    mov qword [RunAbortArmed], 0
    mov qword [RunGuestActive], 0
    jmp restore_and_return

RunAbortResume:
    mov qword [RunAbortArmed], 0
    mov qword [RunGuestActive], 0
    mov rax, 0x2BAD0B5E
    ; The Ctrl+C abort resumes here instead of at full_switchless. The guest's
    ; own stack and registers are abandoned; restore the kernel/shell stack and
    ; the shell's callee-saved registers so the aborted program can never
    ; resume on (or return to) its own stale RIP/RSP/registers.
    jmp restore_and_return

restore_and_return:
    ; A ring-0 iretq does not reload RSP, so restore the saved shell RSP
    ; explicitly, then pop the callee-saved registers the guest never restored.
    mov rsp, [RunSavedRsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret