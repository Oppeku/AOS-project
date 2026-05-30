/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "vga.h"
#include <gfx.h>
#include <multiboot2.h>
#include <stddef.h>
#include <stdint.h>

unsigned char* vga_buffer = (unsigned char*)0xB8000;
#define VGA_TEXT_WIDTH 80
#define VGA_TEXT_HEIGHT 25
#define CONSOLE_MAX_COLS 128
#define CONSOLE_MAX_ROWS 64
#define VGA_HISTORY_ROWS 512
#define FB_CELL_WIDTH 18
#define FB_CELL_HEIGHT 28
#define FB_GLYPH_SCALE 3

static unsigned int vga_mode_cols = VGA_TEXT_WIDTH;
static unsigned int vga_mode_rows = VGA_TEXT_HEIGHT;
static unsigned int vga_detected_cols = VGA_TEXT_WIDTH;
static unsigned int vga_detected_rows = VGA_TEXT_HEIGHT;
static int vga_cursor_x = 0;
static int vga_cursor_row = 0;
static unsigned char vga_default_color = 0x07;
static unsigned char vga_current_color = 0x07;
static int vga_view_offset = 0;
static unsigned char vga_history[VGA_HISTORY_ROWS][CONSOLE_MAX_COLS * 2];
static uint32_t fb_origin_x;
static uint32_t fb_origin_y;

extern void serial_print(const char* s);

static int vga_max_view_offset(void);
static void vga_render(void);
static void fb_update_origin(void);

void vga_init_tty(void) {
    vga_mode_cols = VGA_TEXT_WIDTH;
    vga_mode_rows = VGA_TEXT_HEIGHT;
    vga_detected_cols = VGA_TEXT_WIDTH;
    vga_detected_rows = VGA_TEXT_HEIGHT;
    fb_origin_x = 0;
    fb_origin_y = 0;
    vga_render();
}

static char ascii_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 32);
    }
    return c;
}

