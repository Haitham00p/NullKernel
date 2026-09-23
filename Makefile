CC      := gcc
LD      := ld
NASM    := nasm
LIMINE  ?= limine-binary
QEMU    ?= qemu-system-x86_64
GENVAULT := ncc/genvault.sh

BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/iso
KERNEL    := $(BUILD_DIR)/kernel.elf
ISO       := $(BUILD_DIR)/NullOS.iso
DISK      ?= nullos.img
VAULT_C   := ncc/vault_gen.c
VAULT_H   := ncc/vault_gen.h

CFLAGS := \
	-std=c11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-pie \
	-m64 \
	-mcmodel=kernel \
	-mno-red-zone \
	-MMD -MP \
	-Wall \
	-Wextra \
	-I. \
	-Iinclude

LDFLAGS := \
	-T arch/x86_64/linker.ld \
	-nostdlib \
	-static \
	-z max-page-size=0x1000

C_SOURCES := \
	arch/x86_64/cpu/io.c \
	arch/x86_64/interrupts/idt.c \
	arch/x86_64/interrupts/isr.c \
	arch/x86_64/interrupts/irq.c \
	arch/x86_64/interrupts/pic.c \
	drivers/input/keyboard/keyboard.c \
	drivers/input/keyboard/keyboard_layout.c \
	drivers/storage/ide/ide.c \
	drivers/video_font/font.c \
	drivers/video_framebuffer/framebuffer.c \
 	fs/fat32/directory/directory.c \
	fs/fat32/file/file.c \
	fs/fat32/fat/fat.c \
	fs/fat32/vbr/vbr.c \
	fs/fat32/fat_cluster.c \
	fs/fat32/fat_directory.c \
 	fs/fat32/fat_file.c \
 	fs/fat32/fat_path.c \
 	fs/fat32/fat_dir.c \
	fs/partition/mbr/mbr.c \
	fs/ramfs/ramfs.c \
	kernel/debug/debug.c \
	kernel/drivers_init.c \
	kernel/panic/panic.c \
	kernel/terminal/terminal.c \
	kernel/terminal/cursor.c \
	kernel/kernel.c \
	kernel/elf/elf_loader.c \
	installer/installer.c \
	lib/delay/delay.c \
	lib/string/string.c \
	libc/ctype.c \
	libc/stdio.c \
	libc/stdlib.c \
	libc/string.c \
	mm/heap/heap.c \
	ncc/ncc.c \
	ncc/vault_gen.c \
	n64a/n64a.c \
	n64a/n64a_lex.c \
	n64a/n64a_reg.c \
	n64a/n64a_enc.c \
	n64a/n64a_asm.c \
	n64a/n64a_parse.c \
	n64a/n64a_obj.c \
	nld/link_core.c \
	shell/commands/cmd.c \
	shell/dispatcher.c \
	shell/diskfs.c \
	shell/editor/editor.c \
	shell/parser.c \
	shell/highlight.c \
	shell/selfhost.c \
	shell/shell.c \
	arch/x86_64/power/pwr.c \
	fs/ISO9660/iso9660.c \
	drivers/timer/PIT.c \
	kernel/time/time.c \
	source/kchar.c \
	source/kstring.c \
	source/kterminal.c \
	source/kshell.c \
	lib/sound/sndbeep.c \
	Graphic/graphic.c

ASM_OBJECTS := \
	$(BUILD_DIR)/arch/x86_64/interrupts/isr_asm.o \
	$(BUILD_DIR)/arch/x86_64/interrupts/irq_asm.o \
	$(BUILD_DIR)/arch/x86_64/program/run_asm.o

OBJECTS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o) $(ASM_OBJECTS)

-include $(C_SOURCES:%.c=$(BUILD_DIR)/%.d)

.PHONY: all clean vault run play install boot

.NOTPARALLEL:

all: vault $(ISO)

$(KERNEL): $(OBJECTS)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $^

# Regenerates the self-hosting vault (embedded headers + kernel symbol
# addresses) from the freshly linked kernel, then relinks with the new
# vault. Two passes are needed: the first pass computes addresses from a
# kernel carrying the previous vault, so data-symbol addresses can be
# stale; the second pass regenerates from the relinked kernel and
# converges (only the vault section size changes between passes).
# Guarded by a stamp so repeated `make` runs stay quiet.
vault: $(KERNEL)
	@if [ ! -f .vault-stamp ] || [ "$(KERNEL)" -nt .vault-stamp ] || \
	 [ -n "$$(find shell ncc n64a nld libc kernel lib drivers fs arch source include -name '*.c' -newer .vault-stamp -print -quit 2>/dev/null)" ]; then \
		bash $(GENVAULT) $(KERNEL) $(VAULT_C) $(VAULT_H) && \
		$(MAKE) --no-print-directory $(KERNEL) && \
		bash $(GENVAULT) $(KERNEL) $(VAULT_C) $(VAULT_H) && \
		$(MAKE) --no-print-directory $(KERNEL) && \
		touch .vault-stamp; \
	fi

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/x86_64/interrupts/isr_asm.o: arch/x86_64/interrupts/isr.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/arch/x86_64/interrupts/irq_asm.o: arch/x86_64/interrupts/irq.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/arch/x86_64/program/run_asm.o: arch/x86_64/program/run.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

ROOTFS_FILES := $(shell find rootfs -type f 2>/dev/null)

$(ISO): $(KERNEL) $(ROOTFS_FILES) iso/boot/limine/limine.conf iso/EFI/BOOT/BOOTX64.EFI
	@mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	rm -rf $(ISO_DIR)/rootfs
	cp -r rootfs $(ISO_DIR)/rootfs
	cp $(LIMINE)/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	cp $(LIMINE)/limine-bios.sys $(ISO_DIR)/boot/limine/
	cp $(LIMINE)/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	cp iso/boot/limine/limine.conf $(ISO_DIR)/boot/limine/
	cp iso/EFI/BOOT/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot \
		-boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin -no-emul-boot \
		-o $@ $(ISO_DIR)
	$(LIMINE)/limine bios-install $@

# Reinstall the freshly built ISO onto the working disk image so the OS
# always reflects the current source tree (formats the disk from $(ISO),
# copying the full rootfs with correct uppercase names). Run sync_rootfs
# first if you have OS-side edits you want to keep.
install: $(ISO)
	./run-install.sh $(DISK) --auto

# Boot the installed disk in a visible QEMU window so you can see the OS.
# Runs in the foreground so `make` blocks on it and any error is visible.
# The previous QEMU PID is recorded in a pidfile and killed by exact PID
# only (no pattern matching, which would match this recipe's own shell and
# kill `make` itself).
boot: $(DISK)
	@if [ -f /tmp/nullos-qemu.pid ]; then \
		kill -9 "$$(cat /tmp/nullos-qemu.pid)" 2>/dev/null || true; \
		rm -f /tmp/nullos-qemu.pid; \
	fi
	@sleep 1
	qemu-system-x86_64 -m 256 -boot order=c \
		-drive "file=$(DISK),format=raw,if=ide,index=0" \
		-serial file:/tmp/nullos-serial.log -display gtk

run: all
	./start.sh

play: $(ISO)
	./start.sh

clean:
	rm -rf $(BUILD_DIR) .vault-stamp