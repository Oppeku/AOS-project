/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef PMM_H
#define PMM_H
#include <stdint.h>
#include <stddef.h>

void pmm_init(uint64_t mem_size);
void* pmm_alloc_block();
void pmm_free_block(void* addr);
uint64_t pmm_total_memory(void);
uint64_t pmm_free_memory(void);
uint64_t pmm_used_memory(void);

#endif