static uint8_t glyph_row(char c, int row) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    const uint8_t* g = blank;
    c = ascii_upper(c);

    switch (c) {
        case 'A': { static const uint8_t v[7] = {14, 17, 17, 31, 17, 17, 17}; g = v; break; }
        case 'B': { static const uint8_t v[7] = {30, 17, 17, 30, 17, 17, 30}; g = v; break; }
        case 'C': { static const uint8_t v[7] = {14, 17, 16, 16, 16, 17, 14}; g = v; break; }
        case 'D': { static const uint8_t v[7] = {30, 17, 17, 17, 17, 17, 30}; g = v; break; }
        case 'E': { static const uint8_t v[7] = {31, 16, 16, 30, 16, 16, 31}; g = v; break; }
        case 'F': { static const uint8_t v[7] = {31, 16, 16, 30, 16, 16, 16}; g = v; break; }
        case 'G': { static const uint8_t v[7] = {14, 17, 16, 23, 17, 17, 14}; g = v; break; }
        case 'H': { static const uint8_t v[7] = {17, 17, 17, 31, 17, 17, 17}; g = v; break; }
        case 'I': { static const uint8_t v[7] = {14, 4, 4, 4, 4, 4, 14}; g = v; break; }
        case 'J': { static const uint8_t v[7] = {7, 2, 2, 2, 18, 18, 12}; g = v; break; }
        case 'K': { static const uint8_t v[7] = {17, 18, 20, 24, 20, 18, 17}; g = v; break; }
        case 'L': { static const uint8_t v[7] = {16, 16, 16, 16, 16, 16, 31}; g = v; break; }
        case 'M': { static const uint8_t v[7] = {17, 27, 21, 21, 17, 17, 17}; g = v; break; }
        case 'N': { static const uint8_t v[7] = {17, 25, 21, 19, 17, 17, 17}; g = v; break; }
        case 'O': { static const uint8_t v[7] = {14, 17, 17, 17, 17, 17, 14}; g = v; break; }
        case 'P': { static const uint8_t v[7] = {30, 17, 17, 30, 16, 16, 16}; g = v; break; }
        case 'Q': { static const uint8_t v[7] = {14, 17, 17, 17, 21, 18, 13}; g = v; break; }
        case 'R': { static const uint8_t v[7] = {30, 17, 17, 30, 20, 18, 17}; g = v; break; }
        case 'S': { static const uint8_t v[7] = {15, 16, 16, 14, 1, 1, 30}; g = v; break; }
        case 'T': { static const uint8_t v[7] = {31, 4, 4, 4, 4, 4, 4}; g = v; break; }
        case 'U': { static const uint8_t v[7] = {17, 17, 17, 17, 17, 17, 14}; g = v; break; }
        case 'V': { static const uint8_t v[7] = {17, 17, 17, 17, 17, 10, 4}; g = v; break; }
        case 'W': { static const uint8_t v[7] = {17, 17, 17, 21, 21, 21, 10}; g = v; break; }
        case 'X': { static const uint8_t v[7] = {17, 17, 10, 4, 10, 17, 17}; g = v; break; }
        case 'Y': { static const uint8_t v[7] = {17, 17, 10, 4, 4, 4, 4}; g = v; break; }
        case 'Z': { static const uint8_t v[7] = {31, 1, 2, 4, 8, 16, 31}; g = v; break; }
        case '0': { static const uint8_t v[7] = {14, 17, 19, 21, 25, 17, 14}; g = v; break; }
        case '1': { static const uint8_t v[7] = {4, 12, 4, 4, 4, 4, 14}; g = v; break; }
        case '2': { static const uint8_t v[7] = {14, 17, 1, 2, 4, 8, 31}; g = v; break; }
        case '3': { static const uint8_t v[7] = {30, 1, 1, 14, 1, 1, 30}; g = v; break; }
        case '4': { static const uint8_t v[7] = {2, 6, 10, 18, 31, 2, 2}; g = v; break; }
        case '5': { static const uint8_t v[7] = {31, 16, 16, 30, 1, 1, 30}; g = v; break; }
        case '6': { static const uint8_t v[7] = {14, 16, 16, 30, 17, 17, 14}; g = v; break; }
        case '7': { static const uint8_t v[7] = {31, 1, 2, 4, 8, 8, 8}; g = v; break; }
        case '8': { static const uint8_t v[7] = {14, 17, 17, 14, 17, 17, 14}; g = v; break; }
        case '9': { static const uint8_t v[7] = {14, 17, 17, 15, 1, 1, 14}; g = v; break; }
        case '-': { static const uint8_t v[7] = {0, 0, 0, 31, 0, 0, 0}; g = v; break; }
        case '_': { static const uint8_t v[7] = {0, 0, 0, 0, 0, 0, 31}; g = v; break; }
        case '.': { static const uint8_t v[7] = {0, 0, 0, 0, 0, 12, 12}; g = v; break; }
        case ',': { static const uint8_t v[7] = {0, 0, 0, 0, 0, 4, 8}; g = v; break; }
        case ':': { static const uint8_t v[7] = {0, 12, 12, 0, 12, 12, 0}; g = v; break; }
        case ';': { static const uint8_t v[7] = {0, 12, 12, 0, 4, 4, 8}; g = v; break; }
        case '/': { static const uint8_t v[7] = {1, 1, 2, 4, 8, 16, 16}; g = v; break; }
        case '\\': { static const uint8_t v[7] = {16, 16, 8, 4, 2, 1, 1}; g = v; break; }
        case '|': { static const uint8_t v[7] = {4, 4, 4, 4, 4, 4, 4}; g = v; break; }
        case '+': { static const uint8_t v[7] = {0, 4, 4, 31, 4, 4, 0}; g = v; break; }
        case '*': { static const uint8_t v[7] = {0, 21, 14, 31, 14, 21, 0}; g = v; break; }
        case '=': { static const uint8_t v[7] = {0, 0, 31, 0, 31, 0, 0}; g = v; break; }
        case '<': { static const uint8_t v[7] = {2, 4, 8, 16, 8, 4, 2}; g = v; break; }
        case '>': { static const uint8_t v[7] = {8, 4, 2, 1, 2, 4, 8}; g = v; break; }
        case '[': { static const uint8_t v[7] = {14, 8, 8, 8, 8, 8, 14}; g = v; break; }
        case ']': { static const uint8_t v[7] = {14, 2, 2, 2, 2, 2, 14}; g = v; break; }
        case '(': { static const uint8_t v[7] = {2, 4, 8, 8, 8, 4, 2}; g = v; break; }
        case ')': { static const uint8_t v[7] = {8, 4, 2, 2, 2, 4, 8}; g = v; break; }
        case '#': { static const uint8_t v[7] = {10, 31, 10, 10, 31, 10, 0}; g = v; break; }
        case '$': { static const uint8_t v[7] = {4, 15, 20, 14, 5, 30, 4}; g = v; break; }
        case '%': { static const uint8_t v[7] = {24, 25, 2, 4, 8, 19, 3}; g = v; break; }
        case '&': { static const uint8_t v[7] = {12, 18, 20, 8, 21, 18, 13}; g = v; break; }
        case '!': { static const uint8_t v[7] = {4, 4, 4, 4, 4, 0, 4}; g = v; break; }
        case '?': { static const uint8_t v[7] = {14, 17, 1, 2, 4, 0, 4}; g = v; break; }
        case '"': { static const uint8_t v[7] = {10, 10, 10, 0, 0, 0, 0}; g = v; break; }
        case '\'': { static const uint8_t v[7] = {4, 4, 8, 0, 0, 0, 0}; g = v; break; }
        case '`': { static const uint8_t v[7] = {8, 4, 2, 0, 0, 0, 0}; g = v; break; }
        case '~': { static const uint8_t v[7] = {0, 0, 8, 21, 2, 0, 0}; g = v; break; }
        case '@': { static const uint8_t v[7] = {14, 17, 23, 21, 23, 16, 14}; g = v; break; }
    }

    if (row < 0 || row >= 7) return 0;
    return g[row];
}

