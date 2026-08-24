/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <tmpfs.h>
#include <stdint.h>
#include <stddef.h>

#define TMPFS_MAX_ENTRIES 64
#define TMPFS_PATH_MAX 256
#define TMPFS_FILE_CAPACITY 4096
#define TMPFS_ROOT_INO 0x544D5001ULL

struct tmpfs_entry {
    uint8_t in_use;
    uint8_t type;
    uint16_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint64_t inode;
    char path[TMPFS_PATH_MAX];
    uint8_t data[TMPFS_FILE_CAPACITY];
};

static struct tmpfs_entry g_tmpfs_entries[TMPFS_MAX_ENTRIES];
static uint64_t g_tmpfs_next_inode = TMPFS_ROOT_INO + 1;

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) *d++ = (uint8_t)value;
}

static void local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}

static size_t local_strlen(const char* text) {
    size_t length = 0;
    while (text && text[length]) length++;
    return length;
}

static void copy_string(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) return;
    while (src && src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int path_equals(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int normalize_tmpfs_path(const char* path, char* out,
                                size_t out_size) {
    size_t length;

    if (!path || !out || out_size == 0) return -1;
    while (*path == '/') path++;
    length = local_strlen(path);
    while (length > 3 && path[length - 1] == '/') length--;
    if (length < 3 || path[0] != 't' || path[1] != 'm' ||
        path[2] != 'p' || (length > 3 && path[3] != '/') ||
        length >= out_size) {
        return -1;
    }
    for (size_t i = 0; i < length; i++) out[i] = path[i];
    out[length] = '\0';
    return 0;
}

static int parent_path_of(const char* path, char* parent,
                          size_t parent_size) {
    size_t length = local_strlen(path);
    size_t slash = length;

    if (!parent || parent_size == 0 || length <= 3) return -1;
    while (slash > 0 && path[slash - 1] != '/') slash--;
    if (slash <= 3 || slash > parent_size) return -1;
    for (size_t i = 0; i + 1 < slash; i++) parent[i] = path[i];
    parent[slash - 1] = '\0';
    return 0;
}

static struct tmpfs_entry* find_entry(const char* path) {
    for (size_t i = 0; i < TMPFS_MAX_ENTRIES; i++) {
        if (g_tmpfs_entries[i].in_use &&
            path_equals(g_tmpfs_entries[i].path, path)) {
            return &g_tmpfs_entries[i];
        }
    }
    return NULL;
}

static int parent_directory_exists(const char* path) {
    char parent[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (parent_path_of(path, parent, sizeof(parent)) != 0) return 0;
    if (path_equals(parent, "tmp")) return 1;
    entry = find_entry(parent);
    return entry && entry->type == VFS_NODE_TYPE_DIRECTORY;
}

static struct tmpfs_entry* allocate_entry(const char* path, uint8_t type,
                                           uint16_t mode, uint32_t uid,
                                           uint32_t gid) {
    for (size_t i = 0; i < TMPFS_MAX_ENTRIES; i++) {
        struct tmpfs_entry* entry = &g_tmpfs_entries[i];

        if (entry->in_use) continue;
        local_memset(entry, 0, sizeof(*entry));
        entry->in_use = 1;
        entry->type = type;
        entry->mode = mode & 07777U;
        entry->uid = uid;
        entry->gid = gid;
        entry->inode = g_tmpfs_next_inode++;
        copy_string(entry->path, sizeof(entry->path), path);
        return entry;
    }
    return NULL;
}

static void fill_root_node(struct vfs_node* out) {
    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_DIRECTORY;
    out->backend = VFS_BACKEND_TMPFS;
    out->mode = 01777U;
    out->inode = TMPFS_ROOT_INO;
    copy_string(out->path, sizeof(out->path), "tmp");
}

static void fill_entry_node(const struct tmpfs_entry* entry,
                            struct vfs_node* out) {
    local_memset(out, 0, sizeof(*out));
    out->type = entry->type;
    out->backend = VFS_BACKEND_TMPFS;
    out->mode = entry->mode;
    out->uid = entry->uid;
    out->gid = entry->gid;
    out->size = entry->size;
    out->inode = entry->inode;
    if (entry->type == VFS_NODE_TYPE_REGULAR) out->u.data = entry->data;
    copy_string(out->path, sizeof(out->path), entry->path);
}

static int is_descendant(const char* directory, const char* path) {
    size_t directory_length = local_strlen(directory);

    for (size_t i = 0; i < directory_length; i++) {
        if (path[i] != directory[i]) return 0;
    }
    return path[directory_length] == '/';
}

static int direct_child_name(const char* directory, const char* path,
                             const char** name, size_t* name_length) {
    size_t directory_length = local_strlen(directory);
    const char* child;
    size_t length = 0;

    if (!is_descendant(directory, path)) return 0;
    child = path + directory_length + 1;
    while (child[length] && child[length] != '/') length++;
    if (length == 0 || child[length] != '\0') return 0;
    *name = child;
    *name_length = length;
    return 1;
}

void tmpfs_init(void) {
    local_memset(g_tmpfs_entries, 0, sizeof(g_tmpfs_entries));
    g_tmpfs_next_inode = TMPFS_ROOT_INO + 1;
}

int tmpfs_lookup_path(const char* path, struct vfs_node* out) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (!out || normalize_tmpfs_path(path, normalized,
                                     sizeof(normalized)) != 0) {
        return -1;
    }
    if (path_equals(normalized, "tmp")) {
        fill_root_node(out);
        return 0;
    }
    entry = find_entry(normalized);
    if (!entry) return -1;
    fill_entry_node(entry, out);
    return 0;
}

int tmpfs_create_path_mode(const char* path, uint16_t mode,
                           uint32_t uid, uint32_t gid,
                           struct vfs_node* out) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (!out || normalize_tmpfs_path(path, normalized,
                                     sizeof(normalized)) != 0 ||
        path_equals(normalized, "tmp") ||
        !parent_directory_exists(normalized)) {
        return -1;
    }
    entry = find_entry(normalized);
    if (entry) {
        if (entry->type != VFS_NODE_TYPE_REGULAR) return -1;
        fill_entry_node(entry, out);
        return 0;
    }
    entry = allocate_entry(normalized, VFS_NODE_TYPE_REGULAR,
                           mode, uid, gid);
    if (!entry) return -1;
    fill_entry_node(entry, out);
    return 0;
}

int tmpfs_create_path(const char* path, struct vfs_node* out) {
    return tmpfs_create_path_mode(path, 0600U, 0, 0, out);
}

int tmpfs_mkdir_path_mode(const char* path, uint16_t mode,
                          uint32_t uid, uint32_t gid) {
    char normalized[TMPFS_PATH_MAX];

    if (normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0 ||
        path_equals(normalized, "tmp") || find_entry(normalized) ||
        !parent_directory_exists(normalized)) {
        return -1;
    }
    return allocate_entry(normalized, VFS_NODE_TYPE_DIRECTORY,
                          mode, uid, gid) ? 0 : -1;
}

int tmpfs_truncate_path(const char* path) {
    return tmpfs_resize_path(path, 0);
}

int tmpfs_resize_path(const char* path, uint32_t size) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (size > TMPFS_FILE_CAPACITY ||
        normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    entry = find_entry(normalized);
    if (!entry || entry->type != VFS_NODE_TYPE_REGULAR) return -1;
    if (size > entry->size) {
        local_memset(entry->data + entry->size, 0, size - entry->size);
    }
    entry->size = size;
    return 0;
}

int tmpfs_read_path(const char* path, uint64_t offset, uint8_t* buffer,
                    uint64_t len) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (!buffer ||
        normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    entry = find_entry(normalized);
    if (!entry || entry->type != VFS_NODE_TYPE_REGULAR ||
        offset > entry->size || len > entry->size - offset) {
        return -1;
    }
    local_memcpy(buffer, entry->data + offset, (size_t)len);
    return 0;
}

int tmpfs_write_path(const char* path, uint64_t offset,
                     const uint8_t* buffer, uint64_t len,
                     uint64_t* written, uint32_t* new_size) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;
    uint64_t to_write = len;

    if (!buffer || !written ||
        normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    *written = 0;
    entry = find_entry(normalized);
    if (!entry || entry->type != VFS_NODE_TYPE_REGULAR) return -1;
    if (offset >= TMPFS_FILE_CAPACITY) return 0;
    if (offset + to_write > TMPFS_FILE_CAPACITY) {
        to_write = TMPFS_FILE_CAPACITY - offset;
    }
    local_memcpy(entry->data + offset, buffer, (size_t)to_write);
    *written = to_write;
    if (offset + to_write > entry->size) {
        entry->size = (uint32_t)(offset + to_write);
    }
    if (new_size) *new_size = entry->size;
    return 0;
}

int tmpfs_unlink_path(const char* path) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    entry = find_entry(normalized);
    if (!entry || entry->type != VFS_NODE_TYPE_REGULAR) return -1;
    local_memset(entry, 0, sizeof(*entry));
    return 0;
}

