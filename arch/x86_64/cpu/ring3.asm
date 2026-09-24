BITS 64
global Ring3Enter
global Ring3Int80
extern Ring3SyscallHandler
Ring3Enter:
    cli
    mov rax, rsp
    push qword 0x3B
    push rsi
    pushfq
    or qword [rsp], 0x200
    push qword 0x43
    push rdi
    iretq
Ring3Int80:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    mov r8, rcx
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    call Ring3SyscallHandler
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