static void vga_clear_history_row(int row, unsigned char color) {
    for (int x = 0; x < CONSOLE_MAX_COLS; x++) {
        vga_history[row][x * 2] = ' ';
        vga_history[row][x * 2 + 1] = color;
    }
}

static uint32_t attr_color_to_rgb(uint8_t color) {
    static const uint32_t palette[16] = {
        0x05070a, 0x172554, 0x14532d, 0x164e63,
        0x7f1d1d, 0x581c87, 0x854d0e, 0xd1d5db,
        0x6b7280, 0x2563eb, 0x22c55e, 0x06b6d4,
        0xef4444, 0xa855f7, 0xfacc15, 0xf9fafb
    };
    return palette[color & 0x0F];
}

static void fb_clear_screen(uint8_t attr) {
    uint32_t bg = attr_color_to_rgb((uint8_t)(attr >> 4));
    gfx_clear(bg);
}

static void fb_draw_char(char c, uint8_t attr, uint32_t cell_x, uint32_t cell_y) {
    uint32_t x0 = fb_origin_x + cell_x * FB_CELL_WIDTH;
    uint32_t y0 = fb_origin_y + cell_y * FB_CELL_HEIGHT;
    uint32_t fg = attr_color_to_rgb(attr & 0x0F);
    uint32_t bg = attr_color_to_rgb((uint8_t)(attr >> 4));

    gfx_fill_rect(x0, y0, FB_CELL_WIDTH, FB_CELL_HEIGHT, bg);
    if (c == ' ') return;

    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyph_row(c, row);
        for (int col = 0; col < 5; col++) {
            if (bits & (uint8_t)(1U << (4 - col))) {
                uint32_t px = x0 + 2 + (uint32_t)col * FB_GLYPH_SCALE;
                uint32_t py = y0 + 3 + (uint32_t)row * FB_GLYPH_SCALE;
                gfx_fill_rect(px, py, FB_GLYPH_SCALE, FB_GLYPH_SCALE, fg);
            }
        }
    }
}

static void fb_render(void) {
    int max_offset = vga_max_view_offset();
    int start_row;

    if (!gfx_is_ready()) return;
    if (vga_view_offset > max_offset) {
        vga_view_offset = max_offset;
    }

    start_row = vga_cursor_row - ((int)vga_mode_rows - 1) - vga_view_offset;
    if (start_row < 0) {
        start_row = 0;
    }

    for (unsigned int y = 0; y < vga_mode_rows; y++) {
        int source_row = start_row + y;
        for (unsigned int x = 0; x < vga_mode_cols; x++) {
            char ch = ' ';
            uint8_t attr = vga_default_color;
            if (source_row <= vga_cursor_row && source_row < VGA_HISTORY_ROWS) {
                ch = (char)vga_history[source_row][x * 2];
                attr = vga_history[source_row][x * 2 + 1];
            }
            fb_draw_char(ch, attr, (uint32_t)x, (uint32_t)y);
        }
    }
    gfx_present();
}

