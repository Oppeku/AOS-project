/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <vfs.h>
#include <aosfs.h>
#include <cpio.h>
#include <ext4.h>
#include <fat32.h>
#include <tmpfs.h>

#define MAX_VFS_PATH 256
#define VFS_SYNTH_ROOT_INO 1
#define VFS_SYNTH_MNT_INO 2
#define VFS_TMP_ROOT_INO 3
#define VFS_SYNTH_DYNAMIC_DIR_BASE 0x2000

#define LINUX_R_OK 4
#define LINUX_W_OK 2
#define LINUX_X_OK 1

#define LINUX_O_ACCMODE 3
#define LINUX_O_WRONLY 1
#define LINUX_O_RDWR 2
#define LINUX_O_CREAT 64
#define LINUX_O_EXCL 128
#define LINUX_O_TRUNC 512
#define LINUX_O_DIRECTORY 0x10000

#define LINUX_DTYPE_REG 8
#define LINUX_DTYPE_DIR 4
#define LINUX_DTYPE_CHR 2

struct vfs_mount {
    uint8_t in_use;
    uint8_t backend;
    uint16_t reserved;
    char path[MAX_VFS_PATH];
    char backend_root[MAX_VFS_PATH];
};

static struct vfs_mount g_mounts[VFS_MAX_MOUNTS];

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

static void copy_path_string(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int path_equals(const char* path, const char* target) {
    size_t i = 0;
    while (path[i] != '\0' && target[i] != '\0') {
        if (path[i] != target[i]) {
            return 0;
        }
        i++;
    }
    return path[i] == '\0' && target[i] == '\0';
}

static int component_is_dot(const char* s, size_t len) {
    return len == 1 && s[0] == '.';
}

static int component_is_dotdot(const char* s, size_t len) {
    return len == 2 && s[0] == '.' && s[1] == '.';
}

static void pop_path_component(char* out, size_t* len) {
    while (*len > 0 && out[*len - 1] != '/') {
        (*len)--;
    }
    if (*len > 0 && out[*len - 1] == '/') {
        (*len)--;
    }
    out[*len] = '\0';
}

static int append_path_component(char* out, size_t out_size, size_t* len, const char* component, size_t component_len) {
    if (*len != 0) {
        if (*len + 1 >= out_size) {
            return -1;
        }
        out[*len] = '/';
        (*len)++;
    }
    if (*len + component_len >= out_size) {
        return -1;
    }
    for (size_t i = 0; i < component_len; i++) {
        out[*len + i] = component[i];
    }
    *len += component_len;
    out[*len] = '\0';
    return 0;
}

static int normalize_path(const char* path, char* out, size_t out_size) {
    const char* cursor = path;
    size_t len = 0;

    if (!path || !out || out_size == 0) {
        return -1;
    }

    out[0] = '\0';
    while (*cursor) {
        size_t component_len = 0;

        while (*cursor == '/') {
            cursor++;
        }
        while (cursor[component_len] && cursor[component_len] != '/') {
            component_len++;
        }
        if (component_len == 0) {
            break;
        }

        if (component_is_dot(cursor, component_len)) {
            /* Stay on the current path. */
        } else if (component_is_dotdot(cursor, component_len)) {
            pop_path_component(out, &len);
        } else if (append_path_component(out, out_size, &len, cursor, component_len) != 0) {
            return -1;
        }

        cursor += component_len;
    }

    return 0;
}

static int path_starts_with_component(const char* path, const char* prefix) {
    size_t i = 0;
    if (prefix[0] == '\0') {
        return 1;
    }
    while (prefix[i] != '\0') {
        if (path[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static size_t local_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len] != '\0') {
        len++;
    }
    return len;
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

static int immediate_mount_child(const char* parent, const char* mount_path, const char** child, size_t* child_len) {
    size_t parent_len = local_strlen(parent);
    const char* rest;
    size_t len = 0;

    if (!parent || !mount_path || !child || !child_len) {
        return 0;
    }

    if (parent_len == 0) {
        rest = mount_path;
    } else {
        if (!path_starts_with_component(mount_path, parent)) {
            return 0;
        }
        rest = mount_path + parent_len;
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
    if (len == 0) {
        return 0;
    }
    *child = rest;
    *child_len = len;
    return 1;
}

static int mount_child_seen_before(const char* parent, size_t mount_index, const char* child, size_t child_len) {
    for (size_t i = 0; i < mount_index; i++) {
        const char* earlier_child = NULL;
        size_t earlier_len = 0;
        if (!g_mounts[i].in_use) {
            continue;
        }
        if (!immediate_mount_child(parent, g_mounts[i].path, &earlier_child, &earlier_len)) {
            continue;
        }
        if (earlier_len == child_len) {
            size_t j = 0;
            while (j < child_len && earlier_child[j] == child[j]) {
                j++;
            }
            if (j == child_len) {
                return 1;
            }
        }
    }
    return 0;
}

static int dirent_mount_child_at(const char* parent, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    uint64_t seen = 0;

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        const char* child = NULL;
        size_t child_len = 0;
        size_t j;

        if (!g_mounts[i].in_use) {
            continue;
        }
        if (!immediate_mount_child(parent, g_mounts[i].path, &child, &child_len)) {
            continue;
        }
        if (mount_child_seen_before(parent, i, child, child_len)) {
            continue;
        }
        if (seen == index) {
            if (!name_buf || name_buf_size == 0 || child_len >= name_buf_size) {
                return -1;
            }
            for (j = 0; j < child_len; j++) {
                name_buf[j] = child[j];
            }
            name_buf[j] = '\0';
            if (size) *size = 0;
            if (d_type) *d_type = LINUX_DTYPE_DIR;
            return 0;
        }
        seen++;
    }

    return -1;
}

static uint64_t mount_child_count(const char* parent) {
    uint64_t count = 0;
    char tmp_name[64];

    while (dirent_mount_child_at(parent, count, tmp_name, sizeof(tmp_name), NULL, NULL) == 0) {
        count++;
    }
    return count;
}

static int synthetic_dir_exists(const char* normalized) {
    char parent[MAX_VFS_PATH];

    if (!normalized) {
        return 0;
    }
    if (*normalized == '\0' || path_equals(normalized, "mnt") || path_equals(normalized, "dev")) {
        return 1;
    }
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].in_use) {
            continue;
        }
        if (parent_path_of(g_mounts[i].path, parent, sizeof(parent)) == 0 &&
            path_equals(parent, normalized)) {
            return 1;
        }
    }
    return 0;
}

