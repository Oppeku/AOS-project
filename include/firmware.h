/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_FIRMWARE_H
#define AOS_FIRMWARE_H

#include <stddef.h>
#include <stdint.h>

#define FIRMWARE_MAX_BLOBS 32
#define FIRMWARE_NAME_MAX 96

struct firmware_blob {
    char name[FIRMWARE_NAME_MAX];
    const uint8_t* data;
    uint32_t size;
};

void firmware_init(void);
size_t firmware_count(void);
const struct firmware_blob* firmware_get(size_t index);
int firmware_find(const char* name, const uint8_t** data, uint32_t* size);

#endif
