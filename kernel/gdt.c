/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <gdt.h>

struct tss_entry kernel_tss;

// Define the GDT here so the linker is happy.
// We need 5 standard entries + 2 slots for the 16-byte TSS = 7 slots.
uint64_t gdt_entries[7]; 

// This structure is for the GDTR register
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern uint64_t stack_top;
extern void load_tss(uint16_t);

void init_gdt() {
    // 1. Setup standard GDT entries
    gdt_entries[0] = 0;             // Null segment
    gdt_entries[1] = 0x00209A0000000000; // Kernel Code (64-bit)
    gdt_entries[2] = 0x0000920000000000; // Kernel Data (64-bit)
    gdt_entries[3] = 0x0020F2000000FFFF; // User Data (64-bit, DPL 3)
    gdt_entries[4] = 0x0020FA000000FFFF; // User Code (64-bit, DPL 3)

    // 2. Zero out the TSS
    uint8_t* p = (uint8_t*)&kernel_tss;
    for(uint32_t i = 0; i < sizeof(struct tss_entry); i++) p[i] = 0;

    // 3. Set the kernel stack for interrupts
    kernel_tss.rsp0 = (uint64_t)&stack_top;

    // 4. Fill the 16-byte TSS Descriptor (spread across gdt_entries[5] and [6])
    uint64_t base = (uint64_t)&kernel_tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    struct gdt_tss_descriptor* tss_desc = (struct gdt_tss_descriptor*)&gdt_entries[5];
    tss_desc->limit_low = limit & 0xFFFF;
    tss_desc->base_low = base & 0xFFFF;
    tss_desc->base_mid = (base >> 16) & 0xFF;
    tss_desc->access = 0x89; // Present, 64-bit TSS
    tss_desc->limit_high = (limit >> 16) & 0x0F;
    tss_desc->base_high_mid = (base >> 24) & 0xFF;
    tss_desc->base_high = (base >> 32) & 0xFFFFFFFF;
    tss_desc->reserved = 0;

    // 5. Load the new GDT
    struct gdt_ptr gp;
    gp.limit = (sizeof(uint64_t) * 7) - 1;
    gp.base = (uint64_t)&gdt_entries;
    
    __asm__ volatile("lgdt %0" : : "m"(gp));

    // 6. Load the TSS selector (0x28 is the 5th entry)
    load_tss(0x28);
}
