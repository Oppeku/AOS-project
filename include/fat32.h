/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

int fat32_init(uint32_t mod_start, uint32_t mod_end);
int fat32_is_ready(void);
int fat32_is_fat32_path(const char* path);
int fat32_lookup_path(const char* path, uint8_t* is_dir, uint32_t* first_cluster, uint32_t* size);
int fat32_read_file(uint32_t first_cluster, uint32_t file_size, uint64_t offset, uint8_t* buffer, uint64_t len);
int fat32_create_path(const char* path, uint8_t* is_dir, uint32_t* first_cluster, uint32_t* size);
int fat32_truncate_path(const char* path, uint32_t* first_cluster, uint32_t* size);
int fat32_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size);
int fat32_dirent_at_index(const char* path, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type);
int fat32_access_path(const char* path, uint64_t mode);

#endif