static int make_mount_backend_path(const char* normalized, const char* mount_path, const char* backend_root, char* out, size_t out_size) {
    size_t mount_len = 0;
    size_t backend_len = 0;
    const char* rest = NULL;

    if (!normalized || !out || out_size == 0) {
        return -1;
    }

    if (!path_starts_with_component(normalized, mount_path)) {
        return -1;
    }

    while (mount_path[mount_len]) {
        mount_len++;
    }
    while (backend_root[backend_len]) {
        backend_len++;
    }

    rest = normalized + mount_len;
    while (*rest == '/') {
        rest++;
    }

    if (*rest == '\0') {
        copy_path_string(out, out_size, backend_root);
        return 0;
    }

    if (backend_len == 0) {
        copy_path_string(out, out_size, rest);
        return 0;
    }

    if (backend_len + 1 >= out_size) {
        return -1;
    }

    copy_path_string(out, out_size, backend_root);
    out[backend_len] = '/';
    copy_path_string(out + backend_len + 1, out_size - backend_len - 1, rest);
    return 0;
}

static const struct vfs_mount* find_mount_for_path(const char* normalized, char* backend_path, size_t backend_path_size) {
    const struct vfs_mount* best = NULL;
    size_t best_len = 0;

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        size_t mount_len;
        if (!g_mounts[i].in_use) {
            continue;
        }
        if (!path_starts_with_component(normalized, g_mounts[i].path)) {
            continue;
        }
        mount_len = local_strlen(g_mounts[i].path);
        if (!best || mount_len > best_len) {
            best = &g_mounts[i];
            best_len = mount_len;
        }
    }

    if (!best || make_mount_backend_path(normalized, best->path, best->backend_root, backend_path, backend_path_size) != 0) {
        return NULL;
    }
    return best;
}

