/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

typedef struct vfs_node {
    char name[64];
    uint32_t flags;
    uint32_t length;
    uint32_t inode;
    struct vfs_node* (*open)(struct vfs_node* node);
    uint32_t (*read)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    uint32_t (*write)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
} vfs_node_t;

vfs_node_t* vfs_open(const char* path);
uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);

#endif
