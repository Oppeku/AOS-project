/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PAGE_PRESENT  (1 << 0)
#define PAGE_WRITABLE (1 << 1)
#define PAGE_USER     (1 << 2) // Bit 2: 0=Kernel, 1=User
#define AOS_USER_SPACE_START 0x0000010000000000ULL

void init_vmm();
void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t vmm_unmap_page(uint64_t* pml4, uint64_t virt);
void vmm_free_user_space(uint64_t* pml4);
uint64_t* vmm_copy_p4(uint64_t* src_p4);

#endif
