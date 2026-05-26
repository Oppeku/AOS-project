/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "vmm.h"
#include "pmm.h"
#include <stddef.h> // for size_t

// The kernel's main PML4 table - linking to assembly p4_table
extern uint64_t p4_table[]; 

// Missing logic the linker asked for
void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while(n--)
        *p++ = (unsigned char)c;
    return s;
}

// Ensure this matches the asm global name
extern void load_pagemap(uint64_t);

void init_vmm() {
    // The first 1GB is already identity mapped in paging.asm.
    // We just need to load the PML4 pointer into CR3.
    load_pagemap((uint64_t)p4_table);
}

extern void serial_print(const char* s);

static void* local_memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = dest;
    const uint8_t* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static int split_huge_pd_entry(uint64_t* pd, uint64_t pd_idx) {
    uint64_t old_entry = pd[pd_idx];
    uint64_t* new_pt = (uint64_t*)pmm_alloc_block();
    uint64_t base_phys = old_entry & ~0x1FFFFFULL;
    uint64_t table_flags = old_entry & (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t pte_flags = old_entry & 0xFFFULL;

    if (!new_pt) {
        return -1;
    }

    memset(new_pt, 0, 4096);
    pte_flags &= ~(1ULL << 7);
    for (uint64_t i = 0; i < 512; i++) {
        new_pt[i] = (base_phys + i * 4096ULL) | pte_flags;
    }

    pd[pd_idx] = (uint64_t)new_pt | table_flags;
    return 0;
}

void vmm_map_page(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t table_flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_block();
        if (!new_table) return;
        memset((void*)new_table, 0, 4096);
        pml4[pml4_idx] = new_table | table_flags;
    }

    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_block();
        if (!new_table) return;
        memset((void*)new_table, 0, 4096);
        pdpt[pdpt_idx] = new_table | table_flags;
    }

    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    if ((pd[pd_idx] & PAGE_PRESENT) && (pd[pd_idx] & (1ULL << 7))) {
        if (split_huge_pd_entry(pd, pd_idx) != 0) {
            return;
        }
    }
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        uint64_t new_table = (uint64_t)pmm_alloc_block();
        if (!new_table) return;
        memset((void*)new_table, 0, 4096);
        pd[pd_idx] = new_table | table_flags;
    }

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    pt[pt_idx] = phys | flags;

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t vmm_unmap_page(uint64_t* pml4, uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!pml4 || virt < AOS_USER_SPACE_START || !(pml4[pml4_idx] & PAGE_PRESENT)) return 0;

    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFF);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;

    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFF);
    if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
    if (pd[pd_idx] & (1ULL << 7)) return 0;

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFF);
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

    uint64_t phys = pt[pt_idx] & ~0xFFFULL;
    pt[pt_idx] = 0;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return phys;
}

static int table_has_present_entries(uint64_t* table) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & PAGE_PRESENT) return 1;
    }
    return 0;
}

void vmm_free_user_space(uint64_t* pml4) {
    if (!pml4) return;

    for (int i = 0; i < 512; i++) {
        uint64_t pml4_base = (uint64_t)i << 39;
        if (pml4_base < AOS_USER_SPACE_START) continue;
        if (!(pml4[i] & PAGE_PRESENT) || !(pml4[i] & PAGE_USER)) continue;
        uint64_t* pdpt = (uint64_t*)(pml4[i] & ~0xFFFULL);

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PAGE_PRESENT) || !(pdpt[j] & PAGE_USER)) continue;
            uint64_t* pd = (uint64_t*)(pdpt[j] & ~0xFFFULL);

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PAGE_PRESENT) || !(pd[k] & PAGE_USER)) continue;
                if (pd[k] & (1ULL << 7)) {
                    pmm_free_block((void*)(pd[k] & ~0x1FFFFFULL));
                    pd[k] = 0;
                    continue;
                }

                uint64_t* pt = (uint64_t*)(pd[k] & ~0xFFFULL);
                for (int l = 0; l < 512; l++) {
                    if ((pt[l] & PAGE_PRESENT) && (pt[l] & PAGE_USER)) {
                        pmm_free_block((void*)(pt[l] & ~0xFFFULL));
                        pt[l] = 0;
                    }
                }

                if (!table_has_present_entries(pt)) {
                    pmm_free_block(pt);
                    pd[k] = 0;
                }
            }

            if (!table_has_present_entries(pd)) {
                pmm_free_block(pd);
                pdpt[j] = 0;
            }
        }

        if (!table_has_present_entries(pdpt)) {
            pmm_free_block(pdpt);
            pml4[i] = 0;
        }
    }

    asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}

uint64_t* vmm_copy_p4(uint64_t* src_p4) {
    uint64_t* dst_p4 = (uint64_t*)pmm_alloc_block();
    if (!dst_p4) return NULL;
    memset(dst_p4, 0, 4096);

    for (int i = 0; i < 512; i++) {
        if (!(src_p4[i] & PAGE_PRESENT)) continue;

        if (((uint64_t)i << 39) < AOS_USER_SPACE_START) {
            dst_p4[i] = src_p4[i];
            continue;
        }

        uint64_t* src_pdpt = (uint64_t*)(src_p4[i] & ~0xFFF);
        uint64_t* dst_pdpt = (uint64_t*)pmm_alloc_block();
        memset(dst_pdpt, 0, 4096);
        dst_p4[i] = (uint64_t)dst_pdpt | (src_p4[i] & 0xFFF);

        for (int j = 0; j < 512; j++) {
            if (!(src_pdpt[j] & PAGE_PRESENT)) continue;

            uint64_t* src_pd = (uint64_t*)(src_pdpt[j] & ~0xFFF);
            uint64_t* dst_pd = (uint64_t*)pmm_alloc_block();
            memset(dst_pd, 0, 4096);
            dst_pdpt[j] = (uint64_t)dst_pd | (src_pdpt[j] & 0xFFF);

            for (int k = 0; k < 512; k++) {
                if (!(src_pd[k] & PAGE_PRESENT)) continue;

                if (src_pd[k] & (1 << 7)) { // Huge page 2MB
                    if (i == 0 && j == 0 && k == 0) {
                        dst_pd[k] = src_pd[k]; // Identity map share
                        continue;
                    }
                    dst_pd[k] = src_pd[k]; 
                    continue;
                }

                uint64_t* src_pt = (uint64_t*)(src_pd[k] & ~0xFFF);
                uint64_t* dst_pt = (uint64_t*)pmm_alloc_block();
                memset(dst_pt, 0, 4096);
                dst_pd[k] = (uint64_t)dst_pt | (src_pd[k] & 0xFFF);

                for (int l = 0; l < 512; l++) {
                    if (!(src_pt[l] & PAGE_PRESENT)) continue;

                    if (src_pt[l] & PAGE_USER) {
                        void* new_page = pmm_alloc_block();
                        local_memcpy(new_page, (void*)(src_pt[l] & ~0xFFF), 4096);
                        dst_pt[l] = (uint64_t)new_page | (src_pt[l] & 0xFFF);
                    } else {
                        dst_pt[l] = src_pt[l];
                    }
                }
            }
        }
    }
    return dst_p4;
}
