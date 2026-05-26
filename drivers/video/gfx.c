/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <gfx.h>
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

extern uint64_t p4_table[];
extern void serial_print(const char* s);

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
    gfx_clear(0x05070a);
    return 0;
}

int gfx_init_bochs(void) {
    uint16_t id = bochs_read(VBE_DISPI_INDEX_ID);

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

    if (gfx_init_framebuffer(GFX_BOCHS_PHYS_BASE, GFX_BOCHS_WIDTH, GFX_BOCHS_HEIGHT,
                             GFX_BOCHS_WIDTH * (GFX_BOCHS_BPP / 8), GFX_BOCHS_BPP) != 0) {
        return -1;
    }
    serial_print("Framebuffer: Bochs/QEMU LFB 1024x768x32\n");
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
    gfx_present();
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

    gfx_dirty = 0;
}
