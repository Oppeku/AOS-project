/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "cpio.h"
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);
extern uint8_t aos_boot_verbose;
static uint64_t g_initrd_start;
static uint64_t g_initrd_end;

// Simple hex to integer converter for CPIO headers
static uint32_t parse_hex(const char* s, int n) {
    uint32_t res = 0;
    for (int i = 0; i < n; i++) {
        res <<= 4;
        if (s[i] >= '0' && s[i] <= '9') res += s[i] - '0';
        else if (s[i] >= 'a' && s[i] <= 'f') res += s[i] - 'a' + 10;
        else if (s[i] >= 'A' && s[i] <= 'F') res += s[i] - 'A' + 10;
    }
    return res;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void copy_name(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst || dst_size == 0) {
        return;
    }

    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int initrd_get_file(const char* filename, uint8_t** data, uint32_t* size) {
    if (!filename || !data || !size) return -1;
    if (!g_initrd_start || !g_initrd_end || g_initrd_start >= g_initrd_end) return -1;

    struct cpio_new_header* header = (struct cpio_new_header*)g_initrd_start;

    while ((uint64_t)header < g_initrd_end &&
           header->c_magic[0] == '0' && header->c_magic[1] == '7')
    {
        uint32_t namesize = parse_hex(header->c_namesize, 8);
        uint32_t filesize = parse_hex(header->c_filesize, 8);
        char* name = (char*)(header + 1);

        if (strcmp(name, "TRAILER!!!") == 0) break;

        uint64_t file_data_u64 = (uint64_t)header + sizeof(struct cpio_new_header) + namesize;
        file_data_u64 = (file_data_u64 + 3) & ~3ULL;

        if (strcmp(name, filename) == 0) {
            *data = (uint8_t*)file_data_u64;
            *size = filesize;
            return 0;
        }

        uint64_t next = file_data_u64 + filesize;
        next = (next + 3) & ~3ULL;
        header = (struct cpio_new_header*)next;
    }

    return -1;
}

int initrd_get_entry(uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size) {
    if (!name_buf || name_buf_size == 0 || !size) return -1;
    if (!g_initrd_start || !g_initrd_end || g_initrd_start >= g_initrd_end) return -1;

    uint64_t current = 0;
    struct cpio_new_header* header = (struct cpio_new_header*)g_initrd_start;

    while ((uint64_t)header < g_initrd_end &&
           header->c_magic[0] == '0' && header->c_magic[1] == '7')
    {
        uint32_t namesize = parse_hex(header->c_namesize, 8);
        uint32_t filesize = parse_hex(header->c_filesize, 8);
        char* name = (char*)(header + 1);

        if (strcmp(name, "TRAILER!!!") == 0) break;

        if (current == index) {
            copy_name(name_buf, name_buf_size, name);
            *size = filesize;
            return 0;
        }

        current++;

        uint64_t next = (uint64_t)header + sizeof(struct cpio_new_header) + namesize;
        next = (next + 3) & ~3ULL;
        next += filesize;
        next = (next + 3) & ~3ULL;

        header = (struct cpio_new_header*)next;
    }

    return -1;
}

int init_initrd(uint32_t mod_start, uint32_t mod_end) {
    if (mod_start >= mod_end) {
        serial_print("Initrd: invalid module bounds\n");
        return -1;
    }

    g_initrd_start = mod_start;
    g_initrd_end = mod_end;

    if (aos_boot_verbose) {
        serial_print("Initrd: Parsing CPIO...\n");
    }
    struct cpio_new_header* header = (struct cpio_new_header*)g_initrd_start;

    if ((uint64_t)header >= g_initrd_end ||
        header->c_magic[0] != '0' || header->c_magic[1] != '7')
    {
        serial_print("Initrd: invalid CPIO header\n");
        return -1;
    }

    while ((uint64_t)header < g_initrd_end &&
           header->c_magic[0] == '0' && header->c_magic[1] == '7')
    {
        uint32_t namesize = parse_hex(header->c_namesize, 8);
        uint32_t filesize = parse_hex(header->c_filesize, 8);
        char* name = (char*)(header + 1);
        
        if (strcmp(name, "TRAILER!!!") == 0) {
            if (aos_boot_verbose) {
                serial_print("Initrd: Found TRAILER!!!\n");
            }
            break;
        }

        if (aos_boot_verbose) {
            serial_print("Initrd: Found file: ");
            serial_print(name);
            serial_print("\n");
        }

        uint64_t next = (uint64_t)header + sizeof(struct cpio_new_header) + namesize;
        next = (next + 3) & ~3ULL; 
        next += filesize;
        next = (next + 3) & ~3ULL;
        
        header = (struct cpio_new_header*)next;
    }
    if (aos_boot_verbose) {
        serial_print("Initrd: Parsing Done.\n");
    }
    return 0;
}
