/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Standard x86_64 IDT entry (16 bytes)
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  ist;       // Interrupt Stack Table
    uint8_t  flags;     // Gate Type and DPL
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void init_idt(void);
void set_idt_gate(int n, uint64_t handler, uint16_t sel, uint8_t flags);

#endif