static int fb_visible_start_row(void) {
    int start_row = vga_cursor_row - ((int)vga_mode_rows - 1) - vga_view_offset;
    if (start_row < 0) {
        start_row = 0;
    }
    return start_row;
}

static void fb_draw_history_cell(int history_row, int x) {
    int screen_y;
    int start_row;
    char ch;
    uint8_t attr;

    if (!gfx_is_ready() || x < 0 || x >= (int)vga_mode_cols) return;
    start_row = fb_visible_start_row();
    screen_y = history_row - start_row;
    if (screen_y < 0 || screen_y >= (int)vga_mode_rows) return;
    ch = (char)vga_history[history_row][x * 2];
    attr = vga_history[history_row][x * 2 + 1];
    fb_draw_char(ch, attr, (uint32_t)x, (uint32_t)screen_y);
}

static void fb_update_origin(void) {
    uint32_t width = gfx_width();
    uint32_t height = gfx_height();
    uint32_t cols = width / FB_CELL_WIDTH;
    uint32_t rows = height / FB_CELL_HEIGHT;

    if (cols > CONSOLE_MAX_COLS) cols = CONSOLE_MAX_COLS;
    if (rows > CONSOLE_MAX_ROWS) rows = CONSOLE_MAX_ROWS;
    if (cols < 40) cols = 40;
    if (rows < 10) rows = 10;

    vga_detected_cols = cols;
    vga_detected_rows = rows;
    vga_mode_cols = cols;
    vga_mode_rows = rows;
    fb_origin_x = width > vga_mode_cols * FB_CELL_WIDTH ? (width - vga_mode_cols * FB_CELL_WIDTH) / 2 : 0;
    fb_origin_y = height > vga_mode_rows * FB_CELL_HEIGHT ? (height - vga_mode_rows * FB_CELL_HEIGHT) / 2 : 0;
}

void vga_init_framebuffer(uint64_t mb_info) {
    struct multiboot_tag* tag;

    if (gfx_init_bochs() == 0) {
        fb_update_origin();
        return;
    }

    if (mb_info == 0) return;

    for (tag = (struct multiboot_tag*)(uintptr_t)(mb_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            struct multiboot_tag_framebuffer* fb = (struct multiboot_tag_framebuffer*)tag;
            uint64_t phys = fb->framebuffer_addr;

            if (fb->framebuffer_type != 1 || (fb->framebuffer_bpp != 32 && fb->framebuffer_bpp != 24)) {
                return;
            }

            if (gfx_init_framebuffer(phys, fb->framebuffer_width, fb->framebuffer_height,
                                     fb->framebuffer_pitch, fb->framebuffer_bpp) == 0) {
                fb_update_origin();
                serial_print("Framebuffer: Multiboot LFB enabled\n");
                return;
            }
        }
    }
}

static void vga_shift_history_up(void) {
    for (int row = 0; row < VGA_HISTORY_ROWS - 1; row++) {
        for (int col = 0; col < CONSOLE_MAX_COLS * 2; col++) {
            vga_history[row][col] = vga_history[row + 1][col];
        }
    }
    vga_clear_history_row(VGA_HISTORY_ROWS - 1, vga_default_color);
    if (vga_cursor_row > 0) {
        vga_cursor_row--;
    }
}

static int vga_max_view_offset(void) {
    int used_rows = vga_cursor_row + 1;
    if (used_rows <= (int)vga_mode_rows) {
        return 0;
    }
    return used_rows - (int)vga_mode_rows;
}

