BITS 16
ORG 0x8000

; NullL BIOS stage2.
; Responsibilities:
;   1. keyboard boot menu
;   2. load the kernel using a small installer-written FAT32 extent manifest
;   3. parse ELF64 PT_LOAD segments
;   4. collect E820 memory map and BIOS RSDP
;   5. select a VBE linear framebuffer
;   6. build identity + higher-half 2 MiB paging
;   7. enter long mode and jump to the ELF entry with RDI=NullBootInfo*
;
; Disk contract:
;   partition + 2 = NullL mode mailbox
;   partition + 3 = NLLK kernel extent manifest
;   partition + 7 = this stage2
;
STAGE2_LOAD       equ 0x00008000
MANIFEST_BUF      equ 0x00006000
BOOTINFO_ADDR     equ 0x00006000
FBINFO_ADDR       equ 0x00006100
VBE_CTRL_BUF      equ 0x00006200
VBE_MODE_BUF      equ 0x00006400
E820_ADDR         equ 0x00005000
E820_MAX          equ 128
KERNEL_STAGING    equ 0x02000000
PAGE_PML4         equ 0x00011000
PAGE_PDPT         equ 0x00012000
PAGE_PD           equ 0x00013000
KERNEL_PHYS_BASE  equ 0x00100000
NULLBOOT_MAGIC_LO equ 0x55424F54
NULLBOOT_MAGIC_HI equ 0x4E554C4C
BOOTINFO_SIZE     equ 104
FBINFO_SIZE       equ 48
MANIFEST_MAX_EXT  equ 30
MAILBOX_SECTOR    equ 2
MANIFEST_SECTOR   equ 3
STAGE2_SECTORS    equ 64

start:
    jmp 0x0000:stage2_main

stage2_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7000
    sti
    mov [boot_drive], dl

    call clear_screen
    call check_edd
    jc fatal_disk

    call read_partition_start
    jc fatal_disk

    call read_mailbox
    jc fatal_disk

    cmp al, 0
    je menu
    ; A reboot-c request is intentionally one-shot at the kernel side.
    ; Native NullL still performs the same real boot path.
    mov [requested_mode], al

native_boot:
    call load_kernel
    jc fatal_kernel

    call collect_e820
    jc fatal_memory

    call find_rsdp

    call setup_vbe
    ; VBE is optional. The kernel remains usable without a framebuffer.
    
    call build_bootinfo
    call build_page_tables
    call enter_long_mode

    jmp $

menu:
    mov si, title
    call puts
    mov si, normal
    call puts
    mov si, assembly
    call puts
    mov si, npb
    call puts
    mov si, old_boot
    call puts
    mov si, prompt
    call puts

.wait:
    xor ah, ah
    int 0x16
    cmp al, '1'
    je .normal
    cmp al, '2'
    je .asm
    cmp al, '3'
    je .npb
    cmp al, '0'
    je old_record
    jmp .wait

.normal:
    xor al, al
    jmp .select
.asm:
    mov al, 2
    jmp .select
.npb:
    mov al, 3

.select:
    call write_mailbox
    jc fatal_disk
    mov [requested_mode], al
    jmp native_boot

; ---------------------------------------------------------------------------
; BIOS / disk helpers
; ---------------------------------------------------------------------------

check_edd:
    mov ah, 0x41
    mov bx, 0x55AA
    int 0x13
    jc .fail
    cmp bx, 0xAA55
    jne .fail
    test cx, 1
    jz .fail
    clc
    ret
.fail:
    stc
    ret

read_partition_start:
    mov si, 0x7C00 + 0x1BE
    mov cx, 4
.find:
    cmp byte [si], 0x80
    je .found
    add si, 16
    loop .find
    mov si, 0x7C00 + 0x1BE
.found:
    add si, 8
    mov eax, [si]
    mov [partition_start], eax
    clc
    ret

read_mailbox:
    mov eax, [partition_start]
    add eax, MAILBOX_SECTOR
    mov dword [mailbox_dap.lba_low], eax
    mov dword [mailbox_dap.lba_high], 0
    mov word [mailbox_dap.count], 1
    mov word [mailbox_dap.offset], MANIFEST_BUF
    mov word [mailbox_dap.segment], 0
    mov dword [mailbox_dap.flat_low], 0
    mov dword [mailbox_dap.flat_high], 0
    mov si, mailbox_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .fail

    cmp dword [MANIFEST_BUF], 0x314D4C4E ; NLM1
    jne .none
    mov al, [MANIFEST_BUF+4]
    cmp al, 3
    ja .none
    clc
    ret
