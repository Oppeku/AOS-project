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
#define AOSFS_MAX_ENTRIES 64
#define AOSFS_PATH_MAX 128
#define AOSFS_FILE_CAPACITY 8192
#define AOSFS_MAGIC0 0x46534F41U
#define AOSFS_MAGIC1 0x00003153U
#define AOSFS_VERSION 3U
#define AOSFS_BLOCK_SIZE 512U
#define AOSFS_SUPER_OFFSET 0U
#define AOSFS_TABLE_OFFSET 4096U
#define AOSFS_DATA_OFFSET 65536U

#define LINUX_DTYPE_REG 8
#define LINUX_DTYPE_DIR 4

static const char* g_root_dirs[] = {
    "aos",
    "boot",
    "commands",
    "configs",
    "drivers",
    "etc",
    "kernel",
    "logs",
    "main",
    "memory",
    "mnt",
    "MUI",
    "runtime",
    "root",
    "sqausased",
    "sudo",
    "squashed",
    "tmp",
    "trash",
};

struct aosfs_entry {
    uint8_t in_use;
    uint8_t is_dir;
    uint32_t size;
    uint64_t inode;
    char path[AOSFS_PATH_MAX];
    uint8_t data[AOSFS_FILE_CAPACITY];
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
    uint8_t reserved1[112];
};

struct aosfs_instance {
    uint8_t mounted;
    uint8_t role;
    uint8_t block_backed;
    uint8_t reserved;
    uint32_t blkdev_id;
    uint64_t base_offset;
    uint64_t next_inode;
    struct aosfs_entry entries[AOSFS_MAX_ENTRIES];
};

static struct aosfs_instance g_instances[PARTITION_ROLE_TRASH + 1];
static struct aosfs_instance* g_active;

#define g_entries (g_active->entries)
#define g_next_inode (g_active->next_inode)
#define g_blkdev_id (g_active->blkdev_id)
#define g_base_offset (g_active->base_offset)
#define g_block_backed (g_active->block_backed)

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
    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
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
    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
        if (&g_entries[i] == entry) {
            return (int)i;
        }
    }
    return -1;
}

static uint64_t entry_data_offset(size_t index) {
    return g_base_offset + AOSFS_DATA_OFFSET + (uint64_t)index * AOSFS_FILE_CAPACITY;
}

static void fill_superblock(struct aosfs_superblock* super) {
    local_memset(super, 0, sizeof(*super));
    super->magic0 = AOSFS_MAGIC0;
    super->magic1 = AOSFS_MAGIC1;
    super->version = AOSFS_VERSION;
    super->block_size = AOSFS_BLOCK_SIZE;
    super->max_entries = AOSFS_MAX_ENTRIES;
    super->path_max = AOSFS_PATH_MAX;
    super->file_capacity = AOSFS_FILE_CAPACITY;
    super->table_offset = AOSFS_TABLE_OFFSET;
    super->data_offset = AOSFS_DATA_OFFSET;
}

static int superblock_valid(const struct aosfs_superblock* super) {
    return super &&
           super->magic0 == AOSFS_MAGIC0 &&
           super->magic1 == AOSFS_MAGIC1 &&
           super->version == AOSFS_VERSION &&
           super->max_entries == AOSFS_MAX_ENTRIES &&
           super->path_max == AOSFS_PATH_MAX &&
           super->file_capacity == AOSFS_FILE_CAPACITY;
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
    if (!g_block_backed || index >= AOSFS_MAX_ENTRIES) {
        return 0;
    }
    local_memset(&disk_entry, 0, sizeof(disk_entry));
    disk_entry.in_use = g_entries[index].in_use;
    disk_entry.is_dir = g_entries[index].is_dir;
    disk_entry.size = g_entries[index].size;
    disk_entry.inode = g_entries[index].inode;
    copy_string(disk_entry.path, sizeof(disk_entry.path), g_entries[index].path);
    return blkdev_write(g_blkdev_id, g_base_offset + AOSFS_TABLE_OFFSET + index * sizeof(disk_entry), &disk_entry, sizeof(disk_entry));
}

