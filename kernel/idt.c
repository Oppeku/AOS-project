/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "idt.h"
#include "panic.h"
#include "vga.h"
#include "process.h"
#include "vmm.h"
#include <stdint.h>

struct idt_entry idt[256];
struct idt_ptr idtp;

// These are the "Stubs" inside interrupts.asm
extern void load_idt(uint64_t);
extern void generic_stub();
extern void breakpoint_stub();
extern void invalid_opcode_stub();
extern void gpf_stub();
extern void page_fault_stub();
extern void keyboard_stub();
extern void mouse_stub();
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

struct exception_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

static void serial_print_hex64(uint64_t value) {
    char hex[19];

    hex[0] = '0';
    hex[1] = 'x';
    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (uint8_t)((value >> (60 - i * 4)) & 0xF);
        hex[2 + i] = (char)(nibble < 10 ? '0' + nibble
                                       : 'A' + nibble - 10);
    }
    hex[18] = '\0';
    serial_print(hex);
}

static void serial_print_exception_frame(const struct exception_frame* frame) {
    if (!frame) return;
    serial_print("  RIP=");
    serial_print_hex64(frame->rip);
    serial_print(" CS=");
    serial_print_hex64(frame->cs);
    serial_print(" ERROR=");
    serial_print_hex64(frame->error_code);
    serial_print("\n  RAX=");
    serial_print_hex64(frame->rax);
    serial_print(" RDI=");
    serial_print_hex64(frame->rdi);
    serial_print(" RSI=");
    serial_print_hex64(frame->rsi);
    serial_print("\n  RDX=");
    serial_print_hex64(frame->rdx);
    serial_print(" RBX=");
    serial_print_hex64(frame->rbx);
    serial_print(" RBP=");
    serial_print_hex64(frame->rbp);
    serial_print(" RSP=");
    if ((frame->cs & 3U) == 3U) {
        serial_print_hex64(frame->rsp);
    } else {
        serial_print("(kernel stack)");
    }
    serial_print("\n");
}

static uint64_t read_msr_u64(uint32_t msr) {
    uint32_t low;
    uint32_t high;

    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static int readable_user_u64(uint64_t address, uint64_t* value) {
    if (!value || !current_process ||
        address < AOS_USER_SPACE_START ||
        address > 0x00007FFFFFFFFFF8ULL ||
        !vmm_page_present(current_process->p4_table, address) ||
        !vmm_page_present(current_process->p4_table, address + 7U)) {
        return -1;
    }
    *value = *(const uint64_t*)(uintptr_t)address;
    return 0;
}

static void serial_print_user_tls_diagnostics(
    const struct exception_frame* frame) {
    const uint32_t ia32_fs_base = 0xC0000100U;
    uint64_t hardware_fs = read_msr_u64(ia32_fs_base);
    uint64_t tls_canary;
    uint64_t stack_canary;

    serial_print("  PID=");
    serial_print_hex64(current_process ? current_process->pid : 0);
    serial_print(" TRACKED_FS=");
    serial_print_hex64(current_process ? current_process->fs_base : 0);
    serial_print(" HW_FS=");
    serial_print_hex64(hardware_fs);
    serial_print("\n");
    if (hardware_fs <= UINT64_MAX - 0x28U &&
        readable_user_u64(hardware_fs + 0x28U, &tls_canary) == 0) {
        serial_print("  TLS_CANARY=");
        serial_print_hex64(tls_canary);
    } else {
        serial_print("  TLS_CANARY=(unmapped)");
    }
    if (frame && frame->rbp >= 0x30U &&
        readable_user_u64(frame->rbp - 0x30U, &stack_canary) == 0) {
        serial_print(" STACK_CANARY=");
        serial_print_hex64(stack_canary);
    } else {
        serial_print(" STACK_CANARY=(unmapped)");
    }
    serial_print("\n");
}

// Handler functions that the Assembly calls
void breakpoint_handler(struct exception_frame* frame) {
    serial_print("EXCEPTION: BREAKPOINT\n");
    serial_print_exception_frame(frame);
    if (frame && (frame->cs & 3U) == 3U) {
        serial_print_user_tls_diagnostics(frame);
        syscall_kill_current_process(133);
    }
    aos_panic("Breakpoint exception (03)", "A kernel breakpoint was reached.");
}

void invalid_opcode_handler(struct exception_frame* frame) {
    serial_print("EXCEPTION: INVALID OPCODE\n");
    serial_print_exception_frame(frame);
    if (frame && (frame->cs & 3U) == 3U) {
        syscall_kill_current_process(132);
    }
    aos_panic("Invalid opcode exception (06)", "The CPU rejected an instruction.");
}

void gpf_handler(struct exception_frame* frame) {
    serial_print("EXCEPTION: GENERAL PROTECTION FAULT\n");
    serial_print_exception_frame(frame);
    if (frame && (frame->cs & 3U) == 3U) {
        syscall_kill_current_process(139);
    }
    aos_panic("General protection fault (13)", "Kernel protection rules were violated.");
}

void page_fault_handler(struct exception_frame* frame) {
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));
    if (frame && (frame->error_code & 3U) == 3U && current_process &&
        vmm_handle_cow_fault(current_process->p4_table, cr2) == 0) {
        return;
    }
    serial_print("EXCEPTION: PAGE FAULT at ");
    serial_print_hex64(cr2);
    serial_print("\n");
    serial_print_exception_frame(frame);
    if (frame && (frame->cs & 3U) == 3U) {
        syscall_kill_current_process(139);
    }
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
    set_idt_gate(3,  (uint64_t)breakpoint_stub,     0x08, 0xEE);
    set_idt_gate(6,  (uint64_t)invalid_opcode_stub, 0x08, 0x8E);
    set_idt_gate(13, (uint64_t)gpf_stub,            0x08, 0x8E);
    set_idt_gate(14, (uint64_t)page_fault_stub,     0x08, 0x8E);
    set_idt_gate(32, (uint64_t)timer_stub,          0x08, 0x8E); // INT 0x20 = 32
    set_idt_gate(33, (uint64_t)keyboard_stub,       0x08, 0x8E); // IRQ1 remapped to 33
    set_idt_gate(44, (uint64_t)mouse_stub,          0x08, 0x8E); // IRQ12 remapped to 44

    load_idt((uint64_t)&idtp);
}
