/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <cpio.h>
#include <firmware.h>
#include <stddef.h>
#include <stdint.h>

static struct firmware_blob g_firmware[FIRMWARE_MAX_BLOBS];
static size_t g_firmware_count;

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static int local_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int starts_with(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void copy_string(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    while (src && src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void firmware_init(void) {
    char name[FIRMWARE_NAME_MAX];
    uint32_t size;

    local_memset(g_firmware, 0, sizeof(g_firmware));
    g_firmware_count = 0;

    for (uint64_t index = 0; g_firmware_count < FIRMWARE_MAX_BLOBS; index++) {
        uint8_t* data = 0;

        if (initrd_get_entry(index, name, sizeof(name), &size) != 0) {
            break;
        }
        if (!starts_with(name, "firmware/")) {
            continue;
        }
        if (initrd_get_file(name, &data, &size) != 0) {
            continue;
        }

        copy_string(g_firmware[g_firmware_count].name,
                    sizeof(g_firmware[g_firmware_count].name),
                    name);
        g_firmware[g_firmware_count].data = data;
        g_firmware[g_firmware_count].size = size;
        g_firmware_count++;
    }
}

size_t firmware_count(void) {
    return g_firmware_count;
}

const struct firmware_blob* firmware_get(size_t index) {
    if (index >= g_firmware_count) {
        return 0;
    }
    return &g_firmware[index];
}

int firmware_find(const char* name, const uint8_t** data, uint32_t* size) {
    if (!name || !data || !size) {
        return -1;
    }

    for (size_t i = 0; i < g_firmware_count; i++) {
        if (local_strcmp(g_firmware[i].name, name) == 0) {
            *data = g_firmware[i].data;
            *size = g_firmware[i].size;
            return 0;
        }
    }

    return -1;
}
