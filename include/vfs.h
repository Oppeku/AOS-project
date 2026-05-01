/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_NODE_TYPE_REGULAR 1
#define VFS_NODE_TYPE_DIRECTORY 2

#define VFS_BACKEND_SYNTHETIC 1
#define VFS_BACKEND_INITRD 2
#define VFS_BACKEND_FAT32 3
#define VFS_BACKEND_TMPFS 4
#define VFS_BACKEND_EXT4 5

#define VFS_MAX_MOUNTS 8

struct vfs_node {
    uint8_t type;
    uint8_t backend;
    uint16_t reserved;
    uint32_t size;
    uint64_t inode;
    char path[256];
    union {
        const uint8_t* data;
        uint32_t first_cluster;
    } u;
};

void vfs_init_mounts(void);
int vfs_mount(const char* path, uint8_t backend, const char* backend_root);
int vfs_lookup(const char* path, struct vfs_node* out);
int vfs_access_path(const char* path, uint64_t mode);
int vfs_read_node(const struct vfs_node* node, uint64_t offset, uint8_t* buffer, uint64_t len);
int vfs_write_node(const struct vfs_node* node, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size);
int vfs_dirent_at(const struct vfs_node* node, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type);

#endif
