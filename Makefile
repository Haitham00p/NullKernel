CC      := gcc
LD      := ld
NASM    := nasm
LIMINE  ?= limine-binary

BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/iso
KERNEL    := $(BUILD_DIR)/kernel.elf
ISO       := $(BUILD_DIR)/NullOS.iso

CFLAGS := \
	-std=c11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-pie \
	-m64 \
	-mcmodel=kernel \
	-mno-red-zone \
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
	fs/fat32/fat/fat.c \
	fs/fat32/vbr/vbr.c \
	fs/fat32/fat_cluster.c \
	fs/fat32/fat_directory.c \
 	fs/fat32/fat_file.c \
 	fs/fat32/fat_path.c \
 	fs/fat32/fat_dir.c \
	fs/partition/mbr/mbr.c \
	fs/ramfs/ramfs.c \
	fs/vfs/vfs.c \
	kernel/debug/debug.c \
	kernel/panic/panic.c \
	kernel/terminal/terminal.c \
	kernel/kernel.c \
	lib/delay/delay.c \
	lib/string/string.c \
	mm/heap/heap.c \
	shell/commands/cmd.c \
	shell/editor/editor.c \
	shell/dispatcher.c \
	shell/parser.c \
	shell/shell.c \
	arch/x86_64/power/pwr.c \
	fs/ISO9660/iso9660.c \
	drivers/timer/PIT.c \
	kernel/time/time.c \
	source/kchar.c \
	source/kstring.c \
	source/kterminal.c \
	source/kshell.c


ASM_OBJECTS := \
	$(BUILD_DIR)/arch/x86_64/interrupts/isr_asm.o \
	$(BUILD_DIR)/arch/x86_64/interrupts/irq_asm.o

OBJECTS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o) $(ASM_OBJECTS)

.PHONY: all clean

all: $(ISO)

$(KERNEL): $(OBJECTS)
	@mkdir -p $(@D)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/x86_64/interrupts/isr_asm.o: arch/x86_64/interrupts/isr.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/arch/x86_64/interrupts/irq_asm.o: arch/x86_64/interrupts/irq.asm
	@mkdir -p $(@D)
	$(NASM) -f elf64 $< -o $@

$(ISO): $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
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

clean:
	rm -rf $(BUILD_DIR)
