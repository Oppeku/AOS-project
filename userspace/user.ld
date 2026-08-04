/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

ENTRY(_start)

SECTIONS
{
    /*
     * Addresses below 1 TiB share the kernel's bootstrap page-table branch.
     * Native programs must live above AOS_USER_SPACE_START so fork/exec can
     * copy and destroy their mappings without changing another process.
     */
    . = 0x0000010000100000;

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(.bss*)
        *(COMMON)
    }
}
