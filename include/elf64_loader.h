/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef ELF64_LOADER_H
#define ELF64_LOADER_H

#include <stdint.h>
#include <stddef.h>

#define ELF64_INTERPRETER_PATH_MAX 256
#define ELF64_DEFAULT_DYNAMIC_BASE 0x0000700001000000ULL

struct elf64_load_options {
    uint64_t dynamic_base;
    uint8_t apply_relocations;
};

struct elf64_load_result {
    uint64_t entry;
    uint64_t load_bias;
    uint64_t image_end;
    uint64_t program_headers;
    uint16_t program_header_size;
    uint16_t program_header_count;
    char interpreter[ELF64_INTERPRETER_PATH_MAX];
};

int elf64_load_image_ex(uint64_t* pml4,
                        const uint8_t* image,
                        size_t image_size,
                        const struct elf64_load_options* options,
                        struct elf64_load_result* result);
int elf64_load_image(uint64_t* pml4, const uint8_t* image, size_t image_size, uint64_t* entry_out);

#endif
