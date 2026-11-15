/*
 * NullOS Limine handoff adapter.
 *
 * The current kernel consumes the stable NullBootInfo ABI. This file translates
 * Limine's firmware-neutral boot responses into that ABI before kernel_main.
 * Limine base revision 0 is intentionally requested for this first migration
 * because the existing PMM/VMM and early fault recorder require low identity
 * mappings. Moving those subsystems to HHDM-only access is a separate ABI-safe
 * follow-up; do not raise this revision without that work.
 */
#include <stdint.h>
#include <stddef.h>
#include "include/nullboot.h"
#include "include/limine.h"

#define NULL_LIMINE_MAX_MEMMAP 512U
#define NULL_LIMINE_STACK_SIZE 0x10000U

_Static_assert(sizeof(NullBootInfo) == 112U, "NullBootInfo ABI size changed");

extern char __kernel_virtual_end;
extern void kernel_entry(void);

static NullBootInfo g_boot_info;
static NullBootMemoryEntry g_memory_map[NULL_LIMINE_MAX_MEMMAP];
static NullBootFramebuffer g_framebuffer;

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t g_limine_requests_start[4] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t g_limine_base_revision[3] =
    LIMINE_BASE_REVISION(0);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request g_stack_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .stack_size = NULL_LIMINE_STACK_SIZE
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request g_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request g_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request g_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request g_rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request g_executable_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t g_limine_requests_end[2] =
    LIMINE_REQUESTS_END_MARKER;

static uint32_t translate_memory_type(uint64_t type)
{
    switch (type) {
    case LIMINE_MEMMAP_USABLE:
        return NULLBOOT_MEM_USABLE;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return NULLBOOT_MEM_ACPI_RECLAIM;
    case LIMINE_MEMMAP_ACPI_NVS:
        return NULLBOOT_MEM_ACPI_NVS;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return NULLBOOT_MEM_BAD;
    case LIMINE_MEMMAP_RESERVED:
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
    case LIMINE_MEMMAP_FRAMEBUFFER:
    case LIMINE_MEMMAP_RESERVED_MAPPED:
    default:
        /* Keep bootloader data and every non-usable region out of the PMM. */
        return NULLBOOT_MEM_RESERVED;
    }
}

static int build_memory_map(void)
{
    struct limine_memmap_response *response = g_memmap_request.response;

    if (response == NULL || response->entries == NULL ||
        response->entry_count == 0 ||
        response->entry_count > NULL_LIMINE_MAX_MEMMAP)
        return 0;

    for (uint64_t i = 0; i < response->entry_count; ++i) {
        const struct limine_memmap_entry *entry = response->entries[i];
        if (entry == NULL)
            return 0;

        g_memory_map[i].Base = entry->base;
        g_memory_map[i].Length = entry->length;
        g_memory_map[i].Type = translate_memory_type(entry->type);
        g_memory_map[i].Flags = 0;
    }

    g_boot_info.MemoryMap = (uint64_t)(uintptr_t)g_memory_map;
    g_boot_info.MemoryMapCount = (uint32_t)response->entry_count;
    return 1;
}

static void build_framebuffer(void)
{
    struct limine_framebuffer_response *response =
        g_framebuffer_request.response;

    g_boot_info.Framebuffer = 0;
    if (response == NULL || response->framebuffer_count == 0 ||
        response->framebuffers == NULL || g_hhdm_request.response == NULL)
        return;

    struct limine_framebuffer *fb = response->framebuffers[0];
    if (fb == NULL || fb->address == NULL || fb->width == 0 ||
        fb->height == 0 || fb->pitch == 0 || fb->bpp != 32 ||
        fb->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        fb->width > UINT32_MAX || fb->height > UINT32_MAX ||
        fb->pitch > UINT32_MAX || fb->pitch > UINT64_MAX / fb->height)
        return;

    uint64_t address = (uint64_t)(uintptr_t)fb->address;
    uint64_t hhdm = g_hhdm_request.response->offset;
    if (address < hhdm)
        return;

    g_framebuffer.Address = address;
    g_framebuffer.PhysicalAddress = address - hhdm;
    g_framebuffer.Size = fb->pitch * fb->height;
    g_framebuffer.Width = (uint32_t)fb->width;
    g_framebuffer.Height = (uint32_t)fb->height;
    g_framebuffer.Pitch = (uint32_t)fb->pitch;
    g_framebuffer.Bpp = fb->bpp;
    g_framebuffer.MemoryModel = fb->memory_model;
    g_framebuffer.RedMask = fb->red_mask_size;
    g_framebuffer.RedPosition = fb->red_mask_shift;
    g_framebuffer.GreenMask = fb->green_mask_size;
    g_framebuffer.GreenPosition = fb->green_mask_shift;
    g_framebuffer.BlueMask = fb->blue_mask_size;
    g_framebuffer.BluePosition = fb->blue_mask_shift;
    g_framebuffer.ReservedMask = 0;
    g_framebuffer.ReservedPosition = 0;

    g_boot_info.Framebuffer = (uint64_t)(uintptr_t)&g_framebuffer;
}

NullBootInfo *LimineBootPrepare(void)
{
    struct limine_executable_address_response *executable =
        g_executable_request.response;
    struct limine_rsdp_response *rsdp = g_rsdp_request.response;

    if (!LIMINE_BASE_REVISION_SUPPORTED(g_limine_base_revision) ||
        !LIMINE_LOADED_BASE_REVISION_VALID(g_limine_base_revision) ||
        LIMINE_LOADED_BASE_REVISION(g_limine_base_revision) != 0 ||
        executable == NULL || g_hhdm_request.response == NULL ||
        g_memmap_request.response == NULL)
        return NULL;

    if (executable->virtual_base == 0 ||
        (uint64_t)(uintptr_t)&__kernel_virtual_end < executable->virtual_base)
        return NULL;

    g_boot_info.Magic = NULLBOOT_MAGIC;
    g_boot_info.Version = NULLBOOT_VERSION;
    g_boot_info.Size = sizeof(g_boot_info);
    g_boot_info.MemoryMap = 0;
    g_boot_info.MemoryMapCount = 0;
    g_boot_info.Framebuffer = 0;
    g_boot_info.RSDP = (rsdp != NULL && rsdp->address != NULL)
        ? (uint64_t)(uintptr_t)rsdp->address : 0;
    g_boot_info.BootDrive = 0; /* Firmware-neutral: BIOS drive numbers do not exist on UEFI. */
    g_boot_info.KernelPhysicalBase = executable->physical_base;
    uint64_t kernel_span =
        (uint64_t)(uintptr_t)&__kernel_virtual_end - executable->virtual_base;
    if (executable->physical_base > UINT64_MAX - kernel_span)
        return NULL;
    g_boot_info.KernelPhysicalEnd = executable->physical_base + kernel_span;
    g_boot_info.KernelEntry = (uint64_t)(uintptr_t)kernel_entry;
    g_boot_info.CommandLine = 0;
    /* Bootloader-reclaimable map entries are translated to RESERVED above. */
    g_boot_info.LoaderBase = 0;
    g_boot_info.LoaderEnd = 0;
    g_boot_info.HHDMOffset = g_hhdm_request.response->offset;

    if (g_boot_info.KernelPhysicalEnd <= g_boot_info.KernelPhysicalBase ||
        !build_memory_map())
        return NULL;

    build_framebuffer();
    return &g_boot_info;
}