.none:
    xor al, al
    clc
    ret
.fail:
    stc
    ret

write_mailbox:
    push ax
    mov di, MANIFEST_BUF
    xor ax, ax
    mov cx, 256
    rep stosw
    mov dword [MANIFEST_BUF], 0x314D4C4E
    pop ax
    mov [MANIFEST_BUF+4], al

    mov eax, [partition_start]
    add eax, MAILBOX_SECTOR
    mov dword [mailbox_dap.lba_low], eax
    mov dword [mailbox_dap.lba_high], 0
    mov word [mailbox_dap.count], 1
    mov word [mailbox_dap.offset], MANIFEST_BUF
    mov word [mailbox_dap.segment], 0
    mov dword [mailbox_dap.flat_low], 0
    mov dword [mailbox_dap.flat_high], 0

    mov si, mailbox_dap
    mov dl, [boot_drive]
    mov ah, 0x43
    xor al, al
    int 0x13
    ret

load_kernel:
    ; Read the installer-generated NLLK extent manifest.
    mov eax, [partition_start]
    add eax, MANIFEST_SECTOR
    mov dword [manifest_dap.lba_low], eax
    mov dword [manifest_dap.lba_high], 0
    mov word [manifest_dap.count], 1
    mov word [manifest_dap.offset], MANIFEST_BUF
    mov word [manifest_dap.segment], 0
    mov dword [manifest_dap.flat_low], 0
    mov dword [manifest_dap.flat_high], 0
    mov si, manifest_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .fail

    cmp dword [MANIFEST_BUF], 0x4B4C4C4E ; NLLK
    jne .fail
    cmp dword [MANIFEST_BUF+4], 1
    jne .fail

    mov eax, [MANIFEST_BUF+8]
    mov [kernel_size], eax
    mov eax, [MANIFEST_BUF+12]
    mov [extent_count], eax
    cmp eax, 0
    je .fail
    cmp eax, MANIFEST_MAX_EXT
    ja .fail

    ; Load every extent to the contiguous staging area at 32 MiB.
    xor ebx, ebx
.extent_loop:
    cmp ebx, [extent_count]
    jae .loaded

    mov esi, MANIFEST_BUF + 16
    mov eax, ebx
    shl eax, 4
    add esi, eax

    mov eax, [esi]          ; LBA low
    mov [kernel_dap.lba_low], eax
    mov eax, [esi+4]        ; LBA high
    mov [kernel_dap.lba_high], eax
    mov eax, [esi+8]        ; sector count
    mov [extent_remaining], eax
    mov eax, ebx
    shl eax, 4
    add esi, eax

    ; Destination = KERNEL_STAGING + bytes already loaded.
    mov eax, [loaded_bytes]
    add eax, KERNEL_STAGING
    mov [kernel_dest], eax

.read_chunk:
    mov eax, [extent_remaining]
    test eax, eax
    jz .next_extent
    cmp eax, 127
    jbe .chunk_ok
    mov eax, 127
.chunk_ok:
    mov word [kernel_dap.count], ax

    mov eax, [kernel_dest]
    mov dword [kernel_dap.flat_low], eax
    mov dword [kernel_dap.flat_high], 0
    mov word [kernel_dap.offset], 0
    mov word [kernel_dap.segment], 0

    mov si, kernel_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .fail

    movzx eax, word [kernel_dap.count]
    shl eax, 9
    add [kernel_dest], eax
    add [loaded_bytes], eax
    sub [extent_remaining], eax
    shr eax, 9
    jmp .read_chunk

.next_extent:
    inc ebx
    jmp .extent_loop

.loaded:
    mov eax, [loaded_bytes]
    cmp eax, [kernel_size]
    jb .fail

    ; Parse ELF64 header.
    mov esi, KERNEL_STAGING
    cmp dword [esi], 0x464C457F ; ELF
    jne .fail
    cmp byte [esi+4], 2         ; ELFCLASS64
    jne .fail
    cmp byte [esi+5], 1         ; little endian
    jne .fail

    mov eax, [esi+32]
    mov [phoff_lo], eax
    mov eax, [esi+36]
    mov [phoff_hi], eax
    movzx eax, word [esi+54]
    mov [phentsize], eax
    movzx eax, word [esi+56]
    mov [phnum], eax
    mov eax, [esi+24]
    mov [entry_lo], eax
    mov eax, [esi+28]
    mov [entry_hi], eax

    xor ebx, ebx
