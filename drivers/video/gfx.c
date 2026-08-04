/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <gfx.h>
#include <pci.h>
#include <vmm.h>
#include <stddef.h>
#include <stdint.h>

#define GFX_VIRT_BASE 0xFFFF800002000000ULL
#define GFX_MAX_WIDTH 1024
#define GFX_MAX_HEIGHT 768
#define GFX_BOCHS_PHYS_BASE 0xE0000000ULL
#define GFX_BOCHS_WIDTH 1024
#define GFX_BOCHS_HEIGHT 768
#define GFX_BOCHS_BPP 32
#define PCI_COMMAND_REG 0x04
#define PCI_COMMAND_MEMORY_SPACE 0x2
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA 0x01CF
#define VBE_DISPI_INDEX_ID 0x00
#define VBE_DISPI_INDEX_XRES 0x01
#define VBE_DISPI_INDEX_YRES 0x02
#define VBE_DISPI_INDEX_BPP 0x03
#define VBE_DISPI_INDEX_ENABLE 0x04
#define VBE_DISPI_INDEX_BANK 0x05
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x06
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x07
#define VBE_DISPI_ENABLED 0x01
#define VBE_DISPI_LFB_ENABLED 0x40
#define VBE_DISPI_DISABLED 0x00
#define GFX_CURSOR_WIDTH 16
#define GFX_CURSOR_HEIGHT 24
#define GFX_AA_COORD_SCALE 8
#define GFX_AA_CLIP_MARGIN 128

static uint8_t gfx_ready;
static uint8_t* gfx_fb;
static uint32_t gfx_fb_width;
static uint32_t gfx_fb_height;
static uint32_t gfx_fb_pitch;
static uint8_t gfx_fb_bpp;
static uint32_t gfx_backbuffer[GFX_MAX_WIDTH * GFX_MAX_HEIGHT];
static uint8_t gfx_dirty;
static uint32_t gfx_dirty_x0;
static uint32_t gfx_dirty_y0;
static uint32_t gfx_dirty_x1;
static uint32_t gfx_dirty_y1;
static int32_t gfx_cursor_x;
static int32_t gfx_cursor_y;
static uint32_t gfx_cursor_buttons;
static uint8_t gfx_cursor_visible;

static const uint16_t gfx_cursor_mask[GFX_CURSOR_HEIGHT] = {
    0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00,
    0xfe00, 0xff00, 0xff80, 0xffc0, 0xffe0, 0xfff0,
    0xfff8, 0xfffe, 0xffc0, 0xe7c0, 0xc7c0, 0x8380,
    0x0380, 0x01c0, 0x01c0, 0x00e0, 0x0060, 0x0020,
};

extern uint64_t p4_table[];
extern void serial_print(const char* s);

static void serial_print_hex32(uint32_t value) {
    const char* hex = "0123456789abcdef";
    char out[9];
    for (int i = 0; i < 8; i++) {
        out[i] = hex[(value >> ((7 - i) * 4)) & 0xF];
    }
    out[8] = '\0';
    serial_print(out);
}

static void outw_local(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t inw_local(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void bochs_write(uint16_t index, uint16_t value) {
    outw_local(VBE_DISPI_IOPORT_INDEX, index);
    outw_local(VBE_DISPI_IOPORT_DATA, value);
}

static uint16_t bochs_read(uint16_t index) {
    outw_local(VBE_DISPI_IOPORT_INDEX, index);
    return inw_local(VBE_DISPI_IOPORT_DATA);
}

static uint64_t gfx_find_vga_lfb_phys(void) {
    size_t count = pci_count();

    for (size_t i = 0; i < count; i++) {
        const struct pci_device* dev = pci_get(i);
        uint32_t bar0;
        uint32_t command;

        if (!dev || dev->class_code != 0x03) {
            continue;
        }

        bar0 = dev->bar[0];
        if ((bar0 & 1U) || bar0 == 0) {
            continue;
        }

        command = pci_config_read32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG);
        command |= PCI_COMMAND_MEMORY_SPACE;
        pci_config_write32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG, command);

        return (uint64_t)(bar0 & 0xFFFFFFF0U);
    }

    return GFX_BOCHS_PHYS_BASE;
}

