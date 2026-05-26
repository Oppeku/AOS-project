/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef PARTITION_H
#define PARTITION_H

#include <stdint.h>
#include <stddef.h>

#define PARTITION_MAX 8

enum partition_fs_type {
    PARTITION_FS_UNKNOWN = 0,
    PARTITION_FS_FAT32 = 1,
    PARTITION_FS_EXT4 = 2,
    PARTITION_FS_AOSFS = 3,
    PARTITION_FS_SWAP = 4,
};

enum partition_role {
    PARTITION_ROLE_UNKNOWN = 0,
    PARTITION_ROLE_ROOT = 1,
    PARTITION_ROLE_MAIN = 2,
    PARTITION_ROLE_ETC = 3,
    PARTITION_ROLE_COMMANDS = 4,
    PARTITION_ROLE_TMP = 5,
    PARTITION_ROLE_SWAP = 6,
    PARTITION_ROLE_TRASH = 7,
};

struct partition {
    uint8_t in_use;
    uint8_t fs_type;
    uint8_t role;
    uint8_t reserved;
    uint16_t index;
    uint32_t blkdev_id;
    uint32_t start;
    uint32_t end;
    uint64_t offset;
    uint64_t size;
    char name[16];
};

void partition_init(void);
int partition_register_memory(uint32_t start, uint32_t end, const char* name);
int partition_register_blkdev(uint32_t blkdev_id, uint64_t offset, uint64_t size, uint8_t fs_type, const char* name);
int partition_load_table(uint32_t blkdev_id);
const struct partition* partition_find_by_role(uint8_t role);
const struct partition* partition_find_by_fs(uint8_t fs_type);
const struct partition* partition_get(size_t index);
size_t partition_count(void);
int partition_create_planned(uint8_t fs_type, uint64_t size, uint8_t role);
int partition_delete_planned(size_t index);
int partition_cycle_planned_type(size_t index);
int partition_cycle_planned_role(size_t index);
int partition_delete_last_planned(void);
int partition_cycle_last_planned_type(void);
int partition_cycle_last_planned_role(void);
int partition_create_default_layout(uint32_t blkdev_id);
int partition_write_table(uint32_t blkdev_id);
const char* partition_fs_name(uint8_t fs_type);
const char* partition_role_name(uint8_t role);
void partition_print_table(void);

#endif
