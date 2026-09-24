BITS 64
global Ring3Enter
global Ring3Int80
extern Ring3SyscallHandler
Ring3Enter:
    cli
    mov rax, rsp
    push qword 0x18|3
    push rsi
    pushfq
    or qword [rsp], 0x200
    push qword 0x20|3
    push rdi
    iretq
Ring3Int80:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    call Ring3SyscallHandler
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