static void fill_synthetic_dir(struct vfs_node* out, uint64_t inode, const char* path) {
    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_DIRECTORY;
    out->backend = VFS_BACKEND_SYNTHETIC;
    out->inode = inode;
    copy_path_string(out->path, sizeof(out->path), path);
}

static void fill_synthetic_char_device(struct vfs_node* out, uint64_t device_id, const char* path) {
    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_CHAR_DEVICE;
    out->backend = VFS_BACKEND_SYNTHETIC;
    out->inode = 0xD000 + device_id;
    out->u.first_cluster = (uint32_t)device_id;
    copy_path_string(out->path, sizeof(out->path), path);
}

void vfs_init_mounts(void) {
    local_memset(g_mounts, 0, sizeof(g_mounts));
    aosfs_init();
    (void)vfs_mount("/", VFS_BACKEND_AOSFS, "/");
    (void)vfs_mount("commands", VFS_BACKEND_INITRD, "/");
    (void)vfs_mount("main", VFS_BACKEND_AOSFS, "main");
    (void)vfs_mount("etc", VFS_BACKEND_AOSFS, "etc");
    (void)vfs_mount("tmp", VFS_BACKEND_TMPFS, "tmp");
    (void)vfs_mount("trash", VFS_BACKEND_FAT32, "");
    (void)vfs_mount("fat32", VFS_BACKEND_FAT32, "fat32");
    (void)vfs_mount("mnt/fat32", VFS_BACKEND_FAT32, "fat32");
    (void)vfs_mount("ext4", VFS_BACKEND_EXT4, "ext4");
    (void)vfs_mount("mnt/ext4", VFS_BACKEND_EXT4, "ext4");
}

int vfs_mount(const char* path, uint8_t backend, const char* backend_root) {
    char normalized[MAX_VFS_PATH];
    char root_normalized[MAX_VFS_PATH];

    if (normalize_path(path, normalized, sizeof(normalized)) != 0 ||
        normalize_path(backend_root, root_normalized, sizeof(root_normalized)) != 0) {
        return -1;
    }

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].in_use && path_equals(g_mounts[i].path, normalized)) {
            g_mounts[i].backend = backend;
            copy_path_string(g_mounts[i].backend_root, sizeof(g_mounts[i].backend_root), root_normalized);
            return 0;
        }
    }

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].in_use) {
            g_mounts[i].in_use = 1;
            g_mounts[i].backend = backend;
            copy_path_string(g_mounts[i].path, sizeof(g_mounts[i].path), normalized);
            copy_path_string(g_mounts[i].backend_root, sizeof(g_mounts[i].backend_root), root_normalized);
            return 0;
        }
    }

    return -1;
}

int vfs_mount_info_at(size_t index, struct vfs_mount_info* out) {
    size_t seen = 0;

    if (!out) {
        return -1;
    }

    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].in_use) {
            continue;
        }
        if (seen == index) {
            local_memset(out, 0, sizeof(*out));
            out->backend = g_mounts[i].backend;
            copy_path_string(out->path, sizeof(out->path), g_mounts[i].path);
            copy_path_string(out->backend_root, sizeof(out->backend_root), g_mounts[i].backend_root);
            return 0;
        }
        seen++;
    }

    return -1;
}

static int lookup_fat32_node(const char* normalized, struct vfs_node* out) {
    uint8_t is_dir = 0;
    uint32_t first_cluster = 0;
    uint32_t size = 0;

    if (fat32_lookup_path(normalized, &is_dir, &first_cluster, &size) != 0) {
        return -1;
    }

    local_memset(out, 0, sizeof(*out));
    out->type = is_dir ? VFS_NODE_TYPE_DIRECTORY : VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_FAT32;
    out->size = size;
    out->inode = first_cluster ? first_cluster : 3;
    out->u.first_cluster = first_cluster;
    copy_path_string(out->path, sizeof(out->path), normalized);
    return 0;
}

