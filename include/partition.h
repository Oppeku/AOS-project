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
};

struct partition {
    uint8_t in_use;
    uint8_t fs_type;
    uint16_t index;
    uint32_t start;
    uint32_t end;
    uint64_t offset;
    uint64_t size;
    char name[16];
};

void partition_init(void);
int partition_register_memory(uint32_t start, uint32_t end, const char* name);
const struct partition* partition_find_by_fs(uint8_t fs_type);
const struct partition* partition_get(size_t index);
size_t partition_count(void);
int partition_create_planned(uint8_t fs_type, uint64_t size);
int partition_delete_planned(size_t index);
int partition_cycle_planned_type(size_t index);
int partition_delete_last_planned(void);
int partition_cycle_last_planned_type(void);
const char* partition_fs_name(uint8_t fs_type);
void partition_print_table(void);

#endif
