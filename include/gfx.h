/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>

int gfx_init_bochs(void);
int gfx_init_framebuffer(uint64_t phys, uint32_t width, uint32_t height,
                         uint32_t pitch, uint8_t bpp);
int gfx_is_ready(void);
uint32_t gfx_width(void);
uint32_t gfx_height(void);
uint8_t gfx_bpp(void);
void gfx_clear(uint32_t rgb);
void gfx_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
void gfx_present(void);

#endif
