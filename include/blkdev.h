/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef BLKDEV_H
#define BLKDEV_H

#include <stddef.h>
#include <stdint.h>

#define BLKDEV_MAX 16
#define BLKDEV_NAME_MAX 16
#define BLKDEV_INVALID_ID 0xFFFFFFFFU

struct blkdev {
    uint8_t in_use;
    uint8_t read_only;
    uint8_t has_ops;
    uint8_t reserved;
    uint32_t id;
    uint32_t block_size;
    uint64_t size;
    uint8_t* data;
    void* ctx;
    int (*read_blocks)(void* ctx, uint64_t lba, uint32_t block_count, void* buffer);
    int (*write_blocks)(void* ctx, uint64_t lba, uint32_t block_count, const void* buffer);
    char name[BLKDEV_NAME_MAX];
};

void blkdev_init(void);
uint32_t blkdev_register_memory(const char* name, uint32_t start, uint32_t end, uint32_t block_size, uint8_t read_only);
uint32_t blkdev_register_ops(const char* name, uint64_t size, uint32_t block_size, uint8_t read_only, void* ctx, int (*read_blocks)(void*, uint64_t, uint32_t, void*), int (*write_blocks)(void*, uint64_t, uint32_t, const void*));
const struct blkdev* blkdev_get(uint32_t id);
const struct blkdev* blkdev_get_index(size_t index);
const struct blkdev* blkdev_find(const char* name);
int blkdev_read(uint32_t id, uint64_t offset, void* buffer, uint64_t len);
int blkdev_write(uint32_t id, uint64_t offset, const void* buffer, uint64_t len);
int blkdev_read_blocks(uint32_t id, uint64_t lba, uint32_t block_count, void* buffer);
int blkdev_write_blocks(uint32_t id, uint64_t lba, uint32_t block_count, const void* buffer);

#endif
