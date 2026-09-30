CC      := gcc
LD      := ld
NASM    := nasm
QEMU    ?= qemu-system-x86_64
GENVAULT := ncc/genvault.sh

BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/iso
KERNEL    := $(BUILD_DIR)/kernel.elf
ISO       := $(BUILD_DIR)/NullOS.iso
DISK      ?= nullos.img
VAULT_C   := ncc/vault_gen.c
VAULT_H   := ncc/vault_gen.h
NULLL_STAGE1 := $(BUILD_DIR)/nulll/stage1.bin
NULLL_STAGE2 := $(BUILD_DIR)/nulll/stage2.bin

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
	arch/x86_64/cpu/gdt.c \
	arch/x86_64/acpi/acpi.c \
	arch/x86_64/pci/pci.c \
	arch/x86_64/interrupts/idt.c \
	arch/x86_64/interrupts/isr.c \
	arch/x86_64/interrupts/irq.c \
	arch/x86_64/interrupts/pic.c \
	drivers/input/keyboard/keyboard.c \
	drivers/input/keyboard/keyboard_layout.c \
	drivers/storage/ide/ide.c \
	drivers/storage/nvme/nvme.c \
	drivers/storage/ahci/ahci.c \
	drivers/storage/block/block.c \
	drivers/video_font/font.c \
	drivers/video_framebuffer/framebuffer.c \
	drivers/network/e1000/e1000.c \
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
	kernel/LTSMT/ltsmt.c \
	kernel/drivers_init.c \
	kernel/panic/panic.c \
	kernel/terminal/terminal.c \
	kernel/terminal/cursor.c \
	kernel/kernel.c \
	kernel/ring/ring3.c \
	kernel/task/task.c \
	kernel/process/process.c \
	kernel/syscall/syscall.c \
	kernel/sched/scheduler.c \
	kernel/registry/registry.c \
	kernel/elf/elf_loader.c \
	lib/delay/delay.c \
	lib/string/string.c \
	libc/ctype.c \
	libc/stdio.c \
	libc/stdlib.c \
	libc/string.c \
	mm/pmm.c \
	mm/vmm.c \
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
	shell/commands/lowlevel.c \
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
	Graphic/graphic.c \
	bootmonitor/bootmonitor.c

ASM_OBJECTS := \
	$(BUILD_DIR)/arch/x86_64/interrupts/isr_asm.o \
	$(BUILD_DIR)/arch/x86_64/interrupts/irq_asm.o \
	$(BUILD_DIR)/arch/x86_64/program/run_asm.o \
	$(BUILD_DIR)/arch/x86_64/bootasm.o \
	$(BUILD_DIR)/arch/x86_64/cpu/ring3.o \
	$(BUILD_DIR)/arch/x86_64/cpu/context.o \
	$(BUILD_DIR)/kernel/LTSMT/ltsmt.o \
	$(BUILD_DIR)/drivers/timer/RTC.o

OBJECTS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o) $(ASM_OBJECTS)

-include $(C_SOURCES:%.c=$(BUILD_DIR)/%.d)

.PHONY: all clean vault run boot install nulll

.NOTPARALLEL:

all: vault nulll

nulll: $(NULLL_STAGE1) $(NULLL_STAGE2)

$(KERNEL): $(OBJECTS) | arch/x86_64/linker.ld
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $^

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

$(BUILD_DIR)/arch/x86_64/bootasm.o: arch/x86_64/bootasm.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/arch/x86_64/cpu/ring3.o: arch/x86_64/cpu/ring3.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/arch/x86_64/cpu/context.o: arch/x86_64/cpu/context.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/kernel/LTSMT/ltsmt.o: kernel/LTSMT/ltsmt.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/drivers/timer/RTC.o: drivers/timer/RTC.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(NULLL_STAGE1): nulll/stage1.asm
	@mkdir -p $(@D)
	$(NASM) -f bin $< -o $@

$(NULLL_STAGE2): nulll/stage2.asm
	@mkdir -p $(@D)
	$(NASM) -f bin $< -o $@

install: $(KERNEL) $(NULLL_STAGE1) $(NULLL_STAGE2)
	./run-install.sh $(DISK)

boot: install
	@if [ -f /tmp/nullos-qemu.pid ]; then kill -9 "$$(cat /tmp/nullos-qemu.pid)" 2>/dev/null || true; rm -f /tmp/nullos-qemu.pid; fi
	qemu-system-x86_64 -m 3 -boot order=c -drive "file=$(DISK),format=raw,if=ide,index=0" -netdev user,id=n1 -device e1000,netdev=n1 -serial stdio -display gtk

run: install
	qemu-system-x86_64 -m 3 -boot order=c -drive "file=$(DISK),format=raw,if=ide,index=0" -netdev user,id=n1 -device e1000,netdev=n1 -serial stdio -display gtk

clean:
	rm -rf $(BUILD_DIR) .vault-stamp
