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
KERNEL_STAGING    equ 0x00010000
KERNEL_STAGING_END equ 0x000A0000
KERNEL_STAGING_CAPACITY equ KERNEL_STAGING_END - KERNEL_STAGING
PAGE_PML4         equ 0x00011000
PAGE_PDPT         equ 0x00012000
PAGE_PD           equ 0x00013000
PAGE_FBPD         equ 0x00014000
FB_VA_LO          equ 0xC0000000
FB_VA_HI          equ 0xFFFFFFFF
KERNEL_PHYS_BASE  equ 0x00100000
NULLBOOT_MAGIC_LO equ 0x55424F54
NULLBOOT_MAGIC_HI equ 0x4E554C4C
BOOTINFO_SIZE     equ 104
FBINFO_SIZE       equ 48
MANIFEST_MAX_EXT  equ 30
MAILBOX_SECTOR    equ 2
MANIFEST_SECTOR   equ 3
STAGE2_SECTORS    equ 48

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
    call serial_init
    mov si, serial_entered
    call serial_puts

    call clear_screen
    call check_edd
    jc fatal_disk
    call enable_a20

    call read_partition_start
    jc fatal_disk

    call read_mailbox
    jc fatal_disk

    cmp al, 0
    je menu
    ; A previously stored mode bypasses the menu. Log this explicitly so
    ; the serial trace distinguishes auto-boot from a dead keyboard loop.
    mov [requested_mode], al
    mov si, serial_auto_mode
    call serial_puts

native_boot:
    mov si, serial_loading_kernel
    call serial_puts
    call load_kernel
    jc fatal_kernel
    mov si, serial_kernel_loaded
    call serial_puts

    call collect_e820
    jc fatal_memory
    mov si, serial_e820_ok
    call serial_puts
    xor ax, ax
    mov ds, ax
    mov es, ax
    call enable_unreal_mode

    call find_rsdp
    mov si, serial_rsdp_done
    call serial_puts

    ; VBE firmware may need interrupts; DS/ES are conventional segments here.
    sti
    call setup_vbe
    cli
    ; VBE is optional. The kernel remains usable without a framebuffer.
    xor ax, ax
    mov ds, ax
    mov es, ax
    call enable_unreal_mode
    mov si, serial_vbe_done
    call serial_puts

    call build_bootinfo
    call build_page_tables
    mov si, serial_long_mode
    call serial_puts
    call enter_long_mode

    jmp $

menu:
    mov si, serial_menu
    call serial_puts
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
    call keyboard_get_menu_key
    test al, al
    jz .wait
    mov [last_key], al
    mov si, serial_key
    call serial_puts
    mov al, [last_key]
    call serial_putc
    mov al, 13
    call serial_putc
    mov al, 10
    call serial_putc
    mov al, [last_key]
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
    mov al, 1
    mov [requested_mode], al
    mov si, serial_select_1
    call serial_puts
    jmp .select
.asm:
    mov al, 2
    mov [requested_mode], al
    mov si, serial_select_2
    call serial_puts
    jmp .select
.npb:
    mov al, 3
    mov [requested_mode], al
    mov si, serial_select_3
    call serial_puts

.select:
    mov al, [requested_mode]
    call write_mailbox
    jc fatal_disk
    jmp native_boot

; ---------------------------------------------------------------------------
; COM1 diagnostics. Serial output is best-effort: if the UART is absent or
; disabled, bounded polling ensures it cannot hang the boot path.
; ---------------------------------------------------------------------------
serial_init:
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x03
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    ret

serial_puts:
.next:
    lodsb
    test al, al
    jz .done
    call serial_putc
    jmp .next
.done:
    ret

; Input AL=byte. Preserves AX and DX; skips output if the UART never becomes
; ready, so a missing COM1 cannot freeze NullL.
serial_putc:
    push ax
    push dx
    push cx
    mov cx, 0xFFFF
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jnz .ready
    loop .wait
    pop cx
    pop dx
    pop ax
    ret
