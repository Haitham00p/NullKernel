#ifndef NULLOS_NULLBOOT_H
#define NULLOS_NULLBOOT_H

#include <stdint.h>

#define NULLBOOT_MAGIC 0x4E554C4C55424F54ULL
#define NULLBOOT_VERSION 1U
#define NULLBOOT_MEM_USABLE 1U
#define NULLBOOT_MEM_RESERVED 2U
#define NULLBOOT_MEM_ACPI_RECLAIM 3U
#define NULLBOOT_MEM_ACPI_NVS 4U
#define NULLBOOT_MEM_BAD 5U
#define NULLBOOT_FB_VA 0xFFFFFFFFC0000000ULL

typedef struct __attribute__((packed)) { uint64_t Base; uint64_t Length; uint32_t Type; uint32_t Flags; } NullBootMemoryEntry;
typedef struct __attribute__((packed)) { uint64_t Address; uint64_t PhysicalAddress; uint64_t Size; uint32_t Width; uint32_t Height; uint32_t Pitch; uint16_t Bpp; uint16_t MemoryModel; uint8_t RedMask; uint8_t RedPosition; uint8_t GreenMask; uint8_t GreenPosition; uint8_t BlueMask; uint8_t BluePosition; uint8_t ReservedMask; uint8_t ReservedPosition; } NullBootFramebuffer;
typedef struct __attribute__((packed)) { uint64_t Magic; uint32_t Version; uint32_t Size; uint64_t MemoryMap; uint32_t MemoryMapCount; uint32_t Reserved0; uint64_t Framebuffer; uint64_t RSDP; uint64_t BootDrive; uint64_t KernelPhysicalBase; uint64_t KernelPhysicalEnd; uint64_t KernelEntry; uint64_t CommandLine; uint64_t LoaderBase; uint64_t LoaderEnd; } NullBootInfo;
extern volatile NullBootInfo *NullBoot;

#endif