static void gfx_mark_dirty(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t x1;
    uint32_t y1;

    if (!gfx_ready || w == 0 || h == 0 || x >= gfx_fb_width || y >= gfx_fb_height) return;

    x1 = x + w;
    y1 = y + h;
    if (x1 > gfx_fb_width || x1 < x) x1 = gfx_fb_width;
    if (y1 > gfx_fb_height || y1 < y) y1 = gfx_fb_height;

    if (!gfx_dirty) {
        gfx_dirty_x0 = x;
        gfx_dirty_y0 = y;
        gfx_dirty_x1 = x1;
        gfx_dirty_y1 = y1;
        gfx_dirty = 1;
        return;
    }

    if (x < gfx_dirty_x0) gfx_dirty_x0 = x;
    if (y < gfx_dirty_y0) gfx_dirty_y0 = y;
    if (x1 > gfx_dirty_x1) gfx_dirty_x1 = x1;
    if (y1 > gfx_dirty_y1) gfx_dirty_y1 = y1;
}

static void gfx_mark_cursor_dirty(int32_t x, int32_t y) {
    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = x + GFX_CURSOR_WIDTH;
    int32_t y1 = y + GFX_CURSOR_HEIGHT;

    if (!gfx_ready || x1 <= 0 || y1 <= 0 ||
        x0 >= (int32_t)gfx_fb_width || y0 >= (int32_t)gfx_fb_height) {
        return;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int32_t)gfx_fb_width) x1 = (int32_t)gfx_fb_width;
    if (y1 > (int32_t)gfx_fb_height) y1 = (int32_t)gfx_fb_height;
    gfx_mark_dirty((uint32_t)x0, (uint32_t)y0,
                   (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
}

int gfx_init_framebuffer(uint64_t phys, uint32_t width, uint32_t height,
                         uint32_t pitch, uint8_t bpp) {
    uint64_t size;
    uint64_t page_base;
    uint64_t page_offset;
    uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_WRITE_THROUGH | PAGE_CACHE_DISABLE;

    if (width == 0 || height == 0 || pitch == 0) return -1;
    if (width > GFX_MAX_WIDTH || height > GFX_MAX_HEIGHT) return -1;
    if (bpp != 32 && bpp != 24) return -1;

    size = (uint64_t)pitch * height;
    page_base = phys & ~0xFFFULL;
    page_offset = phys & 0xFFFULL;

    for (uint64_t off = 0; off < size + page_offset; off += 4096ULL) {
        vmm_map_page(p4_table, GFX_VIRT_BASE + off, page_base + off, flags);
    }

    gfx_fb = (uint8_t*)(GFX_VIRT_BASE + page_offset);
    gfx_fb_width = width;
    gfx_fb_height = height;
    gfx_fb_pitch = pitch;
    gfx_fb_bpp = bpp;
    gfx_ready = 1;
    gfx_cursor_x = 0;
    gfx_cursor_y = 0;
    gfx_cursor_buttons = 0;
    gfx_cursor_visible = 0;
    gfx_clear(0x05070a);
    gfx_present();
    return 0;
}

int gfx_init_bochs(void) {
    uint16_t id = bochs_read(VBE_DISPI_INDEX_ID);
    uint64_t lfb_phys;

    if (id < 0xB0C0 || id > 0xB0C5) {
        return -1;
    }

    bochs_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bochs_write(VBE_DISPI_INDEX_XRES, GFX_BOCHS_WIDTH);
    bochs_write(VBE_DISPI_INDEX_YRES, GFX_BOCHS_HEIGHT);
    bochs_write(VBE_DISPI_INDEX_BPP, GFX_BOCHS_BPP);
    bochs_write(VBE_DISPI_INDEX_BANK, 0);
    bochs_write(VBE_DISPI_INDEX_VIRT_WIDTH, GFX_BOCHS_WIDTH);
    bochs_write(VBE_DISPI_INDEX_VIRT_HEIGHT, GFX_BOCHS_HEIGHT);
    bochs_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    lfb_phys = gfx_find_vga_lfb_phys();
    if (gfx_init_framebuffer(lfb_phys, GFX_BOCHS_WIDTH, GFX_BOCHS_HEIGHT,
                             GFX_BOCHS_WIDTH * (GFX_BOCHS_BPP / 8), GFX_BOCHS_BPP) != 0) {
        return -1;
    }
    serial_print("Framebuffer: Bochs/QEMU LFB 1024x768x32 phys=0x");
    serial_print_hex32((uint32_t)lfb_phys);
    serial_print("\n");
    return 0;
}

int gfx_is_ready(void) {
    return gfx_ready != 0;
}

uint32_t gfx_width(void) {
    return gfx_fb_width;
}

uint32_t gfx_height(void) {
    return gfx_fb_height;
}

uint8_t gfx_bpp(void) {
    return gfx_fb_bpp;
}

void gfx_clear(uint32_t rgb) {
    if (!gfx_ready) return;
    for (uint32_t i = 0; i < gfx_fb_width * gfx_fb_height; i++) {
        gfx_backbuffer[i] = rgb;
    }
    gfx_mark_dirty(0, 0, gfx_fb_width, gfx_fb_height);
}

void gfx_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (!gfx_ready || x >= gfx_fb_width || y >= gfx_fb_height) return;
    gfx_backbuffer[y * gfx_fb_width + x] = rgb;
    gfx_mark_dirty(x, y, 1, 1);
}

