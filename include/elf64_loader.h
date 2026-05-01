/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef ELF64_LOADER_H
#define ELF64_LOADER_H

#include <stdint.h>
#include <stddef.h>

int elf64_load_image(uint64_t* pml4, const uint8_t* image, size_t image_size, uint64_t* entry_out);

#endif
