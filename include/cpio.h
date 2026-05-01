/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef CPIO_H
#define CPIO_H

#include <stdint.h>
#include <stddef.h>

struct cpio_new_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

int init_initrd(uint32_t mod_start, uint32_t mod_end);
int initrd_get_file(const char* filename, uint8_t** data, uint32_t* size);
int initrd_get_entry(uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size);

#endif
