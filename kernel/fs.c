/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "fs.h"
#include <vga.h>

vfs_node_t* vfs_open(const char* path) {
    vga_print("VFS: Open ", 0x07, 0, 11);
    vga_print(path, 0x07, 10, 11);
    return NULL;
}

uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node->read != NULL) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}
