/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOSFS_H
#define AOSFS_H

#include <stddef.h>
#include <stdint.h>
#include <vfs.h>

void aosfs_init(void);
int aosfs_mount(uint32_t blkdev_id);
int aosfs_mount_at(uint32_t blkdev_id, uint64_t base_offset);
int aosfs_mount_role(uint8_t role, uint32_t blkdev_id, uint64_t base_offset);
int aosfs_lookup_path(const char* path, struct vfs_node* out);
int aosfs_create_path(const char* path, struct vfs_node* out);
int aosfs_mkdir_path(const char* path);
int aosfs_truncate_path(const char* path);
int aosfs_unlink_path(const char* path);
int aosfs_rmdir_path(const char* path);
int aosfs_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size);
int aosfs_dirent_at_index(const char* path, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type);

#endif
