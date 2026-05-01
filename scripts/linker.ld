/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

ENTRY(_start)

SECTIONS
{
    . = 1M;
    __kernel_start = .;

    .boot : ALIGN(4K)
    {
        KEEP(*(.boot))
    }

    /* Force code into its own 4KB page */
    .text : ALIGN(4K)
    {
        *(.text)
    }

    /* Force strings and constants into a separate 4KB page */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    __kernel_end = .;
}