.ready:
    pop cx
    mov dx, 0x3F8
    pop ax
    out dx, al
    pop dx
    ret

; ---------------------------------------------------------------------------
; Menu keyboard input
; ---------------------------------------------------------------------------
; Prefer the BIOS keyboard API, but fall back to the 8042 controller when
; firmware does not expose the key through INT 16h. Return ASCII in AL.
keyboard_get_menu_key:
.wait:
    mov ah, 0x01
    int 0x16
    jz .controller
    xor ah, ah
    int 0x16
    cmp al, '1'
    je .key1
    cmp al, '2'
    je .key2
    cmp al, '3'
    je .key3
    cmp al, '0'
    je .key0
    xor al, al
    ret

.controller:
    in al, 0x64
    test al, 1
    jz .wait
    in al, 0x60
    ; Ignore key releases and multi-byte scan-code prefixes.
    test al, 0x80
    jnz .wait
    cmp al, 0xF0
    je .wait
    cmp al, 0xE0
    je .wait
    ; Set-1 make codes and common Set-2 make codes.
    cmp al, 0x02
    je .key1
    cmp al, 0x16
    je .key1
    cmp al, 0x03
    je .key2
    cmp al, 0x1E
    je .key2
    cmp al, 0x04
    je .key3
    cmp al, 0x26
    je .key3
    cmp al, 0x0B
    je .key0
    cmp al, 0x45
    je .key0
    xor al, al
    ret
.key1:
    mov al, '1'
    ret
.key2:
    mov al, '2'
    ret
.key3:
    mov al, '3'
    ret
.key0:
    mov al, '0'
    ret

; ---------------------------------------------------------------------------
; A20 gate
; ---------------------------------------------------------------------------
enable_a20:
    ; Fast A20 gate (port 0x92). Preserve the system-reset bit 0.
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al
    ret

; ---------------------------------------------------------------------------
; BIOS / disk helpers
; ---------------------------------------------------------------------------

check_edd:
    ; BIOS video/serial helpers may change DL; restore the boot drive.
    mov dl, [boot_drive]
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
    mov si, serial_manifest_read
    call serial_puts
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
    mov si, serial_manifest_read_ok
    call serial_puts

    cmp dword [MANIFEST_BUF], 0x4B4C4C4E ; NLLK
    jne .fail
    cmp dword [MANIFEST_BUF+4], 1
    jne .fail

    mov eax, [MANIFEST_BUF+8]
    test eax, eax
    jz .fail
    cmp eax, KERNEL_STAGING_CAPACITY
    ja .fail
    mov [kernel_size], eax
    mov eax, [MANIFEST_BUF+12]
    mov [extent_count], eax
    cmp eax, 0
    je .fail
    cmp eax, MANIFEST_MAX_EXT
    ja .fail
    mov si, serial_manifest_valid
    call serial_puts

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

    ; Destination = KERNEL_STAGING + bytes already loaded.
    mov eax, [loaded_bytes]
    add eax, KERNEL_STAGING
    mov [kernel_dest], eax

.read_chunk:
    mov eax, [extent_remaining]
    test eax, eax
    jz .next_extent
    cmp eax, 127
    jbe .limit_64k
    mov eax, 127
.limit_64k:
    ; EDD legacy transfers must not cross a physical 64 KiB boundary.
    mov edx, [kernel_dest]
    and edx, 0xFFFF
    mov ecx, 0x10000
    sub ecx, edx
    shr ecx, 9
    cmp eax, ecx
    jbe .chunk_ok
    mov eax, ecx
