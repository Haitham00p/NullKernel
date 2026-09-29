# NullL — Native NullOS Bootloader

NullL is the native BIOS boot path for NullOS. It is a standalone boot path; the kernel consumes only the NullBoot ABI.

## Boot contract

```
BIOS
  -> MBR stage1
  -> FAT32 reserved-sector stage2
  -> NLLK kernel extent manifest
  -> ELF64 PT_LOAD loader
  -> E820 memory map + BIOS RSDP
  -> VBE linear framebuffer (optional)
  -> NullBootInfo
  -> NullOS higher-half kernel
```

## Disk layout

Relative to the active FAT32 partition:

- sector 2: NLM1 one-shot boot-mode mailbox
- sector 3: NLLK kernel extent manifest
- sector 7: NullL stage2
- sector 1: FAT32 FSInfo
- sector 6: FAT32 backup VBR
- partition-1: saved Old Boot Record

The MBR contains only the NullL stage1 code plus the existing partition table.

## Kernel ABI

`include/nullboot.h` defines the firmware-to-kernel ABI. NullL passes its address in RDI when entering the ELF64 kernel entry point.

The ABI carries:

- E820 memory map
- framebuffer physical/virtual information
- BIOS boot drive
- kernel physical range
- kernel entry point
- loader range

The kernel uses this structure for PMM, ACPI, framebuffer initialization and boot diagnostics.

## Current scope

The native BIOS path is implemented on `dev/null-bootloader`.

The branch intentionally keeps firmware-specific UEFI work separate from the BIOS loader so the BIOS path has no dependency on a third-party bootloader.