void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb) {
    if (!gfx_ready || w == 0 || h == 0 || x >= gfx_fb_width || y >= gfx_fb_height) return;
    if (x + w > gfx_fb_width || x + w < x) w = gfx_fb_width - x;
    if (y + h > gfx_fb_height || y + h < y) h = gfx_fb_height - y;

    for (uint32_t yy = 0; yy < h; yy++) {
        uint32_t* row = &gfx_backbuffer[(y + yy) * gfx_fb_width + x];
        for (uint32_t xx = 0; xx < w; xx++) {
            row[xx] = rgb;
        }
    }
    gfx_mark_dirty(x, y, w, h);
}

void gfx_blend_round_rect(int32_t x, int32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t rgb, uint8_t alpha) {
    int64_t right;
    int64_t bottom;
    int32_t clip_x0;
    int32_t clip_y0;
    int32_t clip_x1;
    int32_t clip_y1;
    uint32_t corner_radius;
    int64_t corner_radius_subpixel;
    int64_t corner_radius_sq;
    uint32_t over_r;
    uint32_t over_g;
    uint32_t over_b;

    if (!gfx_ready || w == 0 || h == 0 || alpha == 0) return;

    right = (int64_t)x + w;
    bottom = (int64_t)y + h;
    if (right <= 0 || bottom <= 0 || x >= (int32_t)gfx_fb_width ||
        y >= (int32_t)gfx_fb_height) {
        return;
    }

    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = right > gfx_fb_width ? (int32_t)gfx_fb_width : (int32_t)right;
    clip_y1 = bottom > gfx_fb_height ? (int32_t)gfx_fb_height : (int32_t)bottom;
    if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1) return;

    corner_radius = radius;
    if (corner_radius > w / 2) corner_radius = w / 2;
    if (corner_radius > h / 2) corner_radius = h / 2;
    corner_radius_subpixel = (int64_t)corner_radius * GFX_AA_COORD_SCALE;
    corner_radius_sq = corner_radius_subpixel * corner_radius_subpixel;

    over_r = (rgb >> 16) & 0xffU;
    over_g = (rgb >> 8) & 0xffU;
    over_b = rgb & 0xffU;

    for (int32_t yy = clip_y0; yy < clip_y1; yy++) {
        uint32_t local_y = (uint32_t)((int64_t)yy - (int64_t)y);
        uint32_t* row = &gfx_backbuffer[(uint32_t)yy * gfx_fb_width];

        for (int32_t xx = clip_x0; xx < clip_x1; xx++) {
            uint32_t local_x = (uint32_t)((int64_t)xx - (int64_t)x);
            uint32_t covered = 16;
            int left_corner = local_x < corner_radius;
            int right_corner = local_x >= w - corner_radius;
            int top_corner = local_y < corner_radius;
            int bottom_corner = local_y >= h - corner_radius;

            if (corner_radius != 0 &&
                (left_corner || right_corner) &&
                (top_corner || bottom_corner)) {
                int64_t center_x = (int64_t)(left_corner
                    ? corner_radius
                    : w - corner_radius) * GFX_AA_COORD_SCALE;
                int64_t center_y = (int64_t)(top_corner
                    ? corner_radius
                    : h - corner_radius) * GFX_AA_COORD_SCALE;
                covered = 0;

                for (int32_t sy = 1; sy < GFX_AA_COORD_SCALE; sy += 2) {
                    for (int32_t sx = 1; sx < GFX_AA_COORD_SCALE; sx += 2) {
                        int64_t sample_x = (int64_t)local_x * GFX_AA_COORD_SCALE + sx;
                        int64_t sample_y = (int64_t)local_y * GFX_AA_COORD_SCALE + sy;
                        int64_t dx = sample_x - center_x;
                        int64_t dy = sample_y - center_y;
                        if (dx * dx + dy * dy <= corner_radius_sq) {
                            covered++;
                        }
                    }
                }
            }

            if (covered != 0) {
                uint32_t effective_alpha =
                    ((uint32_t)alpha * covered + 8U) / 16U;
                uint32_t inverse_alpha = 255U - effective_alpha;
                uint32_t under = row[xx];
                if (effective_alpha == 255) {
                    row[xx] = rgb;
                } else {
                    uint32_t under_r = (under >> 16) & 0xffU;
                    uint32_t under_g = (under >> 8) & 0xffU;
                    uint32_t under_b = under & 0xffU;
                    uint32_t out_r =
                        (under_r * inverse_alpha + over_r * effective_alpha) / 255U;
                    uint32_t out_g =
                        (under_g * inverse_alpha + over_g * effective_alpha) / 255U;
                    uint32_t out_b =
                        (under_b * inverse_alpha + over_b * effective_alpha) / 255U;
                    row[xx] = (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }

    gfx_mark_dirty((uint32_t)clip_x0, (uint32_t)clip_y0,
                   (uint32_t)(clip_x1 - clip_x0), (uint32_t)(clip_y1 - clip_y0));
}

static void gfx_blend_backbuffer_pixel(int32_t x, int32_t y, uint32_t rgb, uint8_t alpha) {
    uint32_t* pixel;
    uint32_t under;
    uint32_t inverse_alpha;
    uint32_t out_r;
    uint32_t out_g;
    uint32_t out_b;

    if (alpha == 0 || x < 0 || y < 0 ||
        x >= (int32_t)gfx_fb_width || y >= (int32_t)gfx_fb_height) {
        return;
    }

    pixel = &gfx_backbuffer[(uint32_t)y * gfx_fb_width + (uint32_t)x];
    if (alpha == 255) {
        *pixel = rgb;
        return;
    }

    under = *pixel;
    inverse_alpha = 255U - alpha;
    out_r = ((((under >> 16) & 0xffU) * inverse_alpha) +
             (((rgb >> 16) & 0xffU) * alpha)) / 255U;
    out_g = ((((under >> 8) & 0xffU) * inverse_alpha) +
             (((rgb >> 8) & 0xffU) * alpha)) / 255U;
    out_b = (((under & 0xffU) * inverse_alpha) +
             ((rgb & 0xffU) * alpha)) / 255U;
    *pixel = (out_r << 16) | (out_g << 8) | out_b;
}

static int gfx_aa_coordinate_valid(int32_t x, int32_t y) {
    return x >= -GFX_AA_CLIP_MARGIN && y >= -GFX_AA_CLIP_MARGIN &&
           x <= (int32_t)gfx_fb_width + GFX_AA_CLIP_MARGIN &&
           y <= (int32_t)gfx_fb_height + GFX_AA_CLIP_MARGIN;
}

void gfx_aa_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                 uint32_t thickness, uint32_t rgb) {
    int32_t pad;
    int32_t clip_x0;
    int32_t clip_y0;
    int32_t clip_x1;
    int32_t clip_y1;
    int64_t ax;
    int64_t ay;
    int64_t bx;
    int64_t by;
    int64_t vx;
    int64_t vy;
    int64_t length_sq;
    int64_t radius;
    int64_t radius_sq;

    if (!gfx_ready || thickness == 0) return;
    if (!gfx_aa_coordinate_valid(x0, y0) || !gfx_aa_coordinate_valid(x1, y1)) return;
    if (thickness > 16) thickness = 16;

    pad = (int32_t)((thickness + 1U) / 2U) + 2;
    clip_x0 = (x0 < x1 ? x0 : x1) - pad;
    clip_y0 = (y0 < y1 ? y0 : y1) - pad;
    clip_x1 = (x0 > x1 ? x0 : x1) + pad + 1;
    clip_y1 = (y0 > y1 ? y0 : y1) + pad + 1;
    if (clip_x0 < 0) clip_x0 = 0;
    if (clip_y0 < 0) clip_y0 = 0;
    if (clip_x1 > (int32_t)gfx_fb_width) clip_x1 = (int32_t)gfx_fb_width;
    if (clip_y1 > (int32_t)gfx_fb_height) clip_y1 = (int32_t)gfx_fb_height;
    if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1) return;

    ax = (int64_t)x0 * GFX_AA_COORD_SCALE + GFX_AA_COORD_SCALE / 2;
    ay = (int64_t)y0 * GFX_AA_COORD_SCALE + GFX_AA_COORD_SCALE / 2;
    bx = (int64_t)x1 * GFX_AA_COORD_SCALE + GFX_AA_COORD_SCALE / 2;
    by = (int64_t)y1 * GFX_AA_COORD_SCALE + GFX_AA_COORD_SCALE / 2;
    vx = bx - ax;
    vy = by - ay;
    length_sq = vx * vx + vy * vy;
    radius = (int64_t)thickness * GFX_AA_COORD_SCALE / 2;
    radius_sq = radius * radius;

    for (int32_t y = clip_y0; y < clip_y1; y++) {
        for (int32_t x = clip_x0; x < clip_x1; x++) {
            uint32_t covered = 0;

            for (int32_t sy = 1; sy < GFX_AA_COORD_SCALE; sy += 2) {
                for (int32_t sx = 1; sx < GFX_AA_COORD_SCALE; sx += 2) {
                    int64_t px = (int64_t)x * GFX_AA_COORD_SCALE + sx;
                    int64_t py = (int64_t)y * GFX_AA_COORD_SCALE + sy;
                    int64_t wx = px - ax;
                    int64_t wy = py - ay;
                    int inside = 0;

                    if (length_sq == 0) {
                        inside = wx * wx + wy * wy <= radius_sq;
                    } else {
                        int64_t projection = wx * vx + wy * vy;
                        if (projection <= 0) {
                            inside = wx * wx + wy * wy <= radius_sq;
                        } else if (projection >= length_sq) {
                            int64_t dx = px - bx;
                            int64_t dy = py - by;
                            inside = dx * dx + dy * dy <= radius_sq;
                        } else {
                            int64_t cross = wx * vy - wy * vx;
                            inside = cross * cross <= radius_sq * length_sq;
                        }
                    }
                    if (inside) covered++;
                }
            }

            if (covered != 0) {
                gfx_blend_backbuffer_pixel(x, y, rgb,
                    (uint8_t)((covered * 255U + 8U) / 16U));
            }
        }
    }

    gfx_mark_dirty((uint32_t)clip_x0, (uint32_t)clip_y0,
                   (uint32_t)(clip_x1 - clip_x0), (uint32_t)(clip_y1 - clip_y0));
}

void gfx_aa_circle(int32_t cx, int32_t cy, uint32_t radius,
                   uint32_t thickness, uint32_t rgb) {
    int32_t pad;
    int32_t clip_x0;
    int32_t clip_y0;
    int32_t clip_x1;
    int32_t clip_y1;
    int64_t center_x;
    int64_t center_y;
    int64_t outer_radius;
    int64_t inner_radius;
    int64_t outer_sq;
    int64_t inner_sq;
    int filled = thickness == 0;

    if (!gfx_ready || radius == 0 || !gfx_aa_coordinate_valid(cx, cy)) return;
    if (radius > 128) radius = 128;
    if (thickness > 16) thickness = 16;

    outer_radius = (int64_t)radius * GFX_AA_COORD_SCALE;
    inner_radius = 0;
    if (!filled) {
        outer_radius += (int64_t)thickness * GFX_AA_COORD_SCALE / 2;
        inner_radius = (int64_t)radius * GFX_AA_COORD_SCALE -
                       (int64_t)thickness * GFX_AA_COORD_SCALE / 2;
        if (inner_radius < 0) inner_radius = 0;
    }
    outer_sq = outer_radius * outer_radius;
    inner_sq = inner_radius * inner_radius;
    pad = (int32_t)((outer_radius + GFX_AA_COORD_SCALE - 1) / GFX_AA_COORD_SCALE) + 1;
    clip_x0 = cx - pad;
    clip_y0 = cy - pad;
    clip_x1 = cx + pad + 1;
    clip_y1 = cy + pad + 1;
    if (clip_x0 < 0) clip_x0 = 0;
    if (clip_y0 < 0) clip_y0 = 0;
    if (clip_x1 > (int32_t)gfx_fb_width) clip_x1 = (int32_t)gfx_fb_width;
    if (clip_y1 > (int32_t)gfx_fb_height) clip_y1 = (int32_t)gfx_fb_height;
    if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1) return;

    center_x = (int64_t)cx * GFX_AA_COORD_SCALE + GFX_AA_COORD_SCALE / 2;
    center_y = (int64_t)cy * GFX_AA_COORD_SCALE + GFX_AA_COORD_SCALE / 2;
    for (int32_t y = clip_y0; y < clip_y1; y++) {
        for (int32_t x = clip_x0; x < clip_x1; x++) {
            uint32_t covered = 0;

            for (int32_t sy = 1; sy < GFX_AA_COORD_SCALE; sy += 2) {
                for (int32_t sx = 1; sx < GFX_AA_COORD_SCALE; sx += 2) {
                    int64_t dx = (int64_t)x * GFX_AA_COORD_SCALE + sx - center_x;
                    int64_t dy = (int64_t)y * GFX_AA_COORD_SCALE + sy - center_y;
                    int64_t distance_sq = dx * dx + dy * dy;
                    if (distance_sq <= outer_sq &&
                        (filled || distance_sq >= inner_sq)) {
                        covered++;
                    }
                }
            }

            if (covered != 0) {
                gfx_blend_backbuffer_pixel(x, y, rgb,
                    (uint8_t)((covered * 255U + 8U) / 16U));
            }
        }
    }

    gfx_mark_dirty((uint32_t)clip_x0, (uint32_t)clip_y0,
                   (uint32_t)(clip_x1 - clip_x0), (uint32_t)(clip_y1 - clip_y0));
}

