/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <partition.h>
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);

#define EXT4_SUPER_OFFSET 1024U
#define EXT4_MAGIC_OFFSET (EXT4_SUPER_OFFSET + 56U)
#define PLANNED_PARTITION_SIZE (8ULL * 1024ULL * 1024ULL)
#define VIRTUAL_DISK_SIZE (64ULL * 1024ULL * 1024ULL)

static struct partition g_partitions[PARTITION_MAX];
static size_t g_partition_count;
static uint64_t g_next_offset;

static void local_memset(void* dst, int value, size_t n) {
    unsigned char* p = (unsigned char*)dst;
    while (n--) {
        *p++ = (unsigned char)value;
    }
}

static void copy_string(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void serial_print_u64(uint64_t value) {
    char buf[21];
    size_t i = sizeof(buf) - 1;
    buf[i] = '\0';
    if (value == 0) {
        serial_print("0");
        return;
    }
    while (value && i > 0) {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    }
    serial_print(&buf[i]);
}

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint8_t detect_fs(uint32_t start, uint32_t end) {
    const uint8_t* image = (const uint8_t*)(uintptr_t)start;
    uint64_t size = (uint64_t)end - start;

    if (end <= start) return PARTITION_FS_UNKNOWN;

    if (size >= 512 &&
        image[510] == 0x55 && image[511] == 0xAA &&
        image[82] == 'F' && image[83] == 'A' && image[84] == 'T' &&
        image[85] == '3' && image[86] == '2') {
        return PARTITION_FS_FAT32;
    }

    if (size >= EXT4_MAGIC_OFFSET + 2 &&
        read_le16(image + EXT4_MAGIC_OFFSET) == 0xEF53) {
        return PARTITION_FS_EXT4;
    }

    return PARTITION_FS_UNKNOWN;
}

void partition_init(void) {
    local_memset(g_partitions, 0, sizeof(g_partitions));
    g_partition_count = 0;
    g_next_offset = 0;
}

int partition_register_memory(uint32_t start, uint32_t end, const char* name) {
    struct partition* part;

    if (end <= start || g_partition_count >= PARTITION_MAX) {
        return -1;
    }

    part = &g_partitions[g_partition_count];
    local_memset(part, 0, sizeof(*part));
    part->in_use = 1;
    part->index = (uint16_t)g_partition_count;
    part->start = start;
    part->end = end;
    part->offset = g_next_offset;
    part->size = (uint64_t)end - start;
    part->fs_type = detect_fs(start, end);
    copy_string(part->name, sizeof(part->name), name);

    g_next_offset += part->size;
    g_partition_count++;
    return (int)part->index;
}

const struct partition* partition_find_by_fs(uint8_t fs_type) {
    for (size_t i = 0; i < g_partition_count; i++) {
        if (g_partitions[i].in_use && g_partitions[i].fs_type == fs_type) {
            return &g_partitions[i];
        }
    }
    return NULL;
}

const struct partition* partition_get(size_t index) {
    if (index >= g_partition_count || !g_partitions[index].in_use) {
        return NULL;
    }
    return &g_partitions[index];
}

size_t partition_count(void) {
    return g_partition_count;
}

int partition_create_planned(uint8_t fs_type, uint64_t size) {
    struct partition* part;
    char name[16] = "newp0";

    if (g_partition_count >= PARTITION_MAX) return -1;
    if (size == 0) size = PLANNED_PARTITION_SIZE;
    if (g_next_offset + size > VIRTUAL_DISK_SIZE) return -1;

    part = &g_partitions[g_partition_count];
    local_memset(part, 0, sizeof(*part));
    part->in_use = 1;
    part->index = (uint16_t)g_partition_count;
    part->offset = g_next_offset;
    part->size = size;
    part->fs_type = fs_type;
    if (part->index < 10) {
        name[4] = (char)('0' + part->index);
    }
    copy_string(part->name, sizeof(part->name), name);

    g_next_offset += size;
    g_partition_count++;
    return (int)part->index;
}

int partition_delete_last_planned(void) {
    if (g_partition_count == 0) return -1;
    return partition_delete_planned(g_partition_count - 1);
}

int partition_cycle_last_planned_type(void) {
    if (g_partition_count == 0) return -1;
    return partition_cycle_planned_type(g_partition_count - 1);
}

int partition_delete_planned(size_t index) {
    struct partition* part;

    if (index >= g_partition_count) return -1;
    part = &g_partitions[index];
    if (!part->in_use || part->start || part->end) return -1;
    if (index + 1 != g_partition_count) return -1;

    g_next_offset = part->offset;
    local_memset(part, 0, sizeof(*part));
    g_partition_count--;
    return 0;
}

int partition_cycle_planned_type(size_t index) {
    struct partition* part;

    if (index >= g_partition_count) return -1;
    part = &g_partitions[index];
    if (!part->in_use || part->start || part->end) return -1;

    if (part->fs_type == PARTITION_FS_UNKNOWN) {
        part->fs_type = PARTITION_FS_EXT4;
    } else if (part->fs_type == PARTITION_FS_EXT4) {
        part->fs_type = PARTITION_FS_FAT32;
    } else {
        part->fs_type = PARTITION_FS_UNKNOWN;
    }
    return 0;
}

const char* partition_fs_name(uint8_t fs_type) {
    switch (fs_type) {
        case PARTITION_FS_FAT32:
            return "fat32";
        case PARTITION_FS_EXT4:
            return "ext4";
        default:
            return "unknown";
    }
}

void partition_print_table(void) {
    serial_print("PartiotionMANAGAER: table\n");
    for (size_t i = 0; i < g_partition_count; i++) {
        serial_print("  ");
        serial_print(g_partitions[i].name);
        serial_print(" fs=");
        serial_print(partition_fs_name(g_partitions[i].fs_type));
        serial_print(" size=");
        serial_print_u64(g_partitions[i].size);
        serial_print("\n");
    }
}