.ph_loop:
    cmp ebx, [phnum]
    jae .segments_done

    mov eax, ebx
    mul dword [phentsize]
    add eax, [phoff_lo]
    adc edx, [phoff_hi]
    ; Kernel program headers are below 4 GiB in the generated image.
    test edx, edx
    jnz .fail
    add eax, KERNEL_STAGING
    mov esi, eax

    cmp dword [esi], 1          ; PT_LOAD
    jne .next_ph

    ; p_offset
    mov eax, [esi+8]
    mov [seg_src_lo], eax
    mov eax, [esi+12]
    mov [seg_src_hi], eax
    test eax, eax
    jnz .fail
    ; p_paddr
    mov edi, [esi+24]
    mov eax, [esi+28]
    test eax, eax
    jnz .fail
    mov [seg_dst], edi
    ; p_filesz
    mov ecx, [esi+32]
    mov eax, [esi+36]
    test eax, eax
    jnz .fail
    mov [seg_filesz], ecx
    ; p_memsz
    mov eax, [esi+40]
    mov edx, [esi+44]
    test edx, edx
    jnz .fail
    mov [seg_memsz], eax

    mov eax, [seg_dst]
    cmp eax, KERNEL_PHYS_BASE
    jb .fail

    ; Copy file bytes to physical p_paddr.
    mov esi, KERNEL_STAGING
    add esi, [seg_src_lo]
    mov edi, [seg_dst]
    mov ecx, [seg_filesz]
    cld
    a32 rep movsb

    ; Zero BSS tail.
    mov eax, [seg_memsz]
    sub eax, [seg_filesz]
    jbe .next_ph
    mov ecx, eax
    xor eax, eax
    a32 rep stosb

.next_ph:
    inc ebx
    jmp .ph_loop

.segments_done:
    mov eax, [entry_lo]
    mov [kernel_entry_lo], eax
    mov eax, [entry_hi]
    mov [kernel_entry_hi], eax

    ; Compute loaded physical end from PT_LOAD segments again.
    xor ebx, ebx
    mov dword [kernel_phys_end], KERNEL_PHYS_BASE
.end_loop:
    cmp ebx, [phnum]
    jae .done_end
    mov eax, ebx
    mul dword [phentsize]
    add eax, [phoff_lo]
    adc edx, [phoff_hi]
    test edx, edx
    jnz .fail
    add eax, KERNEL_STAGING
    mov esi, eax
    cmp dword [esi], 1
    jne .end_next
    mov eax, [esi+24]
    mov edx, [esi+28]
    test edx, edx
    jnz .fail
    add eax, [esi+40]
    jc .fail
    cmp eax, [kernel_phys_end]
    jbe .end_next
    mov [kernel_phys_end], eax
.end_next:
    inc ebx
    jmp .end_loop

.done_end:
    clc
    ret
.fail:
    stc
    ret

; ---------------------------------------------------------------------------
; E820 + ACPI
; ---------------------------------------------------------------------------

collect_e820:
    mov dword [E820_ADDR], 0
    xor ebx, ebx
    xor bp, bp
.next:
    cmp bp, E820_MAX
    jae .done
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    mov di, E820_ADDR
    mov eax, ebp
    imul eax, 24
    add di, ax
    int 0x15
    jc .finish
    cmp eax, 0x534D4150
    jne .fail
    cmp ecx, 20
    jb .fail
    cmp ecx, 24
    jae .entry_ok
    mov dword [di+20], 0
.entry_ok:
    inc bp
    test ebx, ebx
    jnz .next
.finish:
    cmp bp, 0
    je .fail
.done:
    mov [e820_count], bp
    clc
    ret
.fail:
    stc
    ret

find_rsdp:
    xor eax, eax
    mov ax, [0x40E]
    shl eax, 4
    mov [rsdp_scan], eax
    mov ecx, 1024
    call scan_rsdp
    cmp dword [rsdp_found], 0
    jne .done

    mov dword [rsdp_scan], 0x000E0000
    mov ecx, 0x20000
    call scan_rsdp