int tmpfs_rmdir_path(const char* path) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0 ||
        path_equals(normalized, "tmp")) {
        return -1;
    }
    entry = find_entry(normalized);
    if (!entry || entry->type != VFS_NODE_TYPE_DIRECTORY) return -1;
    for (size_t i = 0; i < TMPFS_MAX_ENTRIES; i++) {
        if (g_tmpfs_entries[i].in_use &&
            is_descendant(normalized, g_tmpfs_entries[i].path)) {
            return -1;
        }
    }
    local_memset(entry, 0, sizeof(*entry));
    return 0;
}

int tmpfs_chmod_path(const char* path, uint16_t mode) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    entry = find_entry(normalized);
    if (!entry) return -1;
    entry->mode = mode & 07777U;
    return 0;
}

int tmpfs_chown_path(const char* path, uint32_t uid, uint32_t gid) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* entry;

    if (normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    entry = find_entry(normalized);
    if (!entry) return -1;
    entry->uid = uid;
    entry->gid = gid;
    return 0;
}

int tmpfs_dirent_at_index(const char* path, uint64_t index,
                          char* name_buf, size_t name_buf_size,
                          uint32_t* size, uint8_t* d_type) {
    char normalized[TMPFS_PATH_MAX];
    struct tmpfs_entry* directory;
    uint64_t current = 0;

    if (!name_buf || name_buf_size == 0 ||
        normalize_tmpfs_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }
    if (!path_equals(normalized, "tmp")) {
        directory = find_entry(normalized);
        if (!directory || directory->type != VFS_NODE_TYPE_DIRECTORY) {
            return -1;
        }
    }
    if (index == 0 || index == 1) {
        copy_string(name_buf, name_buf_size, index == 0 ? "." : "..");
        if (size) *size = 0;
        if (d_type) *d_type = 4;
        return 0;
    }
    index -= 2;
    for (size_t i = 0; i < TMPFS_MAX_ENTRIES; i++) {
        const char* name;
        size_t name_length;

        if (!g_tmpfs_entries[i].in_use ||
            !direct_child_name(normalized, g_tmpfs_entries[i].path,
                               &name, &name_length)) {
            continue;
        }
        if (current++ != index) continue;
        if (name_length >= name_buf_size) return -1;
        for (size_t j = 0; j < name_length; j++) name_buf[j] = name[j];
        name_buf[name_length] = '\0';
        if (size) *size = g_tmpfs_entries[i].size;
        if (d_type) {
            *d_type = g_tmpfs_entries[i].type == VFS_NODE_TYPE_DIRECTORY
                          ? 4 : 8;
        }
        return 0;
    }
    return -1;
}