void gfx_set_cursor(int32_t x, int32_t y, uint32_t buttons, uint8_t visible) {
    int32_t max_x;
    int32_t max_y;

    if (!gfx_ready) return;

    max_x = gfx_fb_width > 0 ? (int32_t)gfx_fb_width - 1 : 0;
    max_y = gfx_fb_height > 0 ? (int32_t)gfx_fb_height - 1 : 0;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;
    visible = visible ? 1U : 0U;

    if (gfx_cursor_x == x && gfx_cursor_y == y &&
        gfx_cursor_buttons == buttons && gfx_cursor_visible == visible) {
        return;
    }

    if (gfx_cursor_visible) {
        gfx_mark_cursor_dirty(gfx_cursor_x, gfx_cursor_y);
    }
    gfx_cursor_x = x;
    gfx_cursor_y = y;
    gfx_cursor_buttons = buttons;
    gfx_cursor_visible = visible;
    if (gfx_cursor_visible) {
        gfx_mark_cursor_dirty(gfx_cursor_x, gfx_cursor_y);
    }
}

int gfx_blit_rgb565_scaled(const uint16_t* pixels, uint32_t width, uint32_t height) {
    if (!gfx_ready || !pixels || width == 0 || height == 0) return -1;
    if (width > 2048 || height > 2048) return -1;

    for (uint32_t y = 0; y < gfx_fb_height; y++) {
        uint32_t source_y = (uint32_t)(((uint64_t)y * height) / gfx_fb_height);
        uint32_t* destination = &gfx_backbuffer[y * gfx_fb_width];
        const uint16_t* source = &pixels[source_y * width];
        for (uint32_t x = 0; x < gfx_fb_width; x++) {
            uint32_t source_x = (uint32_t)(((uint64_t)x * width) / gfx_fb_width);
            uint16_t value = source[source_x];
            uint32_t red = (uint32_t)((value >> 11) & 0x1FU);
            uint32_t green = (uint32_t)((value >> 5) & 0x3FU);
            uint32_t blue = (uint32_t)(value & 0x1FU);
            red = (red << 3) | (red >> 2);
            green = (green << 2) | (green >> 4);
            blue = (blue << 3) | (blue >> 2);
            destination[x] = (red << 16) | (green << 8) | blue;
        }
    }
    gfx_mark_dirty(0, 0, gfx_fb_width, gfx_fb_height);
    return 0;
}

