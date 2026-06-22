/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef TMPFS_H
#define TMPFS_H

#include <stdint.h>
#include <stddef.h>
#include <vfs.h>

void tmpfs_init(void);
int tmpfs_lookup_path(const char* path, struct vfs_node* out);
int tmpfs_create_path(const char* path, struct vfs_node* out);
int tmpfs_truncate_path(const char* path);
int tmpfs_read_path(const char* path, uint64_t offset, uint8_t* buffer, uint64_t len);
int tmpfs_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size);
int tmpfs_unlink_path(const char* path);
int tmpfs_dirent_at_index(uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type);

#endif
