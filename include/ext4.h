/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef EXT4_H
#define EXT4_H

#include <stdint.h>
#include <stddef.h>

int ext4_init(uint32_t mod_start, uint32_t mod_end);
int ext4_is_ready(void);
int ext4_is_ext4_path(const char* path);
int ext4_lookup_path(const char* path, uint8_t* is_dir, uint32_t* inode, uint32_t* size);
int ext4_read_file(uint32_t inode, uint32_t file_size, uint64_t offset, uint8_t* buffer, uint64_t len);
int ext4_create_path(const char* path, uint8_t* is_dir, uint32_t* inode, uint32_t* size);
int ext4_truncate_path(const char* path, uint32_t* size);
int ext4_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size);
int ext4_dirent_at_index(const char* path, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type);

#endif
