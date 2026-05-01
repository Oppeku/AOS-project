/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <panic.h>
#include <vga.h>

extern void serial_print(const char* s);

static void serial_print_line(const char* s) {
    serial_print(s);
    serial_print("\n");
}

static void append_string(char* dst, unsigned int dst_size, const char* src) {
    unsigned int i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    while (dst[i] != '\0' && i + 1 < dst_size) {
        i++;
    }
    while (src && *src && i + 1 < dst_size) {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
}

static void append_hex64(char* dst, unsigned int dst_size, uint64_t value) {
    char buf[19];

    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (value >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = (nibble < 10) ? (char)('0' + nibble) : (char)('A' + (nibble - 10));
    }
    buf[18] = '\0';
    append_string(dst, dst_size, buf);
}

void aos_panic(const char* title, const char* detail) {
    serial_print_line("AOS PANIC");
    if (title && *title) {
        serial_print_line(title);
    }
    if (detail && *detail) {
        serial_print_line(detail);
    }

    vga_clear(0x4F);
    vga_print("AOS KERNEL PANIC", 0x4F, 2, 1);
    if (title && *title) {
        vga_print((char*)title, 0x4F, 2, 3);
    }
    if (detail && *detail) {
        vga_print((char*)detail, 0x4F, 2, 5);
    }
    vga_print("System halted.", 0x4F, 2, 8);

    while (1) {
        asm volatile("hlt");
    }
}

void aos_panic_hex(const char* title, const char* detail_prefix, uint64_t value) {
    char detail[96];
    detail[0] = '\0';

    if (detail_prefix && *detail_prefix) {
        append_string(detail, sizeof(detail), detail_prefix);
    }
    append_hex64(detail, sizeof(detail), value);
    aos_panic(title, detail);
}