.done:
    ret

scan_rsdp:
.loop:
    cmp ecx, 0
    je .done
    mov esi, [rsdp_scan]
    test esi, 0x0F
    jnz .next
    cmp dword [esi], 0x20524453
    jne .next
    cmp dword [esi+4], 0x20525450
    jne .next
    xor eax, eax
    xor edx, edx
.sum:
    add dl, [esi+edx]
    inc edx
    cmp edx, 20
    jb .sum
    test dl, dl
    jnz .next
    mov [rsdp_found], esi
    ret
.next:
    add dword [rsdp_scan], 16
    dec ecx
    jmp .loop
.done:
    ret

; ---------------------------------------------------------------------------
; VBE
; ---------------------------------------------------------------------------

setup_vbe:
    mov dword [FBINFO_ADDR], 0
    mov dword [FBINFO_ADDR+4], 0

    mov dword [VBE_CTRL_BUF], 0x32454256 ; "VBE2"
    mov ax, 0x4F00
    mov di, VBE_CTRL_BUF
    int 0x10
    cmp ax, 0x004F
    jne .none

    ; mode list pointer: controller + 0x0E, far pointer.
    mov si, [VBE_CTRL_BUF+0x0E]
    mov ax, [VBE_CTRL_BUF+0x10]
    mov [mode_ptr_off], si
    mov [mode_ptr_seg], ax

.next_mode:
    mov ax, [mode_ptr_seg]
    mov es, ax
    mov si, [mode_ptr_off]
    movzx eax, word [es:si]
    cmp ax, 0xFFFF
    je .none

    mov [candidate_mode], ax
    mov ax, 0x4F01
    mov cx, [candidate_mode]
    mov di, VBE_MODE_BUF
    int 0x10
    cmp ax, 0x004F
    jne .advance

    mov ax, [VBE_MODE_BUF]
    test ax, 0x0080
    jz .advance
    cmp byte [VBE_MODE_BUF+27], 6
    jne .advance
    cmp byte [VBE_MODE_BUF+25], 32
    jne .advance

    ; Prefer 1024x768, otherwise keep the first valid 32-bit LFB mode.
    mov ax, [VBE_MODE_BUF+18]
    cmp ax, 1024
    jne .keep_first
    mov ax, [VBE_MODE_BUF+20]
    cmp ax, 768
    jne .keep_first
    mov ax, [candidate_mode]
    mov [chosen_mode], ax
    jmp .set

.keep_first:
    cmp word [chosen_mode], 0
    jne .advance
    mov ax, [candidate_mode]
    mov [chosen_mode], ax

.advance:
    add word [mode_ptr_off], 2
    jmp .next_mode

.set:
    cmp word [chosen_mode], 0
    je .none
    mov ax, 0x4F02
    mov bx, [chosen_mode]
    or bx, 0x4000
    int 0x10
    cmp ax, 0x004F
    jne .none

    mov ax, 0x4F01
    mov cx, [chosen_mode]
    mov di, VBE_MODE_BUF
    int 0x10
    cmp ax, 0x004F
    jne .none

    xor eax, eax
    mov ax, [VBE_MODE_BUF+18]
    mov [FBINFO_ADDR+24], eax
    mov ax, [VBE_MODE_BUF+20]
    mov [FBINFO_ADDR+28], eax
    mov eax, [VBE_MODE_BUF+16]
    mov [FBINFO_ADDR+32], eax
    movzx eax, byte [VBE_MODE_BUF+25]
    mov [FBINFO_ADDR+36], ax
    movzx eax, byte [VBE_MODE_BUF+27]
    mov [FBINFO_ADDR+38], ax
    mov eax, [VBE_MODE_BUF+40]
    mov [FBINFO_ADDR+8], eax
    mov dword [FBINFO_ADDR+12], 0
    ; 32-bit physical address is enough for legacy VBE on normal x86 PCs.
    mov dword [FBINFO_ADDR+16], eax
    mov dword [FBINFO_ADDR+20], 0

    xor ax, ax
    mov es, ax

    mov al, [VBE_MODE_BUF+21]
    mov [FBINFO_ADDR+40], al
    mov al, [VBE_MODE_BUF+22]
    mov [FBINFO_ADDR+41], al
    mov al, [VBE_MODE_BUF+23]
    mov [FBINFO_ADDR+42], al
    mov al, [VBE_MODE_BUF+24]
    mov [FBINFO_ADDR+43], al
    mov al, [VBE_MODE_BUF+25]
    mov [FBINFO_ADDR+44], al
    mov byte [FBINFO_ADDR+45], 0
    mov byte [FBINFO_ADDR+46], 0
    mov byte [FBINFO_ADDR+47], 0

    ; Address is filled as higher-half alias in build_bootinfo.
    clc
    ret
