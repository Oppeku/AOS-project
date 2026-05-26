/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <partition.h>
#include <blkdev.h>
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);

#define EXT4_SUPER_OFFSET 1024U
#define EXT4_MAGIC_OFFSET (EXT4_SUPER_OFFSET + 56U)
#define AOSFS_MAGIC_SIZE 6U
#define PLANNED_PARTITION_SIZE (8ULL * 1024ULL * 1024ULL)
#define AOSPT_MAGIC0 0x50534F41U
#define AOSPT_MAGIC1 0x00003154U
#define AOSPT_VERSION 1U
#define AOSPT_FLAG_WRITTEN 1U
#define AOSPT_RESERVED_BYTES 512ULL
#define DEFAULT_ROOT_SIZE (8ULL * 1024ULL * 1024ULL)
#define LARGE_ROOT_SIZE (1024ULL * 1024ULL * 1024ULL)
#define PARTITION_USE_REST UINT64_MAX

static struct partition g_partitions[PARTITION_MAX];
static size_t g_partition_count;
static uint64_t g_next_offset;

struct aospt_entry {
    uint64_t offset;
    uint64_t size;
    uint32_t blkdev_id;
    uint16_t index;
    uint8_t fs_type;
    uint8_t role;
    uint8_t flags;
    char name[16];
    uint8_t reserved[12];
};

struct aospt_header {
    uint32_t magic0;
    uint32_t magic1;
    uint32_t version;
    uint32_t entry_count;
    uint64_t disk_size;
    struct aospt_entry entries[PARTITION_MAX];
};

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

static void recalc_indices_and_next_offset(void) {
    g_next_offset = 0;
    for (size_t i = 0; i < g_partition_count; i++) {
        g_partitions[i].index = (uint16_t)i;
        if (g_partitions[i].offset + g_partitions[i].size > g_next_offset) {
            g_next_offset = g_partitions[i].offset + g_partitions[i].size;
        }
    }
}

static int append_blkdev_partition(uint32_t blkdev_id, uint64_t offset, uint64_t size, uint8_t fs_type, uint8_t role, const char* name) {
    int index = partition_register_blkdev(blkdev_id, offset, size, fs_type, name);
    if (index < 0) {
        return -1;
    }
    g_partitions[index].role = role;
    return index;
}

static uint64_t next_offset_for_blkdev(uint32_t blkdev_id) {
    uint64_t next = AOSPT_RESERVED_BYTES;
    for (size_t i = 0; i < g_partition_count; i++) {
        if (!g_partitions[i].in_use || g_partitions[i].blkdev_id != blkdev_id) {
            continue;
        }
        if (g_partitions[i].offset + g_partitions[i].size > next) {
            next = g_partitions[i].offset + g_partitions[i].size;
        }
    }
    return next;
}