static void vga_render(void) {
    if (gfx_is_ready()) {
        fb_render();
        return;
    }

    int max_offset = vga_max_view_offset();
    if (vga_view_offset > max_offset) {
        vga_view_offset = max_offset;
    }

    int start_row = vga_cursor_row - (VGA_TEXT_HEIGHT - 1) - vga_view_offset;
    if (start_row < 0) {
        start_row = 0;
    }

    for (int y = 0; y < VGA_TEXT_HEIGHT; y++) {
        int source_row = start_row + y;
        for (int x = 0; x < VGA_TEXT_WIDTH; x++) {
            int dst = (y * VGA_TEXT_WIDTH + x) * 2;
            if (source_row <= vga_cursor_row && source_row < VGA_HISTORY_ROWS && x < (int)vga_mode_cols) {
                vga_buffer[dst] = vga_history[source_row][x * 2];
                vga_buffer[dst + 1] = vga_history[source_row][x * 2 + 1];
            } else {
                vga_buffer[dst] = ' ';
                vga_buffer[dst + 1] = vga_default_color;
            }
        }
    }
}

void vga_clear(unsigned char color) {
    for (int row = 0; row < VGA_HISTORY_ROWS; row++) {
        vga_clear_history_row(row, color);
    }
    vga_cursor_x = 0;
    vga_cursor_row = 0;
    vga_view_offset = 0;
    vga_default_color = color;
    vga_current_color = color;
    if (gfx_is_ready()) {
        fb_clear_screen(color);
    }
    vga_render();
}

void vga_print(const char* str, unsigned char color, int x, int y) {
    if (gfx_is_ready()) {
        if (y < 0 || y >= VGA_HISTORY_ROWS) return;
        for (int i = 0; str[i] != '\0' && x + i < (int)vga_mode_cols; i++) {
            int index = (x + i) * 2;
            vga_history[y][index] = str[i];
            vga_history[y][index + 1] = color;
        }
        if (y > vga_cursor_row) {
            vga_cursor_row = y;
        }
        vga_render();
        return;
    }

    if (x < 0 || y < 0 || x >= VGA_TEXT_WIDTH || y >= VGA_TEXT_HEIGHT) return;
    int index = (y * VGA_TEXT_WIDTH + x) * 2;
    for (int i = 0; str[i] != '\0' && x + i < VGA_TEXT_WIDTH; i++) {
        vga_buffer[index + (i * 2)] = str[i];
        vga_buffer[index + (i * 2) + 1] = color;
    }
}

void vga_scroll() {
    if (vga_cursor_row + 1 >= VGA_HISTORY_ROWS) {
        vga_shift_history_up();
    }
    vga_render();
}

void vga_scrollback_up(void) {
    int max_offset = vga_max_view_offset();
    if (vga_view_offset < max_offset) {
        vga_view_offset++;
        vga_render();
    }
}

void vga_scrollback_down(void) {
    if (vga_view_offset > 0) {
        vga_view_offset--;
        vga_render();
    }
}

static void vga_ensure_bottom_view(void) {
    if (vga_view_offset != 0) {
        vga_view_offset = 0;
    }
}

static void vga_set_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (x >= (int)vga_mode_cols) x = (int)vga_mode_cols - 1;
    if (y < 0) y = 0;
    if (y >= (int)vga_mode_rows) y = (int)vga_mode_rows - 1;
    vga_cursor_x = x;
    vga_cursor_row = y;
}

static int ansi_parse_number(const char* str, int* index) {
    int value = 0;
    int found = 0;

    while (str[*index] >= '0' && str[*index] <= '9') {
        found = 1;
        value = value * 10 + (str[*index] - '0');
        (*index)++;
    }
    return found ? value : 0;
}

static void vga_clear_to_end_of_line(void) {
    for (int x = vga_cursor_x; x < (int)vga_mode_cols; x++) {
        int index = x * 2;
        vga_history[vga_cursor_row][index] = ' ';
        vga_history[vga_cursor_row][index + 1] = vga_current_color;
        fb_draw_history_cell(vga_cursor_row, x);
    }
}

void vga_get_display_mode(unsigned int* cols, unsigned int* rows,
                          unsigned int* detected_cols, unsigned int* detected_rows,
                          unsigned int* max_cols, unsigned int* max_rows) {
    if (cols) *cols = vga_mode_cols;
    if (rows) *rows = vga_mode_rows;
    if (detected_cols) *detected_cols = vga_detected_cols;
    if (detected_rows) *detected_rows = vga_detected_rows;
    if (max_cols) *max_cols = vga_detected_cols;
    if (max_rows) *max_rows = vga_detected_rows;
}

