/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "vga.h"

unsigned char* vga_buffer = (unsigned char*)0xB8000;
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_HISTORY_ROWS 256

static int vga_cursor_x = 0;
static int vga_cursor_row = 0;
static unsigned char vga_default_color = 0x07;
static int vga_view_offset = 0;
static unsigned char vga_history[VGA_HISTORY_ROWS][VGA_WIDTH * 2];

static void vga_clear_history_row(int row, unsigned char color) {
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_history[row][x * 2] = ' ';
        vga_history[row][x * 2 + 1] = color;
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
    vga_render();
}

void vga_print(const char* str, unsigned char color, int x, int y) {
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

void vga_write(const char* str, unsigned char color) {
    vga_ensure_bottom_view();
    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
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
            vga_history[vga_cursor_row][index + 1] = color;
            vga_render();
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
        } else {
            int index = vga_cursor_x * 2;
            vga_history[vga_cursor_row][index] = c;
            vga_history[vga_cursor_row][index + 1] = color;
            vga_cursor_x++;
            if (vga_cursor_x >= VGA_WIDTH) {
                vga_cursor_x = 0;
                vga_cursor_row++;
                if (vga_cursor_row >= VGA_HISTORY_ROWS) {
                    vga_shift_history_up();
                    vga_cursor_row = VGA_HISTORY_ROWS - 1;
                }
                vga_clear_history_row(vga_cursor_row, vga_default_color);
            }
        }
    }
    vga_render();
}