.chunk_ok:
    mov word [kernel_dap.count], ax
    ; Preserve the requested count: BIOS may modify the DAP count field.
    movzx eax, ax
    mov [chunk_sectors], eax
    shl eax, 9
    mov [chunk_bytes], eax

    ; Stage the kernel below 1 MiB using the portable 16-byte EDD DAP.
    ; Each chunk is <= 127 sectors and begins at a 16-byte boundary, so
    ; it cannot cross the 64 KiB transfer boundary.
    mov edx, [kernel_dest]
    add edx, [chunk_bytes]
    jc .fail
    cmp edx, KERNEL_STAGING_END
    ja .fail
    mov eax, [kernel_dest]
    shr eax, 4
    mov [kernel_dap.segment], ax
    mov word [kernel_dap.offset], 0

    mov si, serial_chunk_read
    call serial_puts
    mov si, kernel_dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .fail
    mov si, serial_chunk_read_ok
    call serial_puts

    ; The BIOS read completed into the low staging buffer.
    mov eax, [chunk_bytes]
    add [kernel_dest], eax
    add [loaded_bytes], eax
    mov eax, [chunk_sectors]
    sub [extent_remaining], eax
    jmp .read_chunk

.next_extent:
    inc ebx
    jmp .extent_loop

.loaded:
    mov eax, [loaded_bytes]
    cmp eax, [kernel_size]
    jb .fail
    mov si, serial_chunks_done
    call serial_puts

    ; Enable flat DS/ES limits for ELF parsing and copies to physical memory
    ; above 1 MiB. The BIOS disk reads themselves used only a 16-byte DAP.
    call enable_unreal_mode
    mov si, serial_unreal_ready
    call serial_puts

    ; Parse ELF64 header.
    mov esi, KERNEL_STAGING
    cmp dword [esi], 0x464C457F ; ELF
    jne .fail
    cmp byte [esi+4], 2         ; ELFCLASS64
    jne .fail
    cmp byte [esi+5], 1         ; little endian
    jne .fail
    mov si, serial_elf_header_ok
    call serial_puts
    mov esi, KERNEL_STAGING

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
    mov si, serial_elf_segments_done
    call serial_puts
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
; Unreal mode: retain flat 4 GiB DS/ES caches while returning to real mode.
; BIOS disk I/O stays in real mode; after loading the ELF into low memory,
; these flat segment limits let the loader parse it and copy PT_LOAD segments
; to physical addresses above 1 MiB without using EDD 3.0 flat-buffer packets.
; ---------------------------------------------------------------------------
enable_unreal_mode:
    cli
    lgdt [gdt_unreal_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    ; Keep 16-bit code semantics while loading 4 GiB data descriptors.
    jmp 0x08:.pm16
.pm16:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov eax, cr0
    and eax, 0xFFFFFFFE
    mov cr0, eax
    jmp 0x0000:.real16
.real16:
    ; Keep IRQs disabled while using the cached flat DS/ES limits. A BIOS
    ; IRQ handler could reload a segment register and destroy unreal mode.
    ret

; ---------------------------------------------------------------------------
; E820 + ACPI
; ---------------------------------------------------------------------------

collect_e820:
    ; E820 is a BIOS real-mode service; use conventional real-mode segments.
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov dword [E820_ADDR], 0
    xor ebx, ebx
    xor bp, bp
.next:
    cmp bp, E820_MAX
    jae .done
    ; Compute the entry buffer address first. Do not overwrite EAX=E820
    ; before INT 15h; BIOS uses EAX to select the E820 service.
    mov di, E820_ADDR
    ; BP is the 16-bit loop index; zero-extend it explicitly because
    ; the upper half of EBP is not guaranteed to be zero in real mode.
    movzx eax, bp
    imul eax, 24
    add di, ax
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
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
    mov ecx, 64              ; first 1 KiB of the EBDA, in 16-byte steps
    call scan_rsdp
    cmp dword [rsdp_found], 0
    jne .done

    mov dword [rsdp_scan], 0x000E0000
    mov ecx, 0x2000           ; 128 KiB BIOS area, in 16-byte steps
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
    ; ACPI RSDP signature is the eight-byte ASCII string "RSD PTR ".
    ; Little-endian first dword is 0x20445352, not 0x20524453.
    cmp dword [esi], 0x20445352
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
    ; VBE BIOS calls expect a conventional real-mode ES:DI buffer.
    xor ax, ax
    mov es, ax

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
    mov al, [VBE_MODE_BUF+26]
    mov [FBINFO_ADDR+45], al
    mov al, [VBE_MODE_BUF+27]
    mov [FBINFO_ADDR+46], al
    mov al, [VBE_MODE_BUF+28]
    mov [FBINFO_ADDR+47], al

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
    cld
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

    ; Give the framebuffer its own higher-half window at
    ; ffffffff:c0000000. This is separate from the kernel's first-1GiB alias.
    mov eax, [FBINFO_ADDR+8]
    test eax, eax
    jz .done
    mov ebx, eax
    and ebx, 0xFFE00000
    mov eax, [FBINFO_ADDR+8]
    sub eax, ebx
    mov [fb_offset], eax

    mov eax, [FBINFO_ADDR+32]
    mul dword [FBINFO_ADDR+28]
    mov dword [FBINFO_ADDR+16], eax
    mov dword [FBINFO_ADDR+20], edx

    mov eax, FB_VA_LO
    add eax, [fb_offset]
    mov [FBINFO_ADDR], eax
    mov dword [FBINFO_ADDR+4], FB_VA_HI
.done:
    ret

; ---------------------------------------------------------------------------
; Page tables / long mode
; ---------------------------------------------------------------------------

build_page_tables:
    cld
    ; Clear 3 pages using 32-bit destination addressing.
    mov edi, PAGE_PML4
    xor eax, eax
    mov ecx, 12288/4 ; three full 4 KiB paging structures
    a32 rep stosd

    ; PML4[0] -> PDPT, PML4[511] -> PDPT.
    a32 mov dword [PAGE_PML4+0], PAGE_PDPT | 3
    a32 mov dword [PAGE_PML4+4], 0
    a32 mov dword [PAGE_PML4+511*8], PAGE_PDPT | 3
    a32 mov dword [PAGE_PML4+511*8+4], 0

    ; PDPT[0] and [510] and [511] -> PD.
    a32 mov dword [PAGE_PDPT+0], PAGE_PD | 3
    a32 mov dword [PAGE_PDPT+510*8], PAGE_PD | 3
    a32 mov dword [PAGE_PDPT+511*8], PAGE_FBPD | 3

    ; 2 MiB identity map for the first 1 GiB. The kernel higher-half
    ; window ffffffff80000000 uses PDPT[510].
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

    ; Separate 2 MiB mappings for the VBE framebuffer window.
    mov edi, PAGE_FBPD
    xor eax, eax
    mov ecx, 4096/4 ; clear all 512 64-bit framebuffer PDEs
    a32 rep stosd
    mov eax, [FBINFO_ADDR+8]
    test eax, eax
    jz .fb_done
    and eax, 0xFFE00000
    mov [fb_phys_base], eax
    mov eax, [FBINFO_ADDR+16]
    mov edx, [fb_offset]
    add eax, edx
    add eax, 0x001FFFFF
    shr eax, 21
    mov [fb_pages], eax
    xor ecx, ecx
.fb_map:
    cmp ecx, [fb_pages]
    jae .fb_done
    mov eax, ecx
    shl eax, 21
    add eax, [fb_phys_base]
    or eax, 0x83
    mov edi, PAGE_FBPD
    mov edx, ecx
    shl edx, 3
    add edi, edx
    mov [edi], eax
    mov dword [edi+4], 0
    inc ecx
    cmp ecx, 512
    jb .fb_map
.fb_done:
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
    ; Long mode requires both protected mode (PE) and paging (PG).
    or eax, 0x80000001
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

    ; Keep the first C stack clear of the ELF staging buffer and page tables.
    mov eax, [abs kernel_phys_end]
    add eax, 0x00100000
    and eax, 0xFFFFFFF0
    mov rsp, rax
    sub rsp, 8

    mov rdi, BOOTINFO_ADDR
    mov rax, [abs BOOTINFO_ADDR+72]
    jmp rax

fatal_disk:
    mov si, serial_fatal_disk
    call serial_puts
    mov si, msg_disk
    jmp fatal
fatal_kernel:
    mov si, serial_fatal_kernel
    call serial_puts
    mov si, msg_kernel
    jmp fatal
fatal_memory:
    mov si, serial_fatal_memory
    call serial_puts
    mov si, msg_memory
    jmp fatal

fatal:
    call puts
    cli
.hang:
    hlt
    jmp .hang

BITS 16
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
last_key db 0
partition_start dd 0
kernel_size dd 0
loaded_bytes dd 0
chunk_sectors dd 0
chunk_bytes dd 0
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
fb_offset dd 0
fb_phys_base dd 0
fb_pages dd 0

title db 13,10,"NULLLOADER - NATIVE BOOT CONTROL",13,10,0
normal db "  [1] Normal-Mode",13,10,0
assembly db "  [2] Assembly-Mode",13,10,0
npb db "  [3] NullOS Pre-Boot",13,10,0
old_boot db "  [0] Old Boot Record",13,10,0
prompt db "Command: ",0
serial_entered db "NullL: stage2 entered",13,10,0
serial_auto_mode db "NullL: mailbox mode active; bypassing menu",13,10,0
serial_menu db "NullL: menu displayed; waiting for key",13,10,0
serial_key db "NullL: key=",0
serial_select_1 db "NullL: selected mode 1",13,10,0
serial_select_2 db "NullL: selected mode 2",13,10,0
serial_select_3 db "NullL: selected mode 3",13,10,0
serial_loading_kernel db "NullL: loading kernel manifest/extents",13,10,0
serial_manifest_read db "NullL: reading manifest sector",13,10,0
serial_manifest_read_ok db "NullL: manifest sector read",13,10,0
serial_manifest_valid db "NullL: manifest validated",13,10,0
serial_chunks_done db "NullL: all kernel chunks loaded",13,10,0
serial_unreal_ready db "NullL: unreal mode enabled",13,10,0
serial_elf_header_ok db "NullL: ELF64 header accepted",13,10,0
serial_elf_segments_done db "NullL: ELF PT_LOAD segments copied",13,10,0
serial_chunk_read db "NullL: reading kernel extent chunk",13,10,0
serial_chunk_read_ok db "NullL: kernel extent chunk read",13,10,0
serial_kernel_loaded db "NullL: ELF segments loaded",13,10,0
serial_e820_ok db "NullL: E820 memory map collected",13,10,0
serial_rsdp_done db "NullL: RSDP scan complete",13,10,0
serial_vbe_done db "NullL: VBE setup complete",13,10,0
serial_long_mode db "NullL: entering long mode",13,10,0
serial_fatal_disk db "NullL: fatal disk error",13,10,0
serial_fatal_kernel db "NullL: fatal kernel load/ELF error",13,10,0
serial_fatal_memory db "NullL: fatal E820 memory map error",13,10,0
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
    db 0x10, 0
.count dw 0
.offset dw 0
.segment dw 0
.lba_low dd 0
.lba_high dd 0
.flat_low dd 0
.flat_high dd 0

align 4, db 0
kernel_dap:
    db 0x10, 0
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
    dq 0x00CF92000000FFFF
gdt64_descriptor:
    dw 23
    dd gdt64

align 8
gdt_unreal:
    dq 0
    dq 0x008F9A000000FFFF ; 16-bit code with 4 GiB limit
    dq 0x00CF92000000FFFF ; 32-bit flat data
gdt_unreal_end:
gdt_unreal_descriptor:
    dw gdt_unreal_end - gdt_unreal - 1
    dd gdt_unreal

times STAGE2_SECTORS*512-($-$$) db 0
