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
void gfx_blend_round_rect(int32_t x, int32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t rgb, uint8_t alpha);
void gfx_aa_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 uint32_t thickness, uint32_t rgb);
void gfx_aa_circle(int32_t cx, int32_t cy, uint32_t radius,
                   uint32_t thickness, uint32_t rgb);
int gfx_blit_rgb565_scaled(const uint16_t* pixels, uint32_t width, uint32_t height);
int gfx_blit_rgb565_rect(const uint16_t* pixels, uint32_t source_width,
                         uint32_t source_height, int32_t x, int32_t y,
                         uint32_t width, uint32_t height);
int gfx_blit_alpha_mask(const uint8_t* alpha, uint32_t source_width,
                        uint32_t source_height, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, uint32_t rgb);
void gfx_set_cursor(int32_t x, int32_t y, uint32_t buttons, uint8_t visible);
void gfx_present(void);

#endif
