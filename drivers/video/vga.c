/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "vga.h"
#include <gfx.h>
#include <multiboot2.h>
#include <stddef.h>
#include <stdint.h>

unsigned char* vga_buffer = (unsigned char*)0xB8000;
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_HISTORY_ROWS 256
#define FB_CELL_WIDTH 10
#define FB_CELL_HEIGHT 18
#define FB_GLYPH_SCALE 2

static unsigned int vga_mode_cols = VGA_WIDTH;
static unsigned int vga_mode_rows = VGA_HEIGHT;
static int vga_cursor_x = 0;
static int vga_cursor_row = 0;
static unsigned char vga_default_color = 0x07;
static unsigned char vga_current_color = 0x07;
static int vga_view_offset = 0;
static unsigned char vga_history[VGA_HISTORY_ROWS][VGA_WIDTH * 2];
static uint32_t fb_origin_x;
static uint32_t fb_origin_y;

extern void serial_print(const char* s);

static int vga_max_view_offset(void);

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
    for (int x = 0; x < VGA_WIDTH; x++) {
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
                uint32_t py = y0 + 2 + (uint32_t)row * FB_GLYPH_SCALE;
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

    start_row = vga_cursor_row - (VGA_HEIGHT - 1) - vga_view_offset;
    if (start_row < 0) {
        start_row = 0;
    }

    for (int y = 0; y < VGA_HEIGHT; y++) {
        int source_row = start_row + y;
        for (int x = 0; x < VGA_WIDTH; x++) {
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
    int start_row = vga_cursor_row - (VGA_HEIGHT - 1) - vga_view_offset;
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

    if (!gfx_is_ready() || x < 0 || x >= VGA_WIDTH) return;
    start_row = fb_visible_start_row();
    screen_y = history_row - start_row;
    if (screen_y < 0 || screen_y >= VGA_HEIGHT) return;
    ch = (char)vga_history[history_row][x * 2];
    attr = vga_history[history_row][x * 2 + 1];
    fb_draw_char(ch, attr, (uint32_t)x, (uint32_t)screen_y);
}

static void fb_update_origin(void) {
    uint32_t width = gfx_width();
    uint32_t height = gfx_height();
    fb_origin_x = width > VGA_WIDTH * FB_CELL_WIDTH ? (width - VGA_WIDTH * FB_CELL_WIDTH) / 2 : 0;
    fb_origin_y = height > VGA_HEIGHT * FB_CELL_HEIGHT ? (height - VGA_HEIGHT * FB_CELL_HEIGHT) / 2 : 0;
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
        for (int col = 0; col < VGA_WIDTH * 2; col++) {
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
    if (used_rows <= VGA_HEIGHT) {
        return 0;
    }
    return used_rows - VGA_HEIGHT;
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

    int start_row = vga_cursor_row - (VGA_HEIGHT - 1) - vga_view_offset;
    if (start_row < 0) {
        start_row = 0;
    }

    for (int y = 0; y < VGA_HEIGHT; y++) {
        int source_row = start_row + y;
        for (int x = 0; x < VGA_WIDTH * 2; x++) {
            if (source_row <= vga_cursor_row && source_row < VGA_HISTORY_ROWS) {
                vga_buffer[y * VGA_WIDTH * 2 + x] = vga_history[source_row][x];
            } else if ((x & 1) == 0) {
                vga_buffer[y * VGA_WIDTH * 2 + x] = ' ';
            } else {
                vga_buffer[y * VGA_WIDTH * 2 + x] = vga_default_color;
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
        for (int i = 0; str[i] != '\0' && x + i < VGA_WIDTH; i++) {
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

    int index = (y * VGA_WIDTH + x) * 2;
    for (int i = 0; str[i] != '\0'; i++) {
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
    if (x >= VGA_WIDTH) x = VGA_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= VGA_HEIGHT) y = VGA_HEIGHT - 1;
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
    for (int x = vga_cursor_x; x < VGA_WIDTH; x++) {
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
    if (detected_cols) *detected_cols = VGA_WIDTH;
    if (detected_rows) *detected_rows = VGA_HEIGHT;
    if (max_cols) *max_cols = VGA_WIDTH;
    if (max_rows) *max_rows = VGA_HEIGHT;
}

int vga_set_display_mode(unsigned int cols, unsigned int rows) {
    if (cols < 40 || rows < 10 || cols > VGA_WIDTH || rows > VGA_HEIGHT) {
        return -1;
    }
    vga_mode_cols = cols;
    vga_mode_rows = rows;
    return 0;
}

void vga_auto_display_mode(void) {
    vga_mode_cols = VGA_WIDTH;
    vga_mode_rows = VGA_HEIGHT;
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
                vga_cursor_x = VGA_WIDTH - 1;
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
            if (vga_cursor_row >= VGA_HEIGHT) {
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
            if (vga_cursor_x >= VGA_WIDTH) {
                vga_cursor_x = 0;
                vga_cursor_row++;
                if (vga_cursor_row >= VGA_HISTORY_ROWS) {
                    vga_shift_history_up();
                    vga_cursor_row = VGA_HISTORY_ROWS - 1;
                }
                vga_clear_history_row(vga_cursor_row, vga_default_color);
                if (vga_cursor_row >= VGA_HEIGHT) {
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
