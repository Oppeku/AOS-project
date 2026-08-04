/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <aosfs.h>
#include <blkdev.h>
#include <cpio.h>
#include <partition.h>

#define AOSFS_ROOT_INO 0xA05F0001ULL
#define AOSFS_DIR_INO_BASE 0xA05F1000ULL
#define AOSFS_FILE_INO_BASE 0xA05F8000ULL
#define AOSFS_DYNAMIC_INO_BASE 0xA05FD000ULL
#define AOSFS_MAX_ENTRIES 2000
#define AOSFS_COMPAT_MAX_ENTRIES 1024
#define AOSFS_LEGACY_MAX_ENTRIES 64
#define AOSFS_PATH_MAX 128
#define AOSFS_INLINE_CAPACITY 8192U
#define AOSFS_MEMORY_FILE_LIMIT 64
#define AOSFS_COMPAT_MAX_FILE_SIZE (64U * 1024U * 1024U)
#define AOSFS_MAX_FILE_SIZE (1024U * 1024U * 1024U)
#define AOSFS_MIN_EXTENT_CAPACITY (64U * 1024U)
#define AOSFS_EXTENT_ALIGNMENT 4096U
#define AOSFS_MAGIC0 0x46534F41U
#define AOSFS_MAGIC1 0x00003153U
#define AOSFS_LEGACY_VERSION 3U
#define AOSFS_VERSION 4U
#define AOSFS_BLOCK_SIZE 512U
#define AOSFS_SUPER_OFFSET 0U
#define AOSFS_TABLE_OFFSET 4096U
#define AOSFS_DATA_OFFSET 524288U
#define AOSFS_LEGACY_DATA_OFFSET 65536U

#define LINUX_DTYPE_REG 8
#define LINUX_DTYPE_DIR 4

static const char* g_root_dirs[] = {
    "aos",
    "bin",
    "boot",
    "bootloader",
    "Bluetooth",
    "commands",
    "configs",
    "drivers",
    "dev",
    "etc",
    "home",
    "kernel",
    "lib",
    "lib64",
    "logs",
    "main",
    "memory",
    "mnt",
    "MUI",
    "networking-stack",
    "opt",
    "proc",
    "run",
    "runtime",
    "root",
    "sbin",
    "sqausased",
    "sudo",
    "sys",
    "squashed",
    "tmp",
    "trash",
    "usr",
    "var",
};

struct aosfs_entry {
    uint8_t in_use;
    uint8_t is_dir;
    uint32_t size;
    uint64_t inode;
    uint64_t data_offset;
    uint64_t data_capacity;
    char path[AOSFS_PATH_MAX];
};

struct aosfs_superblock {
    uint32_t magic0;
    uint32_t magic1;
    uint32_t version;
    uint32_t block_size;
    uint32_t max_entries;
    uint32_t path_max;
    uint32_t file_capacity;
    uint32_t table_offset;
    uint32_t data_offset;
    uint32_t reserved[119];
};

struct aosfs_disk_entry {
    uint8_t in_use;
    uint8_t is_dir;
    uint16_t reserved0;
    uint32_t size;
    uint64_t inode;
    char path[AOSFS_PATH_MAX];
    uint64_t data_offset;
    uint64_t data_capacity;
    uint8_t reserved1[96];
};

struct aosfs_instance {
    uint8_t mounted;
    uint8_t role;
    uint8_t block_backed;
    uint8_t reserved;
    uint32_t blkdev_id;
    uint64_t base_offset;
    uint64_t next_inode;
    uint64_t next_data_offset;
    uint32_t format_version;
    uint32_t entry_limit;
    struct aosfs_entry entries[AOSFS_MAX_ENTRIES];
};

static struct aosfs_instance g_instances[PARTITION_ROLE_TRASH + 1];
static struct aosfs_instance* g_active;
static uint8_t g_memory_data[PARTITION_ROLE_TRASH + 1]
                            [AOSFS_MEMORY_FILE_LIMIT]
                            [AOSFS_INLINE_CAPACITY];

#define g_entries (g_active->entries)
#define g_next_inode (g_active->next_inode)
#define g_blkdev_id (g_active->blkdev_id)
#define g_base_offset (g_active->base_offset)
#define g_block_backed (g_active->block_backed)
#define g_next_data_offset (g_active->next_data_offset)
#define g_format_version (g_active->format_version)
#define g_entry_limit (g_active->entry_limit)

static uint8_t g_extent_copy_buffer[4096];

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
        dst[0] = '\0';
        return;
    }
    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int path_equals(const char* a, const char* b) {
    size_t i = 0;
    while (a && b && a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a && b && a[i] == '\0' && b[i] == '\0';
}

static size_t local_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) {
        len++;
    }
    return len;
}

static int contains_slash(const char* path) {
    while (path && *path) {
        if (*path == '/') {
            return 1;
        }
        path++;
    }
    return 0;
}

static int root_dir_index(const char* path) {
    for (size_t i = 0; i < sizeof(g_root_dirs) / sizeof(g_root_dirs[0]); i++) {
        if (path_equals(path, g_root_dirs[i])) {
            return (int)i;
        }
    }
    return -1;
}