int gfx_blit_rgb565_rect(const uint16_t* pixels, uint32_t source_width,
                         uint32_t source_height, int32_t x, int32_t y,
                         uint32_t width, uint32_t height) {
    int64_t right;
    int64_t bottom;
    int32_t clip_x0;
    int32_t clip_y0;
    int32_t clip_x1;
    int32_t clip_y1;

    if (!gfx_ready || !pixels || source_width == 0 || source_height == 0 ||
        width == 0 || height == 0) {
        return -1;
    }
    if (source_width > 2048 || source_height > 2048 ||
        width > 4096 || height > 4096) {
        return -1;
    }

    right = (int64_t)x + width;
    bottom = (int64_t)y + height;
    if (right <= 0 || bottom <= 0 || x >= (int32_t)gfx_fb_width ||
        y >= (int32_t)gfx_fb_height) {
        return 0;
    }
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = right > gfx_fb_width ? (int32_t)gfx_fb_width : (int32_t)right;
    clip_y1 = bottom > gfx_fb_height ? (int32_t)gfx_fb_height : (int32_t)bottom;

    for (int32_t yy = clip_y0; yy < clip_y1; yy++) {
        uint32_t local_y = (uint32_t)((int64_t)yy - y);
        uint32_t source_y =
            (uint32_t)(((uint64_t)local_y * source_height) / height);
        uint32_t* destination = &gfx_backbuffer[(uint32_t)yy * gfx_fb_width];
        const uint16_t* source = &pixels[source_y * source_width];

        for (int32_t xx = clip_x0; xx < clip_x1; xx++) {
            uint32_t local_x = (uint32_t)((int64_t)xx - x);
            uint32_t source_x =
                (uint32_t)(((uint64_t)local_x * source_width) / width);
            uint16_t value = source[source_x];
            uint32_t red = (uint32_t)((value >> 11) & 0x1fU);
            uint32_t green = (uint32_t)((value >> 5) & 0x3fU);
            uint32_t blue = (uint32_t)(value & 0x1fU);
            red = (red << 3) | (red >> 2);
            green = (green << 2) | (green >> 4);
            blue = (blue << 3) | (blue >> 2);
            destination[xx] = (red << 16) | (green << 8) | blue;
        }
    }

    gfx_mark_dirty((uint32_t)clip_x0, (uint32_t)clip_y0,
                   (uint32_t)(clip_x1 - clip_x0),
                   (uint32_t)(clip_y1 - clip_y0));
    return 0;
}

