/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "idt.h"
#include "panic.h"
#include "vga.h"
#include <stdint.h>

struct idt_entry idt[256];
struct idt_ptr idtp;

// These are the "Stubs" inside interrupts.asm
extern void load_idt(uint64_t);
extern void generic_stub();
extern void invalid_opcode_stub();
extern void gpf_stub();
extern void page_fault_stub();
extern void keyboard_stub();
extern void timer_stub();

void set_idt_gate(int n, uint64_t handler, uint16_t sel, uint8_t flags) {
    idt[n].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].base_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].base_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[n].selector  = sel;
    idt[n].ist       = 0;
    idt[n].flags     = flags;
    idt[n].reserved  = 0;
}

extern void serial_print(const char* s);
extern void syscall_kill_current_process(int exit_code);

// Handler functions that the Assembly calls
void invalid_opcode_handler() {
    syscall_kill_current_process(132);
    serial_print("EXCEPTION: INVALID OPCODE\n");
    aos_panic("Invalid opcode exception (06)", "The CPU rejected an instruction.");
}

void gpf_handler() {
    serial_print("EXCEPTION: GENERAL PROTECTION FAULT\n");
    aos_panic("General protection fault (13)", "Kernel protection rules were violated.");
}

void page_fault_handler() {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    serial_print("EXCEPTION: PAGE FAULT at ");
    char hex[19];
    hex[0] = '0';
    hex[1] = 'x';
    for (int i = 0; i < 16; i++) {
        int nibble = (cr2 >> (60 - i * 4)) & 0xF;
        hex[2 + i] = (char)((nibble < 10) ? (nibble + '0') : (nibble - 10 + 'A'));
    }
    hex[18] = '\0';
    serial_print(hex);
    serial_print("\n");
    aos_panic_hex("Page fault exception (14)", "Fault address: ", cr2);
}

void generic_handler() {
    vga_print("Interrupt Received", 0x07, 0, 0);
}

void init_idt() {
    idtp.limit = (uint16_t)(sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint64_t)&idt;

    // Fill all 256 gates with the generic stub first
    for (int i = 0; i < 256; i++) {
        set_idt_gate(i, (uint64_t)generic_stub, 0x08, 0x8E);
    }

    // Register specific handlers for crashes
    set_idt_gate(6,  (uint64_t)invalid_opcode_stub, 0x08, 0xEE);
    set_idt_gate(13, (uint64_t)gpf_stub,            0x08, 0xEE);
    set_idt_gate(14, (uint64_t)page_fault_stub,     0x08, 0xEE);
    set_idt_gate(32, (uint64_t)timer_stub,          0x08, 0x8E); // INT 0x20 = 32
    set_idt_gate(33, (uint64_t)keyboard_stub,       0x08, 0xEE); // IRQ1 remapped to 33

    load_idt((uint64_t)&idtp);
}