static const char* default_name_for_role(uint8_t role) {
    const char* role_name = partition_role_name(role);
    return role == PARTITION_ROLE_UNKNOWN ? NULL : role_name;
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

static uint8_t detect_fs(uint32_t blkdev_id, uint64_t size) {
    uint8_t image[EXT4_MAGIC_OFFSET + 2];
    uint64_t read_len = sizeof(image);

    if (blkdev_id == BLKDEV_INVALID_ID) return PARTITION_FS_UNKNOWN;
    if (size < read_len) read_len = size;
    local_memset(image, 0, sizeof(image));
    if (blkdev_read(blkdev_id, 0, image, read_len) != 0) {
        return PARTITION_FS_UNKNOWN;
    }

    if (size >= AOSFS_MAGIC_SIZE &&
        image[0] == 'A' && image[1] == 'O' && image[2] == 'S' &&
        image[3] == 'F' && image[4] == 'S' && image[5] == '1') {
        return PARTITION_FS_AOSFS;
    }
    if (size >= 512 + AOSFS_MAGIC_SIZE &&
        image[512] == 'A' && image[513] == 'O' && image[514] == 'S' &&
        image[515] == 'F' && image[516] == 'S' && image[517] == '1') {
        return PARTITION_FS_AOSFS;
    }

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

static uint8_t infer_role_from_name(const char* name) {
    if (!name) return PARTITION_ROLE_UNKNOWN;
    if (name[0] == 'r' && name[1] == 'o' && name[2] == 'o' && name[3] == 't' && name[4] == '\0') {
        return PARTITION_ROLE_ROOT;
    }
    if (name[0] == 'm' && name[1] == 'a' && name[2] == 'i' && name[3] == 'n' && name[4] == '\0') {
        return PARTITION_ROLE_MAIN;
    }
    if (name[0] == 'e' && name[1] == 't' && name[2] == 'c' && name[3] == '\0') {
        return PARTITION_ROLE_ETC;
    }
    if (name[0] == 'c' && name[1] == 'o' && name[2] == 'm' && name[3] == 'm' && name[4] == 'a' && name[5] == 'n' && name[6] == 'd' && name[7] == 's' && name[8] == '\0') {
        return PARTITION_ROLE_COMMANDS;
    }
    if (name[0] == 't' && name[1] == 'm' && name[2] == 'p' && name[3] == '\0') {
        return PARTITION_ROLE_TMP;
    }
    if (name[0] == 's' && name[1] == 'w' && name[2] == 'a' && name[3] == 'p' && name[4] == '\0') {
        return PARTITION_ROLE_SWAP;
    }
    if (name[0] == 't' && name[1] == 'r' && name[2] == 'a' && name[3] == 's' && name[4] == 'h' && name[5] == '\0') {
        return PARTITION_ROLE_TRASH;
    }
    return PARTITION_ROLE_UNKNOWN;
}

static void infer_default_role(struct partition* part) {
    if (!part || part->role != PARTITION_ROLE_UNKNOWN) {
        return;
    }
    part->role = infer_role_from_name(part->name);
    if (part->role == PARTITION_ROLE_UNKNOWN &&
        part->blkdev_id != BLKDEV_INVALID_ID &&
        part->offset == 512 &&
        part->fs_type == PARTITION_FS_AOSFS) {
        part->role = PARTITION_ROLE_ROOT;
    }
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
    part->blkdev_id = blkdev_register_memory(name, start, end, 512, 0);
    part->start = start;
    part->end = end;
    part->offset = g_next_offset;
    part->size = (uint64_t)end - start;
    part->fs_type = detect_fs(part->blkdev_id, part->size);
    part->role = infer_role_from_name(name);
    copy_string(part->name, sizeof(part->name), name);

    g_next_offset += part->size;
    g_partition_count++;
    return (int)part->index;
}

int partition_register_blkdev(uint32_t blkdev_id, uint64_t offset, uint64_t size, uint8_t fs_type, const char* name) {
    struct partition* part;

    if (blkdev_get(blkdev_id) == NULL || size == 0 || g_partition_count >= PARTITION_MAX) {
        return -1;
    }

    part = &g_partitions[g_partition_count];
    local_memset(part, 0, sizeof(*part));
    part->in_use = 1;
    part->index = (uint16_t)g_partition_count;
    part->blkdev_id = blkdev_id;
    part->start = (uint32_t)(offset / 512U);
    part->end = (uint32_t)((offset + size) / 512U);
    if (part->end > 0) {
        part->end--;
    }
    part->offset = offset;
    part->size = size;
    part->fs_type = fs_type;
    part->role = infer_role_from_name(name);
    if (part->role == PARTITION_ROLE_UNKNOWN &&
        offset == 512 &&
        fs_type == PARTITION_FS_AOSFS) {
        part->role = PARTITION_ROLE_ROOT;
    }
    copy_string(part->name, sizeof(part->name), name);

    if (offset + size > g_next_offset) {
        g_next_offset = offset + size;
    }
    g_partition_count++;
    return (int)part->index;
}

int partition_load_table(uint32_t blkdev_id) {
    const struct blkdev* dev = blkdev_get(blkdev_id);
    struct aospt_header header;
    uint32_t loaded = 0;

    if (!dev) {
        return -1;
    }
    local_memset(&header, 0, sizeof(header));
    if (blkdev_read(blkdev_id, 0, &header, sizeof(header)) != 0) {
        return -1;
    }
    if (header.magic0 != AOSPT_MAGIC0 || header.magic1 != AOSPT_MAGIC1 || header.version != AOSPT_VERSION) {
        return -1;
    }
    if (header.entry_count > PARTITION_MAX) {
        return -1;
    }

    for (uint32_t i = 0; i < header.entry_count; i++) {
        struct aospt_entry* entry = &header.entries[i];
        char loaded_name[16];
        uint8_t loaded_role = entry->role;
        uint8_t loaded_flags = entry->flags;

        copy_string(loaded_name, sizeof(loaded_name), entry->name);
        if (loaded_flags != AOSPT_FLAG_WRITTEN && (loaded_flags & AOSPT_FLAG_WRITTEN)) {
            loaded_name[0] = (char)loaded_flags;
            copy_string(loaded_name + 1, sizeof(loaded_name) - 1, entry->name);
            loaded_role = PARTITION_ROLE_UNKNOWN;
            loaded_flags = AOSPT_FLAG_WRITTEN;
        }

        if (!(loaded_flags & AOSPT_FLAG_WRITTEN)) {
            continue;
        }
        if (entry->size == 0 || entry->offset >= dev->size || entry->size > dev->size - entry->offset) {
            continue;
        }
        if (partition_register_blkdev(blkdev_id, entry->offset, entry->size, entry->fs_type, loaded_name) >= 0) {
            g_partitions[g_partition_count - 1].role = loaded_role;
            infer_default_role(&g_partitions[g_partition_count - 1]);
            loaded++;
        }
    }

    return loaded ? (int)loaded : -1;
}

const struct partition* partition_find_by_role(uint8_t role) {
    for (size_t i = 0; i < g_partition_count; i++) {
        if (g_partitions[i].in_use && g_partitions[i].role == role) {
            return &g_partitions[i];
        }
    }
    return NULL;
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

int partition_create_planned(uint8_t fs_type, uint64_t size, uint8_t role) {
    const uint32_t blkdev_id = 0;
    const struct blkdev* dev = blkdev_get(blkdev_id);
    struct partition* part;
    char name[16] = "newp0";
    const char* role_name = default_name_for_role(role);
    uint64_t offset;

    if (g_partition_count >= PARTITION_MAX) return -1;
    if (!dev || dev->read_only) return -1;

    offset = next_offset_for_blkdev(blkdev_id);
    if (offset >= dev->size) return -1;
    if (size == 0) size = PLANNED_PARTITION_SIZE;
    if (size == PARTITION_USE_REST) {
        size = dev->size - offset;
    }
    if (size == 0 || size > dev->size - offset) return -1;

    part = &g_partitions[g_partition_count];
    local_memset(part, 0, sizeof(*part));
    part->in_use = 1;
    part->index = (uint16_t)g_partition_count;
    part->blkdev_id = blkdev_id;
    part->offset = offset;
    part->size = size;
    part->fs_type = fs_type;
    part->role = role;
    if (part->index < 10) {
        name[4] = (char)('0' + part->index);
    }
    copy_string(part->name, sizeof(part->name), role_name ? role_name : name);

    if (part->offset + part->size > g_next_offset) {
        g_next_offset = part->offset + part->size;
    }
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

int partition_cycle_last_planned_role(void) {
    if (g_partition_count == 0) return -1;
    return partition_cycle_planned_role(g_partition_count - 1);
}

int partition_delete_planned(size_t index) {
    struct partition* part;

    if (index >= g_partition_count) return -1;
    part = &g_partitions[index];
    if (!part->in_use || part->start || part->end) return -1;
    if (index + 1 != g_partition_count) return -1;

    local_memset(part, 0, sizeof(*part));
    g_partition_count--;
    recalc_indices_and_next_offset();
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
    } else if (part->fs_type == PARTITION_FS_FAT32) {
        part->fs_type = PARTITION_FS_AOSFS;
    } else {
        part->fs_type = PARTITION_FS_UNKNOWN;
    }
    return 0;
}

int partition_cycle_planned_role(size_t index) {
    struct partition* part;

    if (index >= g_partition_count) return -1;
    part = &g_partitions[index];
    if (!part->in_use || part->start || part->end) return -1;

    if (part->role == PARTITION_ROLE_UNKNOWN) {
        part->role = PARTITION_ROLE_ROOT;
    } else if (part->role == PARTITION_ROLE_ROOT) {
        part->role = PARTITION_ROLE_MAIN;
    } else if (part->role == PARTITION_ROLE_MAIN) {
        part->role = PARTITION_ROLE_ETC;
    } else if (part->role == PARTITION_ROLE_ETC) {
        part->role = PARTITION_ROLE_COMMANDS;
    } else if (part->role == PARTITION_ROLE_COMMANDS) {
        part->role = PARTITION_ROLE_TMP;
    } else if (part->role == PARTITION_ROLE_TMP) {
        part->role = PARTITION_ROLE_SWAP;
    } else if (part->role == PARTITION_ROLE_SWAP) {
        part->role = PARTITION_ROLE_TRASH;
    } else if (part->role == PARTITION_ROLE_TRASH) {
        part->role = PARTITION_ROLE_UNKNOWN;
    } else {
        part->role = PARTITION_ROLE_UNKNOWN;
    }
    return 0;
}

int partition_create_default_layout(uint32_t blkdev_id) {
    const struct blkdev* dev = blkdev_get(blkdev_id);
    size_t write_index = 0;
    uint64_t root_offset = AOSPT_RESERVED_BYTES;
    uint64_t root_size = DEFAULT_ROOT_SIZE;
    uint64_t min_required = AOSPT_RESERVED_BYTES + DEFAULT_ROOT_SIZE;

    if (!dev || dev->read_only || dev->size < min_required) {
        return -1;
    }
    if (dev->size >= (2ULL * 1024ULL * 1024ULL * 1024ULL)) {
        root_size = LARGE_ROOT_SIZE;
    }

    for (size_t i = 0; i < g_partition_count; i++) {
        if (g_partitions[i].in_use && g_partitions[i].blkdev_id != blkdev_id) {
            if (write_index != i) {
                g_partitions[write_index] = g_partitions[i];
            }
            write_index++;
        }
    }
    for (size_t i = write_index; i < g_partition_count; i++) {
        local_memset(&g_partitions[i], 0, sizeof(g_partitions[i]));
    }
    g_partition_count = write_index;
    recalc_indices_and_next_offset();

    if (g_partition_count + 1 > PARTITION_MAX) {
        return -1;
    }

    if (append_blkdev_partition(blkdev_id, root_offset, root_size, PARTITION_FS_AOSFS, PARTITION_ROLE_ROOT, "root") < 0) {
        return -1;
    }

    recalc_indices_and_next_offset();
    return 0;
}

int partition_write_table(uint32_t blkdev_id) {
    const struct blkdev* dev = blkdev_get(blkdev_id);
    struct aospt_header header;
    uint32_t out_index = 0;

    if (!dev || dev->read_only) {
        return -1;
    }

    local_memset(&header, 0, sizeof(header));
    header.magic0 = AOSPT_MAGIC0;
    header.magic1 = AOSPT_MAGIC1;
    header.version = AOSPT_VERSION;
    header.disk_size = dev->size;

    for (size_t i = 0; i < g_partition_count && i < PARTITION_MAX; i++) {
        struct aospt_entry* out;
        struct partition* part = &g_partitions[i];
        if (!part->in_use || part->blkdev_id != blkdev_id || part->offset >= dev->size || part->size > dev->size - part->offset) {
            continue;
        }
        infer_default_role(part);
        if (out_index >= PARTITION_MAX) {
            break;
        }
        out = &header.entries[out_index];
        out->offset = part->offset;
        out->size = part->size;
        out->blkdev_id = blkdev_id;
        out->index = (uint16_t)out_index;
        out->fs_type = part->fs_type;
        out->role = part->role;
        out->flags = AOSPT_FLAG_WRITTEN;
        copy_string(out->name, sizeof(out->name), part->name);
        out_index++;
    }
    header.entry_count = out_index;

    return blkdev_write(blkdev_id, 0, &header, sizeof(header));
}

const char* partition_fs_name(uint8_t fs_type) {
    switch (fs_type) {
        case PARTITION_FS_FAT32:
            return "fat32";
        case PARTITION_FS_EXT4:
            return "ext4";
        case PARTITION_FS_AOSFS:
            return "aosfs";
        case PARTITION_FS_SWAP:
            return "swap";
        default:
            return "unknown";
    }
}

const char* partition_role_name(uint8_t role) {
    switch (role) {
        case PARTITION_ROLE_ROOT:
            return "root";
        case PARTITION_ROLE_MAIN:
            return "main";
        case PARTITION_ROLE_ETC:
            return "etc";
        case PARTITION_ROLE_COMMANDS:
            return "commands";
        case PARTITION_ROLE_TMP:
            return "tmp";
        case PARTITION_ROLE_SWAP:
            return "swap";
        case PARTITION_ROLE_TRASH:
            return "trash";
        default:
            return "unknown";
    }
}

void partition_print_table(void) {
    serial_print("PartiotionMANAGAER: table\n");
    for (size_t i = 0; i < g_partition_count; i++) {
        serial_print("  ");
        serial_print(g_partitions[i].name);
        serial_print(" role=");
        serial_print(partition_role_name(g_partitions[i].role));
        serial_print(" fs=");
        serial_print(partition_fs_name(g_partitions[i].fs_type));
        serial_print(" blkdev=");
        serial_print_u64(g_partitions[i].blkdev_id);
        serial_print(" size=");
        serial_print_u64(g_partitions[i].size);
        serial_print("\n");
    }
}