.none:
    ; A zero framebuffer pointer is a valid "headless" result.
    mov dword [FBINFO_ADDR], 0
    mov dword [FBINFO_ADDR+4], 0
    xor ax, ax
    mov es, ax
    clc
    ret

; ---------------------------------------------------------------------------
; BootInfo
; ---------------------------------------------------------------------------

build_bootinfo:
    ; Clear BootInfo.
    mov di, BOOTINFO_ADDR
    xor eax, eax
    mov cx, BOOTINFO_SIZE/4
    rep stosd

    mov dword [BOOTINFO_ADDR+0], NULLBOOT_MAGIC_LO
    mov dword [BOOTINFO_ADDR+4], NULLBOOT_MAGIC_HI
    mov dword [BOOTINFO_ADDR+8], 1
    mov dword [BOOTINFO_ADDR+12], BOOTINFO_SIZE

    mov dword [BOOTINFO_ADDR+16], E820_ADDR
    mov dword [BOOTINFO_ADDR+20], 0
    movzx eax, word [e820_count]
    mov dword [BOOTINFO_ADDR+24], eax

    mov dword [BOOTINFO_ADDR+32], FBINFO_ADDR
    mov dword [BOOTINFO_ADDR+36], 0
    mov eax, [rsdp_found]
    mov dword [BOOTINFO_ADDR+40], eax
    mov dword [BOOTINFO_ADDR+44], 0
    movzx eax, byte [boot_drive]
    mov dword [BOOTINFO_ADDR+48], eax
    mov dword [BOOTINFO_ADDR+52], 0

    mov dword [BOOTINFO_ADDR+56], KERNEL_PHYS_BASE
    mov dword [BOOTINFO_ADDR+60], 0
    mov eax, [kernel_phys_end]
    mov dword [BOOTINFO_ADDR+64], eax
    mov dword [BOOTINFO_ADDR+68], 0
    mov eax, [kernel_entry_lo]
    mov dword [BOOTINFO_ADDR+72], eax
    mov eax, [kernel_entry_hi]
    mov dword [BOOTINFO_ADDR+76], eax
    mov dword [BOOTINFO_ADDR+80], 0
    mov dword [BOOTINFO_ADDR+84], 0
    mov dword [BOOTINFO_ADDR+88], STAGE2_LOAD
    mov dword [BOOTINFO_ADDR+92], STAGE2_LOAD + STAGE2_SECTORS*512
    mov dword [BOOTINFO_ADDR+96], 0
    mov dword [BOOTINFO_ADDR+100], 0

    ; Turn VBE physical framebuffer into the kernel higher-half alias.
    mov eax, [FBINFO_ADDR+8]
    test eax, eax
    jz .done
    mov [FBINFO_ADDR], eax
    ; The boot page tables map the first 1 GiB at 0xffffffff80000000.
    add dword [FBINFO_ADDR], 0x80000000
    mov dword [FBINFO_ADDR+4], 0xFFFFFFFF
    mov eax, [FBINFO_ADDR+32]
    mul dword [FBINFO_ADDR+28]
    mov dword [FBINFO_ADDR+16], eax
    mov dword [FBINFO_ADDR+20], edx
.done:
    ret

; ---------------------------------------------------------------------------
; Page tables / long mode
; ---------------------------------------------------------------------------