int vga_set_display_mode(unsigned int cols, unsigned int rows) {
    if (cols < 40 || rows < 10 || cols > vga_detected_cols || rows > vga_detected_rows) {
        return -1;
    }
    vga_mode_cols = cols;
    vga_mode_rows = rows;
    if (gfx_is_ready()) {
        fb_origin_x = gfx_width() > vga_mode_cols * FB_CELL_WIDTH ? (gfx_width() - vga_mode_cols * FB_CELL_WIDTH) / 2 : 0;
        fb_origin_y = gfx_height() > vga_mode_rows * FB_CELL_HEIGHT ? (gfx_height() - vga_mode_rows * FB_CELL_HEIGHT) / 2 : 0;
        vga_render();
    }
    return 0;
}

void vga_auto_display_mode(void) {
    if (gfx_is_ready()) {
        fb_update_origin();
        vga_render();
        return;
    }
    vga_mode_cols = VGA_TEXT_WIDTH;
    vga_mode_rows = VGA_TEXT_HEIGHT;
    vga_detected_cols = VGA_TEXT_WIDTH;
    vga_detected_rows = VGA_TEXT_HEIGHT;
}

void vga_write(const char* str, unsigned char color) {
    int need_full_render = 0;

    vga_ensure_bottom_view();
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (c == 27 && str[i + 1] == '[') {
            int p = i + 2;
            int private_mode = 0;
            int first = 0;
            int second = 0;
            char command = 0;

            if (str[p] == '?') {
                private_mode = 1;
                p++;
            }

            first = ansi_parse_number(str, &p);
            if (str[p] == ';') {
                p++;
                second = ansi_parse_number(str, &p);
            }
            command = str[p];
            if (command == '\0') {
                break;
            }

            if (!private_mode && command == 'J' && first == 2) {
                vga_clear(vga_default_color);
            } else if (!private_mode && command == 'H') {
                int row = first ? first : 1;
                int col = second ? second : 1;
                vga_set_cursor(col - 1, row - 1);
                need_full_render = 1;
            } else if (!private_mode && command == 'm') {
                if (first == 7) {
                    vga_current_color = 0x70;
                } else {
                    vga_current_color = vga_default_color;
                }
            } else if (!private_mode && command == 'K') {
                vga_clear_to_end_of_line();
            }

            i = p;
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\b') {
            if (vga_cursor_x > 0) {
                vga_cursor_x--;
            } else if (vga_cursor_row > 0) {
                vga_cursor_row--;
                vga_cursor_x = (int)vga_mode_cols - 1;
            } else {
                continue;
            }

            int index = vga_cursor_x * 2;
            vga_history[vga_cursor_row][index] = ' ';
            vga_history[vga_cursor_row][index + 1] = vga_current_color;
            fb_draw_history_cell(vga_cursor_row, vga_cursor_x);
            continue;
        }
        if (c == '\n') {
            vga_cursor_x = 0;
            vga_cursor_row++;
            if (vga_cursor_row >= VGA_HISTORY_ROWS) {
                vga_shift_history_up();
                vga_cursor_row = VGA_HISTORY_ROWS - 1;
            }
            vga_clear_history_row(vga_cursor_row, vga_default_color);
            if (vga_cursor_row >= (int)vga_mode_rows) {
                need_full_render = 1;
            }
        } else {
            int index = vga_cursor_x * 2;
            int draw_x = vga_cursor_x;
            int draw_row = vga_cursor_row;
            vga_history[vga_cursor_row][index] = c;
            vga_history[vga_cursor_row][index + 1] = vga_current_color ? vga_current_color : color;
            fb_draw_history_cell(draw_row, draw_x);
            vga_cursor_x++;
            if (vga_cursor_x >= (int)vga_mode_cols) {
                vga_cursor_x = 0;
                vga_cursor_row++;
                if (vga_cursor_row >= VGA_HISTORY_ROWS) {
                    vga_shift_history_up();
                    vga_cursor_row = VGA_HISTORY_ROWS - 1;
                }
                vga_clear_history_row(vga_cursor_row, vga_default_color);
                if (vga_cursor_row >= (int)vga_mode_rows) {
                    need_full_render = 1;
                }
            }
        }
    }
    if (!gfx_is_ready() || need_full_render) {
        vga_render();
    } else {
        gfx_present();
    }
}
