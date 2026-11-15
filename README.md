# NullOS

NullOS is an x86-64 hobby operating system with a freestanding kernel, FAT32
root filesystem, and a Limine-based firmware boot path.

## Bootloader

NullOS uses [Limine](https://github.com/limine-bootloader/limine) v12.9.6.
The installer prepares one FAT32 EFI System Partition (MBR type 0xEF) and
installs both:

- BIOS/MBR boot code plus `limine-bios.sys`.
- UEFI x86-64 fallback application at `/EFI/BOOT/BOOTX64.EFI`.

The configuration in `boot/limine.conf` selects the Normal kernel entry and
sets `timeout: 0`, so Limine boots it immediately without displaying a menu.

The kernel consumes `NullBootInfo` ABI version 2. The adapter in
`boot/limine.c` translates Limine's memory map, HHDM framebuffer, RSDP, and
executable-address responses and HHDM offset into that structure.

### Paging compatibility note

The first migration intentionally requests Limine base revision 0. The current
PMM page clearing, the physical heap window, and the early exception recorder
still use low identity-mapped addresses. VMM page-table access uses HHDM. Limine base revision 0 preserves that
mapping while the boot protocol integration is brought online. Base revision 0
is deprecated by the current protocol; do not change the requested revision
until those kernel subsystems have been migrated to HHDM-only physical access
and tested.

## Build and install

On a Debian/Ubuntu build host, install the usual x86-64 freestanding toolchain
and disk utilities:

```sh
sudo apt-get update
sudo apt-get install gcc binutils nasm make curl ca-certificates \
    qemu-system-x86 dosfstools mtools fdisk
```

Build the kernel:

```sh
make
```

Create a hybrid BIOS/UEFI disk image (the installer formats the target image):

```sh
make install DISK=nullos.img
```

Run the image in QEMU using BIOS:

```sh
make run DISK=nullos.img
```

The Limine binary release is downloaded by `tools/fetch-limine.sh`, pinned to
v12.9.6 and checked against its published SHA-256 before use.

## Boot handoff

`arch/x86_64/bootasm.asm` installs the early exception IDT, calls
`LimineBootPrepare`, then passes the generated `NullBootInfo *` in RDI to
`kernel_main`. The Limine request structures and markers are retained by
`arch/x86_64/linker.ld`.
