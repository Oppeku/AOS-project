/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <input.h>
#include <tty.h>
#include <vga.h>
#include <stddef.h>
#include <stdint.h>

#define TTY_ESC 0x1B
#define TTY_CMD_COLOR 'C'
#define TTY_CMD_RESET 'R'
#define TTY_CMD_CLEAR 'L'

static unsigned char tty_output_color = 0x0F;

extern void outb(uint16_t port, uint8_t val);
extern void keyboard_handler_main(void);

static void tty_wait_for_input_irq(void) {
    asm volatile("sti; hlt; cli" ::: "memory");
}

void tty_init(void) {
    tty_output_color = 0x0F;
}

void tty_keyboard_input(char c) {
    input_push_char(AOS_INPUT_SOURCE_PS2, c);
}

char tty_read_byte(void) {
    return input_read_tty_byte();
}

uint32_t tty_pending(void) {
    return input_tty_pending();
}

uint64_t tty_read(char* buf, uint64_t len) {
    uint64_t bytes_read = 0;

    if (!buf || len == 0) {
        return 0;
    }

    while (bytes_read < len) {
        char c = tty_read_byte();
        if (c == 0) {
            if (bytes_read == 0) {
                keyboard_handler_main();
                tty_wait_for_input_irq();
                continue;
            }
            break;
        }
        buf[bytes_read++] = c;
        if (c == '\n' || c == '\r') {
            break;
        }
    }

    return bytes_read;
}

static void tty_serial_write(const char* buf, uint64_t len) {
    for (uint64_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\0') {
            break;
        }
        if (c == TTY_ESC && i + 1 < len) {
            char cmd = buf[++i];
            if (cmd == TTY_CMD_COLOR) {
                if (i + 1 < len) {
                    i++;
                }
                continue;
            }
            if (cmd == TTY_CMD_RESET || cmd == TTY_CMD_CLEAR) {
                continue;
            }
        }
        outb(0x3F8, (uint8_t)c);
    }
}

static void tty_console_write(const char* buf, uint64_t len) {
    char out[128];
    uint64_t i = 0;

    while (i < len) {
        uint64_t out_len = 0;

        while (i < len && out_len + 1 < sizeof(out)) {
            char c = buf[i++];
            if (c == '\0') {
                break;
            }
            if (c == TTY_ESC && i < len) {
                char cmd = buf[i++];
                if (cmd == TTY_CMD_COLOR) {
                    if (i < len) {
                        tty_output_color = (unsigned char)buf[i++];
                    }
                    continue;
                }
                if (cmd == TTY_CMD_RESET) {
                    tty_output_color = 0x0F;
                    continue;
                }
                if (cmd == TTY_CMD_CLEAR) {
                    vga_clear(tty_output_color);
                    tty_output_color = 0x0F;
                    continue;
                }
                out[out_len++] = c;
                out[out_len++] = cmd;
                continue;
            }
            out[out_len++] = c;
        }

        if (out_len > 0) {
            out[out_len] = '\0';
            vga_write(out, tty_output_color);
        }
    }
}

uint64_t tty_write(const char* buf, uint64_t len) {
    if (!buf || len == 0) {
        return 0;
    }

    if (len > 4096) {
        len = 4096;
    }

    tty_serial_write(buf, len);
    tty_console_write(buf, len);
    return len;
}

void tty_get_winsize(uint16_t* rows, uint16_t* cols) {
    unsigned int c = 80;
    unsigned int r = 25;

    vga_get_display_mode(&c, &r, NULL, NULL, NULL, NULL);
    if (rows) {
        *rows = (uint16_t)r;
    }
    if (cols) {
        *cols = (uint16_t)c;
    }
}
