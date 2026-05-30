/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef VGA_H
#define VGA_H

#include <stdint.h>

void vga_init_framebuffer(uint64_t mb_info);
void vga_init_tty(void);
void vga_clear(unsigned char color);
void vga_print(const char* str, unsigned char color, int x, int y);
void vga_scroll();
void vga_scrollback_up(void);
void vga_scrollback_down(void);
void vga_write(const char* str, unsigned char color);
void vga_get_display_mode(unsigned int* cols, unsigned int* rows,
                          unsigned int* detected_cols, unsigned int* detected_rows,
                          unsigned int* max_cols, unsigned int* max_rows);
int vga_set_display_mode(unsigned int cols, unsigned int rows);
void vga_auto_display_mode(void);

#endif
