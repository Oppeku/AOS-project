/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <blkdev.h>

static struct blkdev g_blkdevs[BLKDEV_MAX];

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
}

static void copy_string(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        src = "";
    }
    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int string_equals(const char* a, const char* b) {
    size_t i = 0;
    while (a && b && a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a && b && a[i] == '\0' && b[i] == '\0';
}

void blkdev_init(void) {
    local_memset(g_blkdevs, 0, sizeof(g_blkdevs));
}

uint32_t blkdev_register_memory(const char* name, uint32_t start, uint32_t end, uint32_t block_size, uint8_t read_only) {
    if (end <= start) {
        return BLKDEV_INVALID_ID;
    }
    if (block_size == 0) {
        block_size = 512;
    }

    for (uint32_t i = 0; i < BLKDEV_MAX; i++) {
        if (!g_blkdevs[i].in_use) {
            local_memset(&g_blkdevs[i], 0, sizeof(g_blkdevs[i]));
            g_blkdevs[i].in_use = 1;
            g_blkdevs[i].read_only = read_only ? 1 : 0;
            g_blkdevs[i].id = i;
            g_blkdevs[i].block_size = block_size;
            g_blkdevs[i].size = (uint64_t)end - start;
            g_blkdevs[i].data = (uint8_t*)(uintptr_t)start;
            copy_string(g_blkdevs[i].name, sizeof(g_blkdevs[i].name), name);
            return i;
        }
    }

    return BLKDEV_INVALID_ID;
}

uint32_t blkdev_register_ops(const char* name, uint64_t size, uint32_t block_size, uint8_t read_only, void* ctx, int (*read_blocks)(void*, uint64_t, uint32_t, void*), int (*write_blocks)(void*, uint64_t, uint32_t, const void*)) {
    if (size == 0 || !read_blocks) {
        return BLKDEV_INVALID_ID;
    }
    if (block_size == 0) {
        block_size = 512;
    }

    for (uint32_t i = 0; i < BLKDEV_MAX; i++) {
        if (!g_blkdevs[i].in_use) {
            local_memset(&g_blkdevs[i], 0, sizeof(g_blkdevs[i]));
            g_blkdevs[i].in_use = 1;
            g_blkdevs[i].read_only = read_only ? 1 : 0;
            g_blkdevs[i].has_ops = 1;
            g_blkdevs[i].id = i;
            g_blkdevs[i].block_size = block_size;
            g_blkdevs[i].size = size;
            g_blkdevs[i].ctx = ctx;
            g_blkdevs[i].read_blocks = read_blocks;
            g_blkdevs[i].write_blocks = write_blocks;
            copy_string(g_blkdevs[i].name, sizeof(g_blkdevs[i].name), name);
            return i;
        }
    }

    return BLKDEV_INVALID_ID;
}

const struct blkdev* blkdev_get(uint32_t id) {
    if (id >= BLKDEV_MAX || !g_blkdevs[id].in_use) {
        return NULL;
    }
    return &g_blkdevs[id];
}

const struct blkdev* blkdev_get_index(size_t index) {
    size_t seen = 0;
    for (uint32_t i = 0; i < BLKDEV_MAX; i++) {
        if (!g_blkdevs[i].in_use) {
            continue;
        }
        if (seen == index) {
            return &g_blkdevs[i];
        }
        seen++;
    }
    return NULL;
}

const struct blkdev* blkdev_find(const char* name) {
    for (uint32_t i = 0; i < BLKDEV_MAX; i++) {
        if (g_blkdevs[i].in_use && string_equals(g_blkdevs[i].name, name)) {
            return &g_blkdevs[i];
        }
    }
    return NULL;
}

int blkdev_read(uint32_t id, uint64_t offset, void* buffer, uint64_t len) {
    const struct blkdev* dev = blkdev_get(id);
    uint8_t sector[512];
    uint8_t* out = (uint8_t*)buffer;
    if (!dev || !buffer) {
        return -1;
    }
    if (offset > dev->size || len > dev->size - offset) {
        return -1;
    }
    if (dev->has_ops) {
        while (len > 0) {
            uint64_t lba = offset / dev->block_size;
            uint32_t sector_offset = (uint32_t)(offset % dev->block_size);
            uint64_t chunk = dev->block_size - sector_offset;
            if (chunk > len) chunk = len;
            if (dev->block_size > sizeof(sector) || dev->read_blocks(dev->ctx, lba, 1, sector) != 0) {
                return -1;
            }
            local_memcpy(out, sector + sector_offset, (size_t)chunk);
            out += chunk;
            offset += chunk;
            len -= chunk;
        }
        return 0;
    }
    local_memcpy(buffer, dev->data + offset, (size_t)len);
    return 0;
}

int blkdev_write(uint32_t id, uint64_t offset, const void* buffer, uint64_t len) {
    const struct blkdev* dev = blkdev_get(id);
    uint8_t sector[512];
    const uint8_t* in = (const uint8_t*)buffer;
    if (!dev || !buffer || dev->read_only) {
        return -1;
    }
    if (offset > dev->size || len > dev->size - offset) {
        return -1;
    }
    if (dev->has_ops) {
        if (!dev->write_blocks) {
            return -1;
        }
        while (len > 0) {
            uint64_t lba = offset / dev->block_size;
            uint32_t sector_offset = (uint32_t)(offset % dev->block_size);
            uint64_t chunk = dev->block_size - sector_offset;
            if (chunk > len) chunk = len;
            if (dev->block_size > sizeof(sector) || dev->read_blocks(dev->ctx, lba, 1, sector) != 0) {
                return -1;
            }
            local_memcpy(sector + sector_offset, in, (size_t)chunk);
            if (dev->write_blocks(dev->ctx, lba, 1, sector) != 0) {
                return -1;
            }
            in += chunk;
            offset += chunk;
            len -= chunk;
        }
        return 0;
    }
    local_memcpy(dev->data + offset, buffer, (size_t)len);
    return 0;
}

int blkdev_read_blocks(uint32_t id, uint64_t lba, uint32_t block_count, void* buffer) {
    const struct blkdev* dev = blkdev_get(id);
    uint64_t offset;
    uint64_t len;
    if (!dev) {
        return -1;
    }
    if (dev->has_ops) {
        if (lba > dev->size / dev->block_size || block_count > (dev->size / dev->block_size) - lba) {
            return -1;
        }
        return dev->read_blocks(dev->ctx, lba, block_count, buffer);
    }
    offset = lba * dev->block_size;
    len = (uint64_t)block_count * dev->block_size;
    return blkdev_read(id, offset, buffer, len);
}

int blkdev_write_blocks(uint32_t id, uint64_t lba, uint32_t block_count, const void* buffer) {
    const struct blkdev* dev = blkdev_get(id);
    uint64_t offset;
    uint64_t len;
    if (!dev) {
        return -1;
    }
    if (dev->has_ops) {
        if (dev->read_only || !dev->write_blocks) {
            return -1;
        }
        if (lba > dev->size / dev->block_size || block_count > (dev->size / dev->block_size) - lba) {
            return -1;
        }
        return dev->write_blocks(dev->ctx, lba, block_count, buffer);
    }
    offset = lba * dev->block_size;
    len = (uint64_t)block_count * dev->block_size;
    return blkdev_write(id, offset, buffer, len);
}
