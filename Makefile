CC = gcc
LD = ld
NASM = nasm


CFLAGS = \
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
	-IlimineH \
	-Ifont \
	-Iframebuffer \
	-Iterminal \
	-Iinterrupts \
	-Idrivers \
	-Icpu \
	-Idebug \
	-Istring


LDFLAGS = \
	-T linker.ld \
	-nostdlib \
	-static \
	-z max-page-size=0x1000



CFILES = \
	src/kernel.c \
	framebuffer/framebuffer.c \
	terminal/terminal.c \
	font/font.c \
	interrupts/idt.c \
	interrupts/isr.c \
	interrupts/irq.c \
	interrupts/pic.c \
	cpu/io.c \
	drivers/keyboard.c \
	drivers/keyboard_layout.c \
	debug/debug.c \
	shell/shell.c \
	shell/bin/cmd.c \
	string/string.c \
	shell/dispatcher.c \
	shell/parser.c \
	panic/panic.c \
	heap/heap.c \
	ramfs/ramfs.c
	



OBJS = \
	src/kernel.o \
	framebuffer/framebuffer.o \
	terminal/terminal.o \
	font/font.o \
	interrupts/idt.o \
	interrupts/isr.o \
	interrupts/irq.o \
	interrupts/pic.o \
	cpu/io.o \
	drivers/keyboard.o \
	drivers/keyboard_layout.o \
	interrupts/isr_asm.o \
	interrupts/irq_asm.o \
	debug/debug.o \
	shell/shell.o \
	shell/bin/cmd.o \
	string/string.o \
	shell/dispatcher.o \
	shell/parser.o \
	panic/panic.o \
	memory/heap.o \
	ramfs/ramfs.o 


LIMINE = /home/null/limine-binary



all: NullOS.iso



kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.elf $(OBJS)



%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@



interrupts/isr_asm.o: interrupts/isr.asm
	$(NASM) -f elf64 interrupts/isr.asm -o interrupts/isr_asm.o



interrupts/irq_asm.o: interrupts/irq.asm
	$(NASM) -f elf64 interrupts/irq.asm -o interrupts/irq_asm.o




NullOS.iso: kernel.elf

	mkdir -p iso/boot/limine

	cp kernel.elf iso/boot/kernel.elf

	cp $(LIMINE)/limine-bios-cd.bin iso/boot/limine/
	cp $(LIMINE)/limine-bios.sys iso/boot/limine/
	cp $(LIMINE)/limine-uefi-cd.bin iso/boot/limine/


	xorriso -as mkisofs \
		-R \
		-r \
		-J \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-no-emul-boot \
		-o NullOS.iso \
		iso


	$(LIMINE)/limine bios-install NullOS.iso





clean:
	rm -f $(OBJS)
	rm -f kernel.elf
	rm -f NullOS.iso


.PHONY: all clean