static int lookup_ext4_node(const char* normalized, struct vfs_node* out) {
    uint8_t is_dir = 0;
    uint32_t inode = 0;
    uint32_t size = 0;

    if (ext4_lookup_path(normalized, &is_dir, &inode, &size) != 0) {
        return -1;
    }

    local_memset(out, 0, sizeof(*out));
    out->type = is_dir ? VFS_NODE_TYPE_DIRECTORY : VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_EXT4;
    out->size = size;
    out->inode = inode;
    copy_path_string(out->path, sizeof(out->path), normalized);
    return 0;
}

static int lookup_tmpfs_node(const char* normalized, struct vfs_node* out) {
    if (!path_starts_with_component(normalized, "tmp")) {
        return -1;
    }
    return tmpfs_lookup_path(normalized, out);
}

static int lookup_initrd_node(const char* normalized, struct vfs_node* out) {
    uint8_t* data = NULL;
    uint32_t size = 0;

    if (!normalized || !out) {
        return -1;
    }
    if (*normalized == '\0') {
        local_memset(out, 0, sizeof(*out));
        out->type = VFS_NODE_TYPE_DIRECTORY;
        out->backend = VFS_BACKEND_INITRD;
        out->inode = 0x1A17D;
        copy_path_string(out->path, sizeof(out->path), "");
        return 0;
    }
    for (size_t i = 0; normalized[i]; i++) {
        if (normalized[i] == '/') {
            return -1;
        }
    }
    if (initrd_get_file(normalized, &data, &size) != 0) {
        return -1;
    }

    local_memset(out, 0, sizeof(*out));
    out->type = VFS_NODE_TYPE_REGULAR;
    out->backend = VFS_BACKEND_INITRD;
    out->size = size;
    out->inode = 0x1A000 + (uint64_t)(uintptr_t)data;
    out->u.data = data;
    copy_path_string(out->path, sizeof(out->path), normalized);
    return 0;
}

int vfs_lookup(const char* path, struct vfs_node* out) {
    char normalized[MAX_VFS_PATH];
    char backend_path[MAX_VFS_PATH];
    const struct vfs_mount* mount;

    if (!out) {
        return -1;
    }
    if (normalize_path(path, normalized, sizeof(normalized)) != 0) {
        return -1;
    }

    if (*normalized == '\0') {
        return aosfs_lookup_path("", out);
    }

    if (path_equals(normalized, "dev")) {
        fill_synthetic_dir(out, 0xD00, normalized);
        return 0;
    }
    if (path_equals(normalized, "dev/console")) {
        fill_synthetic_char_device(out, VFS_DEV_CONSOLE, normalized);
        return 0;
    }
    if (path_equals(normalized, "dev/tty0")) {
        fill_synthetic_char_device(out, VFS_DEV_TTY0, normalized);
        return 0;
    }
    if (path_equals(normalized, "dev/null")) {
        fill_synthetic_char_device(out, VFS_DEV_NULL, normalized);
        return 0;
    }

    mount = find_mount_for_path(normalized, backend_path, sizeof(backend_path));
    if (mount) {
        if (mount->backend == VFS_BACKEND_TMPFS) {
            return lookup_tmpfs_node(backend_path, out);
        }
        if (mount->backend == VFS_BACKEND_INITRD) {
            return lookup_initrd_node(backend_path, out);
        }
        if (mount->backend == VFS_BACKEND_FAT32) {
            return lookup_fat32_node(backend_path, out);
        }
        if (mount->backend == VFS_BACKEND_EXT4) {
            return lookup_ext4_node(backend_path, out);
        }
        if (mount->backend == VFS_BACKEND_AOSFS) {
            return aosfs_lookup_path(backend_path, out);
        }
    }

    if (synthetic_dir_exists(normalized)) {
        fill_synthetic_dir(out, path_equals(normalized, "mnt") ? VFS_SYNTH_MNT_INO : VFS_SYNTH_DYNAMIC_DIR_BASE, normalized);
        return 0;
    }

    return aosfs_lookup_path(normalized, out);
}

