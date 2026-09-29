# NullL

NullL is the native bootloader for NullOS.

## Contract

Firmware -> NullL -> ELF64 -> NullBootInfo -> NullOS.

NullL does not chainload Limine and does not use the Limine boot protocol.

The BIOS and UEFI loaders must produce the same NullBootInfo ABI. The kernel receives a pointer to NullBootInfo in RDI.

## Memory

The kernel remains a higher-half ELF:

- VMA: 0xffffffff80000000
- physical/LMA base: 0x00100000

The loader maps the kernel physical load addresses into the higher-half virtual addresses before entering the ELF entry point.

## Firmware paths

- bios/: legacy BIOS stage1/stage2 path.
- uefi/: native x86-64 UEFI PE/COFF loader.
- elf/: common ELF64 validation/loading rules.
- bootinfo/: common handoff ABI.

## Non-goals

NullL does not become a filesystem driver for the kernel. It only implements enough storage access to locate and load the configured kernel image.