int gfx_blit_alpha_mask(const uint8_t* alpha, uint32_t source_width,
                        uint32_t source_height, int32_t x, int32_t y,
                        uint32_t width, uint32_t height, uint32_t rgb) {
    int64_t right;
    int64_t bottom;
    int32_t clip_x0;
    int32_t clip_y0;
    int32_t clip_x1;
    int32_t clip_y1;
    uint32_t step_x;
    uint32_t step_y;

    if (!gfx_ready || !alpha || source_width == 0 || source_height == 0 ||
        width == 0 || height == 0) {
        return -1;
    }
    if (source_width > 64 || source_height > 64 ||
        width > 256 || height > 256) {
        return -1;
    }

    right = (int64_t)x + width;
    bottom = (int64_t)y + height;
    if (right <= 0 || bottom <= 0 || x >= (int32_t)gfx_fb_width ||
        y >= (int32_t)gfx_fb_height) {
        return 0;
    }
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = right > gfx_fb_width ? (int32_t)gfx_fb_width : (int32_t)right;
    clip_y1 = bottom > gfx_fb_height ? (int32_t)gfx_fb_height : (int32_t)bottom;
    step_x = width > 1 && source_width > 1
                 ? (uint32_t)((((uint64_t)source_width - 1U) << 16) /
                              (width - 1U))
                 : 0;
    step_y = height > 1 && source_height > 1
                 ? (uint32_t)((((uint64_t)source_height - 1U) << 16) /
                              (height - 1U))
                 : 0;

    for (int32_t yy = clip_y0; yy < clip_y1; yy++) {
        uint32_t local_y = (uint32_t)((int64_t)yy - y);
        uint32_t source_y_fixed = local_y * step_y;
        uint32_t source_y0 = source_y_fixed >> 16;
        uint32_t source_y1 =
            source_y0 + 1U < source_height ? source_y0 + 1U : source_y0;
        uint32_t fraction_y = source_y_fixed & 0xffffU;

        for (int32_t xx = clip_x0; xx < clip_x1; xx++) {
            uint32_t local_x = (uint32_t)((int64_t)xx - x);
            uint32_t source_x_fixed = local_x * step_x;
            uint32_t source_x0 = source_x_fixed >> 16;
            uint32_t source_x1 =
                source_x0 + 1U < source_width ? source_x0 + 1U : source_x0;
            uint32_t fraction_x = source_x_fixed & 0xffffU;
            uint32_t top =
                ((uint32_t)alpha[source_y0 * source_width + source_x0] *
                     (0x10000U - fraction_x) +
                 (uint32_t)alpha[source_y0 * source_width + source_x1] *
                     fraction_x) >>
                16;
            uint32_t bottom_alpha =
                ((uint32_t)alpha[source_y1 * source_width + source_x0] *
                     (0x10000U - fraction_x) +
                 (uint32_t)alpha[source_y1 * source_width + source_x1] *
                     fraction_x) >>
                16;
            uint32_t coverage =
                (top * (0x10000U - fraction_y) +
                 bottom_alpha * fraction_y) >>
                16;

            if (coverage != 0) {
                gfx_blend_backbuffer_pixel(xx, yy, rgb,
                                           (uint8_t)coverage);
            }
        }
    }

    gfx_mark_dirty((uint32_t)clip_x0, (uint32_t)clip_y0,
                   (uint32_t)(clip_x1 - clip_x0),
                   (uint32_t)(clip_y1 - clip_y0));
    return 0;
}