static int parent_path_of(const char* path, char* out, size_t out_size) {
    size_t len = local_strlen(path);
    size_t slash = len;

    if (!out || out_size == 0) {
        return -1;
    }
    out[0] = '\0';
    if (!path || len == 0) {
        return 0;
    }
    while (slash > 0 && path[slash - 1] != '/') {
        slash--;
    }
    if (slash == 0) {
        return 0;
    }
    if (slash - 1 >= out_size) {
        return -1;
    }
    for (size_t i = 0; i < slash - 1; i++) {
        out[i] = path[i];
    }
    out[slash - 1] = '\0';
    return 0;
}

static int path_starts_with_component(const char* path, const char* prefix) {
    size_t i = 0;
    if (!path || !prefix) {
        return 0;
    }
    if (prefix[0] == '\0') {
        return 1;
    }
    while (prefix[i]) {
        if (path[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static uint8_t role_from_selector(const char* selector, size_t len) {
    if (len == 4 && selector[0] == 'r' && selector[1] == 'o' && selector[2] == 'o' && selector[3] == 't') {
        return PARTITION_ROLE_ROOT;
    }
    if (len == 4 && selector[0] == 'm' && selector[1] == 'a' && selector[2] == 'i' && selector[3] == 'n') {
        return PARTITION_ROLE_MAIN;
    }
    if (len == 3 && selector[0] == 'e' && selector[1] == 't' && selector[2] == 'c') {
        return PARTITION_ROLE_ETC;
    }
    if (len == 8 && selector[0] == 'c' && selector[1] == 'o' && selector[2] == 'm' && selector[3] == 'm' &&
        selector[4] == 'a' && selector[5] == 'n' && selector[6] == 'd' && selector[7] == 's') {
        return PARTITION_ROLE_COMMANDS;
    }
    if (len == 3 && selector[0] == 't' && selector[1] == 'm' && selector[2] == 'p') {
        return PARTITION_ROLE_TMP;
    }
    if (len == 5 && selector[0] == 't' && selector[1] == 'r' && selector[2] == 'a' && selector[3] == 's' && selector[4] == 'h') {
        return PARTITION_ROLE_TRASH;
    }
    return PARTITION_ROLE_UNKNOWN;
}

static struct aosfs_instance* instance_for_role(uint8_t role) {
    if (role > PARTITION_ROLE_TRASH) {
        role = PARTITION_ROLE_ROOT;
    }
    return &g_instances[role];
}

static int select_instance_for_path(const char** path_ptr) {
    const char* path = path_ptr ? *path_ptr : NULL;
    uint8_t role = PARTITION_ROLE_ROOT;
    size_t len = 0;

    if (!path_ptr || !path) {
        return -1;
    }

    while (*path == '/') {
        path++;
    }

    if (path[0] == '@') {
        const char* selector = path + 1;
        while (selector[len] && selector[len] != '/') {
            len++;
        }
        role = role_from_selector(selector, len);
        if (role != PARTITION_ROLE_UNKNOWN) {
            path = selector + len;
            while (*path == '/') {
                path++;
            }
        }
    } else {
        while (path[len] && path[len] != '/') {
            len++;
        }
        role = role_from_selector(path, len);
        if (role != PARTITION_ROLE_UNKNOWN && g_instances[role].mounted) {
            path += len;
            while (*path == '/') {
                path++;
            }
        } else {
            role = PARTITION_ROLE_ROOT;
        }
    }

    g_active = instance_for_role(role);
    if (!g_active->mounted && role != PARTITION_ROLE_ROOT) {
        g_active = instance_for_role(PARTITION_ROLE_ROOT);
    }
    *path_ptr = path;
    return 0;
}

static void make_node_path(const char* path, char* out, size_t out_size) {
    const char* role_name;
    size_t len = 0;

    if (!out || out_size == 0) {
        return;
    }
    if (!g_active || g_active->role == PARTITION_ROLE_ROOT) {
        copy_string(out, out_size, path);
        return;
    }

    role_name = partition_role_name(g_active->role);
    if (out_size < 3) {
        out[0] = '\0';
        return;
    }
    out[len++] = '@';
    while (*role_name && len + 1 < out_size) {
        out[len++] = *role_name++;
    }
    if (path && path[0] && len + 1 < out_size) {
        out[len++] = '/';
        while (*path && len + 1 < out_size) {
            out[len++] = *path++;
        }
    }
    out[len] = '\0';
}

static int immediate_child_name(const char* parent, const char* child_path, const char** name, size_t* name_len) {
    size_t parent_len = local_strlen(parent);
    const char* rest;
    size_t len = 0;

    if (!parent || !child_path || !name || !name_len) {
        return 0;
    }
    if (parent_len == 0) {
        rest = child_path;
    } else {
        if (!path_starts_with_component(child_path, parent)) {
            return 0;
        }
        rest = child_path + parent_len;
        if (*rest != '/') {
            return 0;
        }
        rest++;
    }
    if (*rest == '\0') {
        return 0;
    }
    while (rest[len] && rest[len] != '/') {
        len++;
    }
    if (len == 0 || rest[len] == '/') {
        return 0;
    }
    *name = rest;
    *name_len = len;
    return 1;
}

static int name_equals_len(const char* a, const char* b, size_t b_len) {
    size_t i = 0;
    while (a && a[i] && i < b_len) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a && a[i] == '\0' && i == b_len;
}

static int root_name_is_builtin(const char* name) {
    return root_dir_index(name) >= 0;
}

static struct aosfs_entry* find_entry(const char* path) {
    for (size_t i = 0; i < g_entry_limit; i++) {
        if (g_entries[i].in_use && path_equals(g_entries[i].path, path)) {
            return &g_entries[i];
        }
    }
    return NULL;
}

static int entry_index(const struct aosfs_entry* entry) {
    if (!entry) {
        return -1;
    }
    for (size_t i = 0; i < g_entry_limit; i++) {
        if (&g_entries[i] == entry) {
            return (int)i;
        }
    }
    return -1;
}

static uint64_t align_up_u64(uint64_t value, uint64_t alignment) {
    if (alignment == 0 || value > UINT64_MAX - (alignment - 1)) {
        return UINT64_MAX;
    }
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64_t legacy_entry_data_offset(size_t index) {
    return AOSFS_LEGACY_DATA_OFFSET + (uint64_t)index * AOSFS_INLINE_CAPACITY;
}

static uint8_t* memory_entry_data(size_t index) {
    if (!g_active || g_active->role > PARTITION_ROLE_TRASH ||
        index >= AOSFS_MEMORY_FILE_LIMIT) {
        return NULL;
    }
    return g_memory_data[g_active->role][index];
}

static uint64_t entry_data_offset(const struct aosfs_entry* entry, size_t index) {
    if (g_format_version >= AOSFS_VERSION && entry && entry->data_capacity != 0) {
        return entry->data_offset;
    }
    return legacy_entry_data_offset(index);
}

static uint64_t active_storage_size(void) {
    const struct blkdev* dev = blkdev_get(g_blkdev_id);
    const struct partition* part = partition_find_by_role(g_active->role);

    if (!dev || g_base_offset >= dev->size) {
        return 0;
    }
    if (part && part->blkdev_id == g_blkdev_id &&
        part->offset == g_base_offset && part->size <= dev->size - g_base_offset) {
        return part->size;
    }
    return dev->size - g_base_offset;
}

static void fill_superblock(struct aosfs_superblock* super) {
    local_memset(super, 0, sizeof(*super));
    super->magic0 = AOSFS_MAGIC0;
    super->magic1 = AOSFS_MAGIC1;
    super->version = AOSFS_VERSION;
    super->block_size = AOSFS_BLOCK_SIZE;
    super->max_entries = AOSFS_MAX_ENTRIES;
    super->path_max = AOSFS_PATH_MAX;
    super->file_capacity = AOSFS_MAX_FILE_SIZE;
    super->table_offset = AOSFS_TABLE_OFFSET;
    super->data_offset = AOSFS_DATA_OFFSET;
}

static int superblock_valid(const struct aosfs_superblock* super) {
    return super &&
           super->magic0 == AOSFS_MAGIC0 &&
           super->magic1 == AOSFS_MAGIC1 &&
           super->version == AOSFS_VERSION &&
           (super->max_entries == AOSFS_MAX_ENTRIES ||
            super->max_entries == AOSFS_COMPAT_MAX_ENTRIES) &&
           super->path_max == AOSFS_PATH_MAX &&
           (super->file_capacity == AOSFS_MAX_FILE_SIZE ||
            super->file_capacity == AOSFS_COMPAT_MAX_FILE_SIZE) &&
           super->table_offset == AOSFS_TABLE_OFFSET &&
           super->data_offset == AOSFS_DATA_OFFSET;
}

static int legacy_superblock_valid(const struct aosfs_superblock* super) {
    return super &&
           super->magic0 == AOSFS_MAGIC0 &&
           super->magic1 == AOSFS_MAGIC1 &&
           super->version == AOSFS_LEGACY_VERSION &&
           super->max_entries == AOSFS_LEGACY_MAX_ENTRIES &&
           super->path_max == AOSFS_PATH_MAX &&
           super->file_capacity == AOSFS_INLINE_CAPACITY &&
           super->table_offset == AOSFS_TABLE_OFFSET &&
           super->data_offset == AOSFS_LEGACY_DATA_OFFSET;
}

static int sync_superblock(void) {
    struct aosfs_superblock super;
    if (!g_block_backed) {
        return 0;
    }
    fill_superblock(&super);
    return blkdev_write(g_blkdev_id, g_base_offset + AOSFS_SUPER_OFFSET, &super, sizeof(super));
}

static int sync_entry(size_t index) {
    struct aosfs_disk_entry disk_entry;
    if (!g_block_backed || index >= g_entry_limit) {
        return 0;
    }
    local_memset(&disk_entry, 0, sizeof(disk_entry));
    disk_entry.in_use = g_entries[index].in_use;
    disk_entry.is_dir = g_entries[index].is_dir;
    disk_entry.size = g_entries[index].size;
    disk_entry.inode = g_entries[index].inode;
    disk_entry.data_offset = g_entries[index].data_offset;
    disk_entry.data_capacity = g_entries[index].data_capacity;
    copy_string(disk_entry.path, sizeof(disk_entry.path), g_entries[index].path);
    return blkdev_write(g_blkdev_id, g_base_offset + AOSFS_TABLE_OFFSET + index * sizeof(disk_entry), &disk_entry, sizeof(disk_entry));
}

static int sync_entry_data(size_t index) {
    (void)index;
    return 0;
}

static int dynamic_child_seen(const char* parent, const char* name) {
    const char* child = NULL;
    size_t child_len = 0;
    for (size_t i = 0; i < g_entry_limit; i++) {
        if (!g_entries[i].in_use) {
            continue;
        }
        if (immediate_child_name(parent, g_entries[i].path, &child, &child_len) &&
            name_equals_len(name, child, child_len)) {
            return 1;
        }
    }
    return 0;
}

static int dynamic_dir_has_children(const char* path) {
    const char* child = NULL;
    size_t child_len = 0;

    for (size_t i = 0; i < g_entry_limit; i++) {
        if (!g_entries[i].in_use) {
            continue;
        }
        if (immediate_child_name(path, g_entries[i].path, &child, &child_len)) {
            return 1;
        }
    }

    return 0;
}

static int dir_exists(const char* path) {
    struct aosfs_entry* entry;
    if (!path) {
        return 0;
    }
    if (path[0] == '\0' || root_dir_index(path) >= 0) {
        return 1;
    }
    entry = find_entry(path);
    return entry && entry->is_dir;
}

static int commands_leaf(const char* path, const char** leaf_out) {
    const char prefix[] = "commands/";
    size_t i = 0;
    while (prefix[i]) {
        if (!path || path[i] != prefix[i]) {
            return -1;
        }
        i++;
    }
    if (path[i] == '\0' || contains_slash(path + i)) {
        return -1;
    }
    *leaf_out = path + i;
    return 0;
}

static void fill_dir_node(const char* path, uint64_t inode, struct vfs_node* out) {
    char node_path[AOSFS_PATH_MAX + 16];

    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_DIRECTORY;
    out->backend = VFS_BACKEND_AOSFS;
    out->inode = inode;
    make_node_path(path, node_path, sizeof(node_path));
    copy_string(out->path, sizeof(out->path), node_path);
}

static void fill_file_node(const char* path, const uint8_t* data, uint32_t size, struct vfs_node* out) {
    char node_path[AOSFS_PATH_MAX + 16];

    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_AOSFS;
    out->size = size;
    out->inode = AOSFS_FILE_INO_BASE + ((uint64_t)(uintptr_t)data >> 4);
    out->u.data = data;
    make_node_path(path, node_path, sizeof(node_path));
    copy_string(out->path, sizeof(out->path), node_path);
}

static void fill_dynamic_node(struct aosfs_entry* entry, struct vfs_node* out) {
    char node_path[AOSFS_PATH_MAX + 16];
    int index = entry_index(entry);

    local_memset(out, 0, sizeof(*out));
    out->type = entry->is_dir ? VFS_NODE_TYPE_DIRECTORY : VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_AOSFS;
    out->size = entry->size;
    out->inode = entry->inode;
    out->u.data = g_block_backed || index < 0
                      ? NULL
                      : memory_entry_data((size_t)index);
    make_node_path(entry->path, node_path, sizeof(node_path));
    copy_string(out->path, sizeof(out->path), node_path);
}

void aosfs_init(void) {
    local_memset(g_instances, 0, sizeof(g_instances));
    local_memset(g_memory_data, 0, sizeof(g_memory_data));
    for (size_t i = 0; i < sizeof(g_instances) / sizeof(g_instances[0]); i++) {
        g_instances[i].role = (uint8_t)i;
        g_instances[i].blkdev_id = BLKDEV_INVALID_ID;
        g_instances[i].next_inode = AOSFS_DYNAMIC_INO_BASE;
        g_instances[i].next_data_offset = AOSFS_DATA_OFFSET;
        g_instances[i].format_version = AOSFS_VERSION;
        g_instances[i].entry_limit = AOSFS_MAX_ENTRIES;
    }
    g_active = instance_for_role(PARTITION_ROLE_ROOT);
}

int aosfs_mount(uint32_t blkdev_id) {
    return aosfs_mount_at(blkdev_id, 0);
}

int aosfs_mount_at(uint32_t blkdev_id, uint64_t base_offset) {
    return aosfs_mount_role(PARTITION_ROLE_ROOT, blkdev_id, base_offset);
}

int aosfs_mount_role(uint8_t role, uint32_t blkdev_id, uint64_t base_offset) {
    struct aosfs_superblock super;
    uint64_t highest_inode = AOSFS_DYNAMIC_INO_BASE - 1;
    uint64_t storage_size;
    size_t entry_count;
    int legacy = 0;

    if (blkdev_get(blkdev_id) == NULL) {
        return -1;
    }
    g_active = instance_for_role(role);
    g_blkdev_id = blkdev_id;
    g_base_offset = base_offset;
    g_block_backed = 1;
    g_active->mounted = 1;
    g_format_version = AOSFS_VERSION;
    g_entry_limit = AOSFS_MAX_ENTRIES;
    g_next_data_offset = AOSFS_DATA_OFFSET;
    storage_size = active_storage_size();
    if (storage_size < AOSFS_DATA_OFFSET) {
        return -1;
    }

    local_memset(&super, 0, sizeof(super));
    if (blkdev_read(g_blkdev_id, g_base_offset + AOSFS_SUPER_OFFSET, &super, sizeof(super)) != 0) {
        return -1;
    }
    legacy = legacy_superblock_valid(&super);
    if (!superblock_valid(&super) && !legacy) {
        local_memset(g_entries, 0, sizeof(g_entries));
        if (sync_superblock() != 0) {
            return -1;
        }
        for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
            if (sync_entry(i) != 0) {
                return -1;
            }
        }
        g_next_inode = AOSFS_DYNAMIC_INO_BASE;
        return 0;
    }

    local_memset(g_entries, 0, sizeof(g_entries));
    g_entry_limit = legacy ? AOSFS_LEGACY_MAX_ENTRIES : super.max_entries;
    if (legacy) {
        g_next_data_offset = align_up_u64(
            legacy_entry_data_offset(AOSFS_LEGACY_MAX_ENTRIES),
            AOSFS_EXTENT_ALIGNMENT);
    }
    entry_count = g_entry_limit;
    for (size_t i = 0; i < entry_count; i++) {
        struct aosfs_disk_entry disk_entry;
        uint64_t extent_end;

        local_memset(&disk_entry, 0, sizeof(disk_entry));
        if (blkdev_read(g_blkdev_id, g_base_offset + AOSFS_TABLE_OFFSET + i * sizeof(disk_entry), &disk_entry, sizeof(disk_entry)) != 0) {
            return -1;
        }
        if (!disk_entry.in_use) {
            continue;
        }
        g_entries[i].in_use = 1;
        g_entries[i].is_dir = disk_entry.is_dir ? 1 : 0;
        g_entries[i].size = disk_entry.size;
        g_entries[i].inode = disk_entry.inode;
        copy_string(g_entries[i].path, sizeof(g_entries[i].path), disk_entry.path);

        if (!g_entries[i].is_dir) {
            if (legacy) {
                uint64_t copied = 0;
                uint64_t new_offset = g_next_data_offset;

                if (g_entries[i].size > AOSFS_INLINE_CAPACITY) {
                    return -1;
                }
                if (new_offset > UINT64_MAX - AOSFS_INLINE_CAPACITY ||
                    new_offset + AOSFS_INLINE_CAPACITY > storage_size) {
                    return -1;
                }
                while (copied < g_entries[i].size) {
                    uint64_t chunk = g_entries[i].size - copied;
                    if (chunk > sizeof(g_extent_copy_buffer)) {
                        chunk = sizeof(g_extent_copy_buffer);
                    }
                    if (blkdev_read(g_blkdev_id,
                                    g_base_offset + legacy_entry_data_offset(i) + copied,
                                    g_extent_copy_buffer,
                                    chunk) != 0 ||
                        blkdev_write(g_blkdev_id,
                                     g_base_offset + new_offset + copied,
                                     g_extent_copy_buffer,
                                     chunk) != 0) {
                        return -1;
                    }
                    copied += chunk;
                }
                g_entries[i].data_offset = new_offset;
                g_entries[i].data_capacity = AOSFS_INLINE_CAPACITY;
                g_next_data_offset = new_offset + AOSFS_INLINE_CAPACITY;
            } else {
                g_entries[i].data_offset = disk_entry.data_offset;
                g_entries[i].data_capacity = disk_entry.data_capacity;
                if (g_entries[i].size > AOSFS_MAX_FILE_SIZE ||
                    g_entries[i].size > g_entries[i].data_capacity ||
                    (g_entries[i].data_capacity != 0 &&
                     g_entries[i].data_offset < AOSFS_DATA_OFFSET)) {
                    return -1;
                }
            }

            if (g_entries[i].data_capacity != 0) {
                if (g_entries[i].data_offset > UINT64_MAX - g_entries[i].data_capacity) {
                    return -1;
                }
                extent_end = g_entries[i].data_offset + g_entries[i].data_capacity;
                if (extent_end > storage_size) {
                    return -1;
                }
                if (!legacy && extent_end > g_next_data_offset) {
                    g_next_data_offset = extent_end;
                }
            }
        }
        if (g_entries[i].inode > highest_inode) {
            highest_inode = g_entries[i].inode;
        }
    }
    g_next_inode = highest_inode + 1;

    if (legacy) {
        g_format_version = AOSFS_VERSION;
        g_entry_limit = AOSFS_MAX_ENTRIES;
        for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
            if (sync_entry(i) != 0) {
                return -1;
            }
        }
        if (sync_superblock() != 0) {
            return -1;
        }
    }
    return 0;
}

int aosfs_format_role(uint8_t role, uint32_t blkdev_id, uint64_t base_offset) {
    struct aosfs_instance* instance;

    if (blkdev_get(blkdev_id) == NULL) {
        return -1;
    }
    instance = instance_for_role(role);
    if (!instance) {
        return -1;
    }
    local_memset(instance, 0, sizeof(*instance));
    instance->mounted = 1;
    instance->role = role;
    instance->block_backed = 1;
    instance->blkdev_id = blkdev_id;
    instance->base_offset = base_offset;
    instance->next_inode = AOSFS_DYNAMIC_INO_BASE;
    instance->next_data_offset = AOSFS_DATA_OFFSET;
    instance->format_version = AOSFS_VERSION;
    instance->entry_limit = AOSFS_MAX_ENTRIES;
    g_active = instance;

    if (active_storage_size() < AOSFS_DATA_OFFSET) {
        return -1;
    }
    if (sync_superblock() != 0) {
        return -1;
    }
    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
        if (sync_entry(i) != 0) {
            return -1;
        }
    }
    return 0;
}

int aosfs_lookup_path(const char* path, struct vfs_node* out) {
    uint8_t* data = NULL;
    uint32_t size = 0;
    const char* leaf = NULL;
    struct aosfs_entry* entry = NULL;
    int dir_index;

    if (!path || !out) {
        return -1;
    }
    if (select_instance_for_path(&path) != 0) {
        return -1;
    }

    entry = find_entry(path);
    if (entry) {
        fill_dynamic_node(entry, out);
        return 0;
    }

    if (path[0] == '\0') {
        fill_dir_node("", AOSFS_ROOT_INO, out);
        return 0;
    }

    dir_index = root_dir_index(path);
    if (dir_index >= 0) {
        fill_dir_node(path, AOSFS_DIR_INO_BASE + (uint64_t)dir_index, out);
        return 0;
    }

    if (commands_leaf(path, &leaf) == 0 && initrd_get_file(leaf, &data, &size) == 0) {
        fill_file_node(path, data, size, out);
        return 0;
    }

    if (initrd_get_file(path, &data, &size) == 0) {
        fill_file_node(path, data, size, out);
        return 0;
    }

    return -1;
}

int aosfs_create_path(const char* path, struct vfs_node* out) {
    char parent[AOSFS_PATH_MAX];
    struct aosfs_entry* entry;
    uint8_t* data = NULL;
    uint32_t size = 0;
    const char* command_name = NULL;

    if (!path || !out || path[0] == '\0' || local_strlen(path) >= AOSFS_PATH_MAX) {
        return -1;
    }
    if (select_instance_for_path(&path) != 0 || path[0] == '\0' || local_strlen(path) >= AOSFS_PATH_MAX) {
        return -1;
    }
    if (commands_leaf(path, &command_name) == 0) {
        return -1;
    }
    if (!contains_slash(path) && initrd_get_file(path, &data, &size) == 0) {
        return -1;
    }
    if (parent_path_of(path, parent, sizeof(parent)) != 0 || !dir_exists(parent)) {
        return -1;
    }

    entry = find_entry(path);
    if (entry) {
        if (entry->is_dir) {
            return -1;
        }
        fill_dynamic_node(entry, out);
        return 0;
    }

    size_t file_entry_limit = g_block_backed
                                  ? g_entry_limit
                                  : AOSFS_MEMORY_FILE_LIMIT;
    for (size_t i = 0; i < file_entry_limit; i++) {
        if (!g_entries[i].in_use) {
            local_memset(&g_entries[i], 0, sizeof(g_entries[i]));
            g_entries[i].in_use = 1;
            g_entries[i].is_dir = 0;
            g_entries[i].size = 0;
            g_entries[i].inode = g_next_inode++;
            copy_string(g_entries[i].path, sizeof(g_entries[i].path), path);
            if (sync_entry(i) != 0 || sync_entry_data(i) != 0) {
                local_memset(&g_entries[i], 0, sizeof(g_entries[i]));
                return -1;
            }
            fill_dynamic_node(&g_entries[i], out);
            return 0;
        }
    }

    return -1;
}

int aosfs_mkdir_path(const char* path) {
    char parent[AOSFS_PATH_MAX];
    const char* command_name = NULL;

    if (!path || path[0] == '\0' || local_strlen(path) >= AOSFS_PATH_MAX) {
        return -1;
    }
    if (select_instance_for_path(&path) != 0 || path[0] == '\0' || local_strlen(path) >= AOSFS_PATH_MAX) {
        return -1;
    }
    if (find_entry(path) || root_dir_index(path) >= 0) {
        return -1;
    }
    if (commands_leaf(path, &command_name) == 0 || (!contains_slash(path) && root_name_is_builtin(path))) {
        return -1;
    }
    if (parent_path_of(path, parent, sizeof(parent)) != 0 || !dir_exists(parent)) {
        return -1;
    }

    for (size_t i = 0; i < g_entry_limit; i++) {
        if (!g_entries[i].in_use) {
            g_entries[i].in_use = 1;
            g_entries[i].is_dir = 1;
            g_entries[i].size = 0;
            g_entries[i].inode = g_next_inode++;
            copy_string(g_entries[i].path, sizeof(g_entries[i].path), path);
            if (sync_entry(i) != 0) {
                local_memset(&g_entries[i], 0, sizeof(g_entries[i]));
                return -1;
            }
            return 0;
        }
    }

    return -1;
}

static int reserve_entry_extent(struct aosfs_entry* entry, size_t index, uint64_t required) {
    const struct blkdev* dev;
    uint64_t new_capacity;
    uint64_t new_offset;
    uint64_t new_end;
    uint64_t copied = 0;
    uint64_t old_offset;
    uint64_t old_capacity;

    if (!entry || index >= g_entry_limit || !g_block_backed ||
        g_format_version < AOSFS_VERSION || required > AOSFS_MAX_FILE_SIZE) {
        return -1;
    }
    if (required <= entry->data_capacity) {
        return 0;
    }

    new_capacity = entry->data_capacity;
    if (new_capacity < AOSFS_MIN_EXTENT_CAPACITY) {
        new_capacity = AOSFS_MIN_EXTENT_CAPACITY;
    }
    while (new_capacity < required) {
        if (new_capacity > AOSFS_MAX_FILE_SIZE / 2U) {
            new_capacity = AOSFS_MAX_FILE_SIZE;
            break;
        }
        new_capacity *= 2U;
    }
    if (new_capacity < required || new_capacity > AOSFS_MAX_FILE_SIZE) {
        return -1;
    }

    old_offset = entry->data_offset;
    old_capacity = entry->data_capacity;
    if (old_capacity != 0 &&
        old_offset <= UINT64_MAX - old_capacity &&
        old_offset + old_capacity == g_next_data_offset) {
        new_end = old_offset + new_capacity;
        dev = blkdev_get(g_blkdev_id);
        if (!dev || new_end > active_storage_size()) return -1;
        entry->data_capacity = new_capacity;
        if (sync_entry(index) != 0) {
            entry->data_capacity = old_capacity;
            return -1;
        }
        g_next_data_offset = new_end;
        return 0;
    }

    new_offset = align_up_u64(g_next_data_offset, AOSFS_EXTENT_ALIGNMENT);
    if (new_offset == UINT64_MAX || new_offset > UINT64_MAX - new_capacity) {
        return -1;
    }
    new_end = new_offset + new_capacity;
    dev = blkdev_get(g_blkdev_id);
    if (!dev || new_end > active_storage_size()) {
        return -1;
    }

    while (copied < entry->size) {
        uint64_t chunk = entry->size - copied;
        if (chunk > sizeof(g_extent_copy_buffer)) {
            chunk = sizeof(g_extent_copy_buffer);
        }
        if (old_capacity == 0 ||
            blkdev_read(g_blkdev_id,
                        g_base_offset + old_offset + copied,
                        g_extent_copy_buffer,
                        chunk) != 0 ||
            blkdev_write(g_blkdev_id,
                         g_base_offset + new_offset + copied,
                         g_extent_copy_buffer,
                         chunk) != 0) {
            return -1;
        }
        copied += chunk;
    }

    entry->data_offset = new_offset;
    entry->data_capacity = new_capacity;
    if (sync_entry(index) != 0) {
        entry->data_offset = old_offset;
        entry->data_capacity = old_capacity;
        return -1;
    }
    g_next_data_offset = new_end;
    return 0;
}

int aosfs_read_path(const char* path, uint64_t offset, uint8_t* buffer, uint64_t len) {
    struct aosfs_entry* entry;
    uint8_t* memory_data;
    int index;

    if (!path || (!buffer && len != 0) || select_instance_for_path(&path) != 0) {
        return -1;
    }
    entry = find_entry(path);
    index = entry_index(entry);
    if (!entry || entry->is_dir || index < 0 ||
        offset > entry->size || len > (uint64_t)entry->size - offset) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    if (g_block_backed && g_format_version >= AOSFS_VERSION) {
        if (entry->data_capacity == 0 ||
            offset > entry->data_capacity ||
            len > entry->data_capacity - offset) {
            return -1;
        }
        return blkdev_read(g_blkdev_id,
                           g_base_offset + entry_data_offset(entry, (size_t)index) + offset,
                           buffer,
                           len);
    }

    if (offset > AOSFS_INLINE_CAPACITY ||
        len > AOSFS_INLINE_CAPACITY - offset) {
        return -1;
    }
    memory_data = memory_entry_data((size_t)index);
    if (!memory_data) return -1;
    local_memcpy(buffer, memory_data + offset, (size_t)len);
    return 0;
}

int aosfs_truncate_path(const char* path) {
    struct aosfs_entry* entry;
    uint8_t* memory_data;
    int index;
    if (select_instance_for_path(&path) != 0) {
        return -1;
    }
    entry = find_entry(path);
    index = entry_index(entry);
    if (!entry || entry->is_dir) {
        return -1;
    }
    entry->size = 0;
    memory_data = memory_entry_data((size_t)index);
    if (!g_block_backed && memory_data) {
        local_memset(memory_data, 0, AOSFS_INLINE_CAPACITY);
    }
    return sync_entry((size_t)index) == 0 &&
           sync_entry_data((size_t)index) == 0 ? 0 : -1;
}

int aosfs_unlink_path(const char* path) {
    struct aosfs_entry* entry;
    int index;

    if (!path || path[0] == '\0') {
        return -1;
    }
    if (select_instance_for_path(&path) != 0 || path[0] == '\0') {
        return -1;
    }

    entry = find_entry(path);
    index = entry_index(entry);
    if (!entry || entry->is_dir || index < 0) {
        return -1;
    }

    local_memset(entry, 0, sizeof(*entry));
    return sync_entry((size_t)index);
}

int aosfs_rmdir_path(const char* path) {
    struct aosfs_entry* entry;
    int index;

    if (!path || path[0] == '\0') {
        return -1;
    }
    if (select_instance_for_path(&path) != 0 || path[0] == '\0') {
        return -1;
    }
    if (root_dir_index(path) >= 0) {
        return -1;
    }

    entry = find_entry(path);
    index = entry_index(entry);
    if (!entry || !entry->is_dir || index < 0) {
        return -1;
    }
    if (dynamic_dir_has_children(path)) {
        return -1;
    }

    local_memset(entry, 0, sizeof(*entry));
    return sync_entry((size_t)index);
}

int aosfs_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size) {
    struct aosfs_entry* entry;
    uint8_t* memory_data;
    int index;
    uint64_t end;

    if (written) *written = 0;
    if (select_instance_for_path(&path) != 0) {
        return -1;
    }
    entry = find_entry(path);
    index = entry_index(entry);
    if (!entry || entry->is_dir || index < 0 || (!buffer && len != 0) ||
        offset > AOSFS_MAX_FILE_SIZE || len > AOSFS_MAX_FILE_SIZE - offset) {
        return -1;
    }
    end = offset + len;
    if (len == 0) {
        if (new_size) *new_size = entry->size;
        return 0;
    }

    if (g_block_backed && g_format_version >= AOSFS_VERSION) {
        if (reserve_entry_extent(entry, (size_t)index, end) != 0 ||
            blkdev_write(g_blkdev_id,
                         g_base_offset + entry_data_offset(entry, (size_t)index) + offset,
                         buffer,
                         len) != 0) {
            return -1;
        }
    } else {
        if (offset > AOSFS_INLINE_CAPACITY ||
            len > AOSFS_INLINE_CAPACITY - offset) {
            return -1;
        }
        memory_data = memory_entry_data((size_t)index);
        if (!memory_data) return -1;
        local_memcpy(memory_data + offset, buffer, (size_t)len);
    }
    if (end > entry->size) {
        entry->size = (uint32_t)end;
    }
    if (sync_entry((size_t)index) != 0 || sync_entry_data((size_t)index) != 0) {
        return -1;
    }
    if (written) *written = len;
    if (new_size) *new_size = entry->size;
    return 0;
}

int aosfs_dirent_at_index(const char* path, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    uint32_t entry_size = 0;
    uint64_t root_dir_count = sizeof(g_root_dirs) / sizeof(g_root_dirs[0]);
    uint64_t seen = 0;

    if (!path || !name_buf || name_buf_size == 0) {
        return -1;
    }
    if (select_instance_for_path(&path) != 0) {
        return -1;
    }

    if (index == 0) {
        copy_string(name_buf, name_buf_size, ".");
        if (size) *size = 0;
        if (d_type) *d_type = LINUX_DTYPE_DIR;
        return 0;
    }
    if (index == 1) {
        copy_string(name_buf, name_buf_size, "..");
        if (size) *size = 0;
        if (d_type) *d_type = LINUX_DTYPE_DIR;
        return 0;
    }

    index -= 2;

    if (path[0] == '\0') {
        if (index < root_dir_count) {
            copy_string(name_buf, name_buf_size, g_root_dirs[index]);
            if (size) *size = 0;
            if (d_type) *d_type = LINUX_DTYPE_DIR;
            return 0;
        }
        index -= root_dir_count;

        for (size_t i = 0; i < g_entry_limit; i++) {
            const char* child = NULL;
            size_t child_len = 0;
            if (!g_entries[i].in_use || !immediate_child_name("", g_entries[i].path, &child, &child_len)) {
                continue;
            }
            if (root_name_is_builtin(child)) {
                continue;
            }
            if (seen == index) {
                if (child_len >= name_buf_size) {
                    return -1;
                }
                local_memcpy(name_buf, child, child_len);
                name_buf[child_len] = '\0';
                if (size) *size = g_entries[i].size;
                if (d_type) *d_type = g_entries[i].is_dir ? LINUX_DTYPE_DIR : LINUX_DTYPE_REG;
                return 0;
            }
            seen++;
        }
        index -= seen;

        for (uint64_t initrd_index = 0;; initrd_index++) {
            if (initrd_get_entry(initrd_index, name_buf, name_buf_size, &entry_size) != 0) {
                return -1;
            }
            if (root_name_is_builtin(name_buf) || dynamic_child_seen("", name_buf)) {
                continue;
            }
            if (index == 0) {
                if (size) *size = entry_size;
                if (d_type) *d_type = LINUX_DTYPE_REG;
                return 0;
            }
            index--;
        }
    }

    seen = 0;
    for (size_t i = 0; i < g_entry_limit; i++) {
        const char* child = NULL;
        size_t child_len = 0;
        if (!g_entries[i].in_use || !immediate_child_name(path, g_entries[i].path, &child, &child_len)) {
            continue;
        }
        if (seen == index) {
            if (child_len >= name_buf_size) {
                return -1;
            }
            local_memcpy(name_buf, child, child_len);
            name_buf[child_len] = '\0';
            if (size) *size = g_entries[i].size;
            if (d_type) *d_type = g_entries[i].is_dir ? LINUX_DTYPE_DIR : LINUX_DTYPE_REG;
            return 0;
        }
        seen++;
    }
    index -= seen;

    if (path_equals(path, "commands")) {
        if (initrd_get_entry(index, name_buf, name_buf_size, &entry_size) != 0) {
            return -1;
        }
        if (size) *size = entry_size;
        if (d_type) *d_type = LINUX_DTYPE_REG;
        return 0;
    }

    return -1;
}
