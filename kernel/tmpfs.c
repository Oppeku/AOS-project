/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <tmpfs.h>
#include <stdint.h>
#include <stddef.h>

#define TMPFS_MAX_FILES 32
#define TMPFS_NAME_MAX 64
#define TMPFS_FILE_CAPACITY 4096
#define TMPFS_ROOT_INO 0x544D5001ULL

struct tmpfs_entry {
    uint8_t in_use;
    uint8_t reserved[7];
    uint32_t size;
    uint32_t reserved2;
    uint64_t inode;
    char name[TMPFS_NAME_MAX];
    uint8_t data[TMPFS_FILE_CAPACITY];
};

static struct tmpfs_entry g_tmpfs_entries[TMPFS_MAX_FILES];
static uint64_t g_tmpfs_next_inode = TMPFS_ROOT_INO + 1;

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

static void copy_name(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst || dst_size == 0) {
        return;
    }
    while (src && src[i] != '\0' && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static const char* skip_leading_slashes(const char* path) {
    while (path && *path == '/') {
        path++;
    }
    return path ? path : "";
}

static int path_equals(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

static int extract_tmpfs_leaf(const char* path, char* leaf, size_t leaf_size) {
    const char* normalized = skip_leading_slashes(path);
    size_t i = 0;

    if (path_equals(normalized, "tmp") || normalized[0] == '\0') {
        if (leaf_size > 0) {
            leaf[0] = '\0';
        }
        return 0;
    }

    if (normalized[0] != 't' || normalized[1] != 'm' || normalized[2] != 'p') {
        return -1;
    }
    normalized += 3;
    if (*normalized == '/') {
        normalized++;
    }
    if (*normalized == '\0') {
        if (leaf_size > 0) {
            leaf[0] = '\0';
        }
        return 0;
    }

    while (normalized[i] != '\0' && normalized[i] != '/') {
        if (i + 1 >= leaf_size) {
            return -1;
        }
        leaf[i] = normalized[i];
        i++;
    }
    if (normalized[i] == '/') {
        return -1;
    }
    leaf[i] = '\0';
    return 0;
}

static struct tmpfs_entry* find_entry(const char* leaf) {
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
        if (!g_tmpfs_entries[i].in_use) {
            continue;
        }
        if (path_equals(g_tmpfs_entries[i].name, leaf)) {
            return &g_tmpfs_entries[i];
        }
    }
    return NULL;
}

static struct tmpfs_entry* allocate_entry(const char* leaf) {
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
        if (g_tmpfs_entries[i].in_use) {
            continue;
        }
        local_memset(&g_tmpfs_entries[i], 0, sizeof(g_tmpfs_entries[i]));
        g_tmpfs_entries[i].in_use = 1;
        g_tmpfs_entries[i].inode = g_tmpfs_next_inode++;
        copy_name(g_tmpfs_entries[i].name, sizeof(g_tmpfs_entries[i].name), leaf);
        return &g_tmpfs_entries[i];
    }
    return NULL;
}

static void fill_root_node(struct vfs_node* out) {
    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_DIRECTORY;
    out->backend = VFS_BACKEND_TMPFS;
    out->inode = TMPFS_ROOT_INO;
    copy_name(out->path, sizeof(out->path), "tmp");
}

static void fill_file_node(const struct tmpfs_entry* entry, const char* leaf, struct vfs_node* out) {
    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_TMPFS;
    out->size = entry->size;
    out->inode = entry->inode;
    out->u.data = entry->data;
    copy_name(out->path, sizeof(out->path), "tmp/");
    copy_name(out->path + 4, sizeof(out->path) - 4, leaf);
}

void tmpfs_init(void) {
    local_memset(g_tmpfs_entries, 0, sizeof(g_tmpfs_entries));
    g_tmpfs_next_inode = TMPFS_ROOT_INO + 1;
}

int tmpfs_lookup_path(const char* path, struct vfs_node* out) {
    char leaf[TMPFS_NAME_MAX];
    struct tmpfs_entry* entry = NULL;

    if (!path || !out) {
        return -1;
    }
    if (extract_tmpfs_leaf(path, leaf, sizeof(leaf)) != 0) {
        return -1;
    }
    if (leaf[0] == '\0') {
        fill_root_node(out);
        return 0;
    }

    entry = find_entry(leaf);
    if (!entry) {
        return -1;
    }

    fill_file_node(entry, leaf, out);
    return 0;
}

int tmpfs_create_path(const char* path, struct vfs_node* out) {
    char leaf[TMPFS_NAME_MAX];
    struct tmpfs_entry* entry = NULL;

    if (!path || !out) {
        return -1;
    }
    if (extract_tmpfs_leaf(path, leaf, sizeof(leaf)) != 0 || leaf[0] == '\0') {
        return -1;
    }

    entry = find_entry(leaf);
    if (!entry) {
        entry = allocate_entry(leaf);
    }
    if (!entry) {
        return -1;
    }

    fill_file_node(entry, leaf, out);
    return 0;
}

int tmpfs_truncate_path(const char* path) {
    char leaf[TMPFS_NAME_MAX];
    struct tmpfs_entry* entry = NULL;

    if (!path) {
        return -1;
    }
    if (extract_tmpfs_leaf(path, leaf, sizeof(leaf)) != 0 || leaf[0] == '\0') {
        return -1;
    }
    entry = find_entry(leaf);
    if (!entry) {
        return -1;
    }
    entry->size = 0;
    return 0;
}

int tmpfs_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size) {
    char leaf[TMPFS_NAME_MAX];
    struct tmpfs_entry* entry = NULL;
    uint64_t capacity = TMPFS_FILE_CAPACITY;
    uint64_t to_write = len;

    if (!path || !buffer || !written) {
        return -1;
    }
    *written = 0;

    if (extract_tmpfs_leaf(path, leaf, sizeof(leaf)) != 0 || leaf[0] == '\0') {
        return -1;
    }
    entry = find_entry(leaf);
    if (!entry) {
        return -1;
    }
    if (offset >= capacity) {
        return 0;
    }
    if (offset + to_write > capacity) {
        to_write = capacity - offset;
    }

    local_memcpy(entry->data + offset, buffer, (size_t)to_write);
    *written = to_write;
    if (offset + to_write > entry->size) {
        entry->size = (uint32_t)(offset + to_write);
    }
    if (new_size) {
        *new_size = entry->size;
    }
    return 0;
}

int tmpfs_dirent_at_index(uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    uint64_t current = 0;

    if (!name_buf || name_buf_size == 0) {
        return -1;
    }

    if (index == 0) {
        copy_name(name_buf, name_buf_size, ".");
        if (size) *size = 0;
        if (d_type) *d_type = 4;
        return 0;
    }
    if (index == 1) {
        copy_name(name_buf, name_buf_size, "..");
        if (size) *size = 0;
        if (d_type) *d_type = 4;
        return 0;
    }

    index -= 2;
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
        if (!g_tmpfs_entries[i].in_use) {
            continue;
        }
        if (current == index) {
            copy_name(name_buf, name_buf_size, g_tmpfs_entries[i].name);
            if (size) *size = g_tmpfs_entries[i].size;
            if (d_type) *d_type = 8;
            return 0;
        }
        current++;
    }

    return -1;
}
