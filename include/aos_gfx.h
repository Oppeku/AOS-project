/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_GFX_H
#define AOS_GFX_H

#include <stdint.h>

struct aos_gfx_info {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t ready;
};

int64_t aos_gfx_info(struct aos_gfx_info* out);
int64_t aos_gfx_clear(uint32_t rgb);
int64_t aos_gfx_pixel(uint32_t x, uint32_t y, uint32_t rgb);
int64_t aos_gfx_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
int64_t aos_gfx_present(void);

#endif
