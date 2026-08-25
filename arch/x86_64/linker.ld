ENTRY(kernel_main)

SECTIONS
{
    . = 0xffffffff80000000;

    .text ALIGN(4K) :
    {
        *(.text*)
    }

    /* Auto-registered driver/component initializers (see include/drivers.h).
       Kept contiguous so the kernel can iterate __init_start..__init_end. */
    .init_array :
    {
        PROVIDE_HIDDEN (__init_start = .);
        KEEP(*(.drivers.init))
        PROVIDE_HIDDEN (__init_end = .);
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }

    .data ALIGN(4K) :
    {
        *(.data*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }
}
