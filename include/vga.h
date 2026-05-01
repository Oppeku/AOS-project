/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef VGA_H
#define VGA_H

void vga_clear(unsigned char color);
void vga_print(const char* str, unsigned char color, int x, int y);
void vga_scroll();
void vga_scrollback_up(void);
void vga_scrollback_down(void);
void vga_write(const char* str, unsigned char color);

#endif