int vfs_access_path(const char* path, uint64_t mode) {
    struct vfs_node node;

    if (!path) {
        return -1;
    }
    if (mode & ~(LINUX_R_OK | LINUX_W_OK | LINUX_X_OK)) {
        return -1;
    }
    if (vfs_lookup(path, &node) != 0) {
        return -1;
    }
    if (node.type == VFS_NODE_TYPE_CHAR_DEVICE) {
        return 0;
    }
    if (mode & LINUX_W_OK) {
        return (node.backend == VFS_BACKEND_AOSFS || node.backend == VFS_BACKEND_TMPFS || node.backend == VFS_BACKEND_FAT32 || node.backend == VFS_BACKEND_EXT4) ? 0 : -1;
    }
    return 0;
}

int vfs_read_node(const struct vfs_node* node, uint64_t offset, uint8_t* buffer, uint64_t len) {
    if (!node || !buffer || node->type != VFS_NODE_TYPE_REGULAR) {
        return -1;
    }

    if (node->backend == VFS_BACKEND_FAT32) {
        return fat32_read_file(node->u.first_cluster, node->size, offset, buffer, len);
    }
    if (node->backend == VFS_BACKEND_EXT4) {
        return ext4_read_file((uint32_t)node->inode, node->size, offset, buffer, len);
    }
    if (node->backend == VFS_BACKEND_TMPFS) {
        return tmpfs_read_path(node->path, offset, buffer, len);
    }
    if (node->backend != VFS_BACKEND_AOSFS && node->backend != VFS_BACKEND_INITRD) {
        return -1;
    }

    local_memcpy(buffer, node->u.data + offset, (size_t)len);
    return 0;
}

int vfs_write_node(const struct vfs_node* node, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size) {
    if (!node || !buffer || node->type != VFS_NODE_TYPE_REGULAR) {
        return -1;
    }
    if (node->backend == VFS_BACKEND_TMPFS) {
        return tmpfs_write_path(node->path, offset, buffer, len, written, new_size);
    }
    if (node->backend == VFS_BACKEND_FAT32) {
        return fat32_write_path(node->path, offset, buffer, len, written, new_size);
    }
    if (node->backend == VFS_BACKEND_EXT4) {
        return ext4_write_path(node->path, offset, buffer, len, written, new_size);
    }
    if (node->backend == VFS_BACKEND_AOSFS) {
        return aosfs_write_path(node->path, offset, buffer, len, written, new_size);
    }
    return -1;
}

int vfs_unlink_path(const char* path) {
    char normalized[MAX_VFS_PATH];
    char backend_path[MAX_VFS_PATH];
    const struct vfs_mount* mount;
    struct vfs_node node;

    if (normalize_path(path, normalized, sizeof(normalized)) != 0 || normalized[0] == '\0') {
        return -1;
    }
    if (vfs_lookup(normalized, &node) != 0 || node.type != VFS_NODE_TYPE_REGULAR) {
        return -1;
    }

    mount = find_mount_for_path(normalized, backend_path, sizeof(backend_path));
    if (mount && mount->backend == VFS_BACKEND_AOSFS) {
        return aosfs_unlink_path(backend_path);
    }
    if (!mount && node.backend == VFS_BACKEND_AOSFS) {
        return aosfs_unlink_path(normalized);
    }
    return -1;
}

int vfs_rmdir_path(const char* path) {
    char normalized[MAX_VFS_PATH];
    char backend_path[MAX_VFS_PATH];
    const struct vfs_mount* mount;
    struct vfs_node node;

    if (normalize_path(path, normalized, sizeof(normalized)) != 0 || normalized[0] == '\0') {
        return -1;
    }
    if (vfs_lookup(normalized, &node) != 0 || node.type != VFS_NODE_TYPE_DIRECTORY) {
        return -1;
    }

    mount = find_mount_for_path(normalized, backend_path, sizeof(backend_path));
    if (mount && mount->backend == VFS_BACKEND_AOSFS) {
        return aosfs_rmdir_path(backend_path);
    }
    if (!mount && node.backend == VFS_BACKEND_AOSFS) {
        return aosfs_rmdir_path(normalized);
    }
    return -1;
}

