/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>
#include <vga.h>

// Basic US QWERTY Scancode Map
unsigned char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',   
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,         
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
};

static unsigned char kbd_us_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define KEYBOARD_BUFFER_SIZE 1024
#define KEY_HISTORY_PREV 0x11
#define KEY_HISTORY_NEXT 0x12
#define KEY_LEFT 0x13
#define KEY_RIGHT 0x14
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t buffer_head = 0;
static uint32_t buffer_tail = 0;
static uint8_t key_down[128] = {0};
static uint8_t shift_down = 0;
static uint8_t ctrl_down = 0;
static uint8_t extended_prefix = 0;

static void keyboard_push(char c) {
    uint32_t next = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != buffer_tail) {
        keyboard_buffer[buffer_head] = c;
        buffer_head = next;
    }
}

char keyboard_pop() {
    if (buffer_head == buffer_tail) return 0;
    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_handler_main() {
    if (!(inb(0x64) & 0x01)) {
        return;
    }

    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) {
        extended_prefix = 1;
        return;
    }

    if (extended_prefix) {
        extended_prefix = 0;
        if (scancode & 0x80) {
            return;
        }

        switch (scancode) {
            case 0x48:
                keyboard_push(KEY_HISTORY_PREV);
                return;
            case 0x50:
                keyboard_push(KEY_HISTORY_NEXT);
                return;
            case 0x4B:
                keyboard_push(KEY_LEFT);
                return;
            case 0x4D:
                keyboard_push(KEY_RIGHT);
                return;
            case 0x49:
                vga_scrollback_up();
                return;
            case 0x51:
                vga_scrollback_down();
                return;
            default:
                return;
        }
    }

    uint8_t keycode = scancode & 0x7F;

    if (keycode >= 128) {
        return;
    }

    if (scancode & 0x80) {
        key_down[keycode] = 0;
        if (keycode == 42 || keycode == 54) {
            shift_down = 0;
        }
        if (keycode == 29) {
            ctrl_down = 0;
        }
        return;
    }

    if (key_down[keycode]) {
        return;
    }
    key_down[keycode] = 1;

    if (keycode == 42 || keycode == 54) {
        shift_down = 1;
        return;
    }
    if (keycode == 29) {
        ctrl_down = 1;
        return;
    }

    unsigned char c = shift_down ? kbd_us_shift[keycode] : kbd_us[keycode];
    if (c != 0) {
        if (ctrl_down && c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 'a' + 1);
        } else if (ctrl_down && c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 1);
        }
        keyboard_push((char)c);
    }
}