static int sync_entry_data(size_t index) {
    if (!g_block_backed || index >= AOSFS_MAX_ENTRIES || g_entries[index].is_dir) {
        return 0;
    }
    return blkdev_write(g_blkdev_id, entry_data_offset(index), g_entries[index].data, AOSFS_FILE_CAPACITY);
}

static int dynamic_child_seen(const char* parent, const char* name) {
    const char* child = NULL;
    size_t child_len = 0;
    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
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

    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
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

    local_memset(out, 0, sizeof(*out));
    out->type = entry->is_dir ? VFS_NODE_TYPE_DIRECTORY : VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_AOSFS;
    out->size = entry->size;
    out->inode = entry->inode;
    out->u.data = entry->data;
    make_node_path(entry->path, node_path, sizeof(node_path));
    copy_string(out->path, sizeof(out->path), node_path);
}

void aosfs_init(void) {
    local_memset(g_instances, 0, sizeof(g_instances));
    for (size_t i = 0; i < sizeof(g_instances) / sizeof(g_instances[0]); i++) {
        g_instances[i].role = (uint8_t)i;
        g_instances[i].blkdev_id = BLKDEV_INVALID_ID;
        g_instances[i].next_inode = AOSFS_DYNAMIC_INO_BASE;
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

    if (blkdev_get(blkdev_id) == NULL) {
        return -1;
    }
    g_active = instance_for_role(role);
    g_blkdev_id = blkdev_id;
    g_base_offset = base_offset;
    g_block_backed = 1;
    g_active->mounted = 1;

    local_memset(&super, 0, sizeof(super));
    if (blkdev_read(g_blkdev_id, g_base_offset + AOSFS_SUPER_OFFSET, &super, sizeof(super)) != 0) {
        return -1;
    }
    if (!superblock_valid(&super)) {
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
    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
        struct aosfs_disk_entry disk_entry;
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
        if (g_entries[i].size > AOSFS_FILE_CAPACITY) {
            g_entries[i].size = AOSFS_FILE_CAPACITY;
        }
        g_entries[i].inode = disk_entry.inode;
        copy_string(g_entries[i].path, sizeof(g_entries[i].path), disk_entry.path);
        if (!g_entries[i].is_dir &&
            blkdev_read(g_blkdev_id, entry_data_offset(i), g_entries[i].data, AOSFS_FILE_CAPACITY) != 0) {
            return -1;
        }
        if (g_entries[i].inode > highest_inode) {
            highest_inode = g_entries[i].inode;
        }
    }
    g_next_inode = highest_inode + 1;
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

    if (!contains_slash(path) && initrd_get_file(path, &data, &size) == 0) {
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

    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
        if (!g_entries[i].in_use) {
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

    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
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

int aosfs_truncate_path(const char* path) {
    struct aosfs_entry* entry;
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
    local_memset(entry->data, 0, sizeof(entry->data));
    return sync_entry((size_t)index) == 0 && sync_entry_data((size_t)index) == 0 ? 0 : -1;
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
    int index;
    uint64_t space;

    if (written) *written = 0;
    if (select_instance_for_path(&path) != 0) {
        return -1;
    }
    entry = find_entry(path);
    index = entry_index(entry);
    if (!entry || entry->is_dir || !buffer || offset >= AOSFS_FILE_CAPACITY) {
        return -1;
    }

    space = AOSFS_FILE_CAPACITY - offset;
    if (len > space) {
        len = space;
    }
    local_memcpy(entry->data + offset, buffer, (size_t)len);
    if (offset + len > entry->size) {
        entry->size = (uint32_t)(offset + len);
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

        for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
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
    for (size_t i = 0; i < AOSFS_MAX_ENTRIES; i++) {
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
