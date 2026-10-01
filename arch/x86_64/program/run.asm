; Copyright (C) 2026 Haitham Al-Nasser
; This file is part of NullOS.
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
; See the GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with NullOS. If not, see <https://www.gnu.org/licenses/>.
;
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

; Dedicated guest stack.  Lives inside the kernel image (higher-half,
; always mapped and writable), so a guest program can never depend on the
; low flat identity-map window reaching 0x300000.  Using a kernel-resident
; buffer guarantees the stack is valid on every machine regardless of how
; much low RAM the bootloader identity-maps.  Aligned to 16 so the ABI
; 16-byte stack alignment holds at the guest entry point.
align 16
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
    mov qword [RunSavedRsp], rsp
    mov qword [RunAbortArmed], 0
    mov qword [RunGuestActive], 1
    mov rsp, GuestStackEnd
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