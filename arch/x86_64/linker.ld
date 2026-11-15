ENTRY(kernel_entry)

/* Keep the kernel at 16 MiB so the legacy NLD executable slot at physical
   1-16 MiB cannot overwrite the higher-half kernel image. */
KERNEL_VMA = 0xffffffff81000000;
KERNEL_LMA = 0x01000000;

SECTIONS
{
    . = KERNEL_VMA;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA + KERNEL_LMA)
    {
        *(.text*)
    }

    /* Auto-registered driver/component initializers (see include/drivers.h).
       Kept contiguous so the kernel can iterate __init_start..__init_end. */
    .init_array ALIGN(4K) : AT(ADDR(.init_array) - KERNEL_VMA + KERNEL_LMA)
    {
        PROVIDE_HIDDEN (__init_start = .);
        KEEP(*(.drivers.init))
        PROVIDE_HIDDEN (__init_end = .);
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA + KERNEL_LMA)
    {
        *(.rodata*)
        *(.eh_frame_hdr*)
        *(.eh_frame*)
        *(.gcc_except_table*)
    }

    /* Keep Limine's mutable request/response structures in their own
       writable ELF section, and retain the marker-delimited request range. */
    .limine_requests ALIGN(8) : AT(ADDR(.limine_requests) - KERNEL_VMA + KERNEL_LMA)
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA + KERNEL_LMA)
    {
        *(.data*)
        *(.data.rel.ro*)
    }

    .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - KERNEL_VMA + KERNEL_LMA)
    {
        *(COMMON)
        *(.bss*)
        *(.sbss*)
    }

    __kernel_virtual_end = .;
}