int vfs_dirent_at(const struct vfs_node* node, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    if (!node || !name_buf || name_buf_size == 0 || node->type != VFS_NODE_TYPE_DIRECTORY) {
        return -1;
    }

    if (node->backend == VFS_BACKEND_FAT32) {
        return fat32_dirent_at_index(node->path, index, name_buf, name_buf_size, size, d_type);
    }
    if (node->backend == VFS_BACKEND_EXT4) {
        return ext4_dirent_at_index(node->path, index, name_buf, name_buf_size, size, d_type);
    }
    if (node->backend == VFS_BACKEND_AOSFS) {
        return aosfs_dirent_at_index(node->path, index, name_buf, name_buf_size, size, d_type);
    }
    if (node->backend == VFS_BACKEND_INITRD) {
        uint32_t entry_size = 0;
        if (node->type != VFS_NODE_TYPE_DIRECTORY) {
            return -1;
        }
        if (index == 0) {
            copy_path_string(name_buf, name_buf_size, ".");
            if (size) *size = 0;
            if (d_type) *d_type = LINUX_DTYPE_DIR;
            return 0;
        }
        if (index == 1) {
            copy_path_string(name_buf, name_buf_size, "..");
            if (size) *size = 0;
            if (d_type) *d_type = LINUX_DTYPE_DIR;
            return 0;
        }
        if (initrd_get_entry(index - 2, name_buf, name_buf_size, &entry_size) != 0) {
            return -1;
        }
        if (size) *size = entry_size;
        if (d_type) *d_type = LINUX_DTYPE_REG;
        return 0;
    }

    if (node->backend == VFS_BACKEND_SYNTHETIC) {
        if (node->inode == VFS_SYNTH_ROOT_INO) {
            uint64_t mount_count;
            uint64_t initrd_index;
            if (index == 0) {
                copy_path_string(name_buf, name_buf_size, ".");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_DIR;
                return 0;
            }
            if (index == 1) {
                copy_path_string(name_buf, name_buf_size, "..");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_DIR;
                return 0;
            }

            if (dirent_mount_child_at("", index - 2, name_buf, name_buf_size, size, d_type) == 0) {
                return 0;
            }
            mount_count = mount_child_count("");
            if (index < 2 + mount_count) {
                return -1;
            }
            if (index == 2 + mount_count) {
                copy_path_string(name_buf, name_buf_size, "dev");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_DIR;
                return 0;
            }
            initrd_index = index - 3 - mount_count;
            if (initrd_get_entry(initrd_index, name_buf, name_buf_size, size) != 0) {
                return -1;
            }
            if (d_type) *d_type = LINUX_DTYPE_REG;
            return 0;
        }

        if (path_equals(node->path, "dev")) {
            if (index == 0) {
                copy_path_string(name_buf, name_buf_size, ".");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_DIR;
                return 0;
            }
            if (index == 1) {
                copy_path_string(name_buf, name_buf_size, "..");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_DIR;
                return 0;
            }
            if (index == 2) {
                copy_path_string(name_buf, name_buf_size, "console");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_CHR;
                return 0;
            }
            if (index == 3) {
                copy_path_string(name_buf, name_buf_size, "tty0");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_CHR;
                return 0;
            }
            if (index == 4) {
                copy_path_string(name_buf, name_buf_size, "null");
                if (size) *size = 0;
                if (d_type) *d_type = LINUX_DTYPE_CHR;
                return 0;
            }
            return -1;
        }

        if (index == 0) {
            copy_path_string(name_buf, name_buf_size, ".");
            if (size) *size = 0;
            if (d_type) *d_type = LINUX_DTYPE_DIR;
            return 0;
        }
        if (index == 1) {
            copy_path_string(name_buf, name_buf_size, "..");
            if (size) *size = 0;
            if (d_type) *d_type = LINUX_DTYPE_DIR;
            return 0;
        }
        return dirent_mount_child_at(node->path, index - 2, name_buf, name_buf_size, size, d_type);
    }

    if (node->backend == VFS_BACKEND_TMPFS) {
        return tmpfs_dirent_at_index(index, name_buf, name_buf_size, size, d_type);
    }

    return -1;
}