static int gfx_cursor_has_pixel(int32_t x, int32_t y) {
    if (x < 0 || x >= GFX_CURSOR_WIDTH || y < 0 || y >= GFX_CURSOR_HEIGHT) {
        return 0;
    }
    return (gfx_cursor_mask[y] & (uint16_t)(0x8000U >> x)) != 0;
}

static void gfx_framebuffer_pixel(int32_t x, int32_t y, uint32_t rgb) {
    uint8_t* destination;

    if (x < 0 || y < 0 || x >= (int32_t)gfx_fb_width || y >= (int32_t)gfx_fb_height) {
        return;
    }
    destination = gfx_fb + (uint64_t)(uint32_t)y * gfx_fb_pitch +
                  (uint64_t)(uint32_t)x * (gfx_fb_bpp / 8);
    if (gfx_fb_bpp == 32) {
        *(uint32_t*)destination = rgb;
    } else {
        destination[0] = (uint8_t)(rgb & 0xffU);
        destination[1] = (uint8_t)((rgb >> 8) & 0xffU);
        destination[2] = (uint8_t)((rgb >> 16) & 0xffU);
    }
}

static void gfx_draw_cursor(void) {
    uint32_t fill = (gfx_cursor_buttons & 1U) ? 0x46a86fU : 0xffffffU;

    if (!gfx_cursor_visible) return;

    for (int32_t y = 0; y < GFX_CURSOR_HEIGHT; y++) {
        for (int32_t x = 0; x < GFX_CURSOR_WIDTH; x++) {
            int border;
            if (!gfx_cursor_has_pixel(x, y)) continue;

            border = !gfx_cursor_has_pixel(x - 1, y) ||
                     !gfx_cursor_has_pixel(x + 1, y) ||
                     !gfx_cursor_has_pixel(x, y - 1) ||
                     !gfx_cursor_has_pixel(x, y + 1);
            gfx_framebuffer_pixel(gfx_cursor_x + x, gfx_cursor_y + y,
                                  border ? 0x101610U : fill);
        }
    }
}

void gfx_present(void) {
    if (!gfx_ready || !gfx_dirty) return;

    for (uint32_t y = gfx_dirty_y0; y < gfx_dirty_y1; y++) {
        uint32_t* src = &gfx_backbuffer[y * gfx_fb_width + gfx_dirty_x0];
        uint8_t* dst = gfx_fb + (uint64_t)y * gfx_fb_pitch + (uint64_t)gfx_dirty_x0 * (gfx_fb_bpp / 8);
        uint32_t count = gfx_dirty_x1 - gfx_dirty_x0;

        if (gfx_fb_bpp == 32) {
            uint32_t* dst32 = (uint32_t*)dst;
            for (uint32_t x = 0; x < count; x++) {
                dst32[x] = src[x];
            }
        } else if (gfx_fb_bpp == 24) {
            for (uint32_t x = 0; x < count; x++) {
                uint32_t rgb = src[x];
                dst[x * 3] = (uint8_t)(rgb & 0xFF);
                dst[x * 3 + 1] = (uint8_t)((rgb >> 8) & 0xFF);
                dst[x * 3 + 2] = (uint8_t)((rgb >> 16) & 0xFF);
            }
        }
    }

    gfx_draw_cursor();
    gfx_dirty = 0;
}