build_page_tables:
    ; Clear 3 pages.
    mov di, PAGE_PML4
    xor eax, eax
    mov cx, 3072/4
    rep stosd

    ; PML4[0] -> PDPT, PML4[511] -> PDPT.
    mov dword [PAGE_PML4+0], PAGE_PDPT | 3
    mov dword [PAGE_PML4+4], 0
    mov dword [PAGE_PML4+511*8], PAGE_PDPT | 3
    mov dword [PAGE_PML4+511*8+4], 0

    ; PDPT[0] and [510] and [511] -> PD.
    mov dword [PAGE_PDPT+0], PAGE_PD | 3
    mov dword [PAGE_PDPT+510*8], PAGE_PD | 3
    mov dword [PAGE_PDPT+511*8], PAGE_PD | 3

    ; 2 MiB identity map for the first 1 GiB. This simultaneously provides
    ; the higher-half aliases at ffffffff80000000 and ffffffffC0000000.
    xor ecx, ecx
.pd:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83
    mov edi, PAGE_PD
    mov edx, ecx
    shl edx, 3
    add edi, edx
    mov [edi], eax
    mov dword [edi+4], 0
    inc ecx
    cmp ecx, 512
    jb .pd

    ret

enter_long_mode:
    cli
    lgdt [gdt64_descriptor]

    mov eax, PAGE_PML4
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp 0x08:long_mode_entry

BITS 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax

    mov rsp, 0x0000000000018000
    and rsp, -16
    sub rsp, 8

    mov rdi, BOOTINFO_ADDR
    mov rax, [BOOTINFO_ADDR+72]
    jmp rax

fatal_disk:
    mov si, msg_disk
    jmp fatal
fatal_kernel:
    mov si, msg_kernel
    jmp fatal
fatal_memory:
    mov si, msg_memory
    jmp fatal

fatal:
    call puts
    cli
.hang:
    hlt
    jmp .hang

old_record:
    mov si, loading_old
    call puts
    mov word [old_dap.count], 1
    mov word [old_dap.offset], 0x7C00
    mov word [old_dap.segment], 0
    mov eax, [partition_start]
    dec eax
    mov dword [old_dap.lba_low], eax
    mov dword [old_dap.lba_high], 0
    mov si, old_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc old_error
    cmp word [0x7C00+510], 0xAA55
    jne old_error
    mov dl, [boot_drive]
    jmp 0x0000:0x7C00
old_error:
    mov si, old_fail
    jmp fatal

clear_screen:
    mov ax, 0x0003
    int 0x10
    ret

puts:
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .next
.done:
    ret

boot_drive db 0
requested_mode db 0
partition_start dd 0
kernel_size dd 0
loaded_bytes dd 0
extent_count dd 0
extent_remaining dd 0
kernel_dest dd 0
phoff_lo dd 0
phoff_hi dd 0
phentsize dd 0
phnum dd 0
entry_lo dd 0
entry_hi dd 0
seg_src_lo dd 0
seg_src_hi dd 0
seg_dst dd 0
seg_filesz dd 0
seg_memsz dd 0
kernel_entry_lo dd 0
kernel_entry_hi dd 0
kernel_phys_end dd KERNEL_PHYS_BASE
kernel_phys_base dd KERNEL_PHYS_BASE
e820_count dw 0
rsdp_scan dd 0
rsdp_found dd 0
mode_ptr_off dw 0
mode_ptr_seg dw 0
candidate_mode dw 0
chosen_mode dw 0

title db 13,10,"NULLLOADER - NATIVE BOOT CONTROL",13,10,0
normal db "  [1] Normal-Mode",13,10,0
assembly db "  [2] Assembly-Mode",13,10,0
npb db "  [3] NullOS Pre-Boot",13,10,0
old_boot db "  [0] Old Boot Record",13,10,0
prompt db "Command: ",0
loading_old db 13,10,"Loading Old Boot Record...",13,10,0
old_fail db "NullL: Old Boot Record invalid",13,10,0
msg_disk db "NullL: disk error",13,10,0
msg_kernel db "NullL: kernel load/ELF error",13,10,0
msg_memory db "NullL: E820 memory map error",13,10,0

align 4, db 0
mailbox_dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
.flat_low dd 0
.flat_high dd 0

align 4, db 0
manifest_dap:
    db 0x18, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
.flat_low dd 0
.flat_high dd 0

align 4, db 0
kernel_dap:
    db 0x18, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
.flat_low dd 0
.flat_high dd 0

align 4, db 0
old_dap:
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
.flat_low dd 0
.flat_high dd 0

align 8
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
gdt64_descriptor:
    dw 23
    dd gdt64

times STAGE2_SECTORS*512-($-$$) db 0
