/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "pmm.h"
#include "multiboot2.h"
#include <vga.h>

// 32KB bitmap can manage 1GB of memory (32768 bytes * 8 bits/byte * 4096 bytes/page)
uint8_t pmm_bitmap[32768]; 
uint64_t total_blocks = 262144; // 1GB in 4KB blocks
static uint64_t free_blocks;

extern void serial_print(const char* s);
extern uint8_t aos_boot_verbose;
extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];

static void mark_used_range(uint64_t start, uint64_t end) {
    if (end <= start) return;

    const uint64_t one_gib = 1024ULL * 1024ULL * 1024ULL;
    uint64_t range_start = start & ~0xFFFULL;
    uint64_t range_end = (end + 0xFFFULL) & ~0xFFFULL;

    if (range_start >= one_gib) return;
    if (range_end > one_gib) range_end = one_gib;

    for (uint64_t addr = range_start; addr < range_end; addr += 4096) {
        uint64_t block = addr / 4096;
        uint8_t mask = (uint8_t)(1 << (block % 8));
        if ((pmm_bitmap[block / 8] & mask) == 0 && free_blocks > 0) {
            free_blocks--;
        }
        pmm_bitmap[block / 8] |= mask;
    }
}

void pmm_init(uint64_t mb_info) {
    if (aos_boot_verbose) {
        serial_print("PMM: Initializing...\n");
        vga_print("PMM: Initializing...", 0x0F, 0, 10);
    }
    total_blocks = 0;
    free_blocks = 0;

    // 1. Mark everything as used by default (1)
    for(int i = 0; i < 32768; i++) pmm_bitmap[i] = 0xFF;
    
    struct multiboot_tag* tag;
    int found_mmap = 0;
    
    // mb_info points to the total size (4 bytes) and a reserved field (4 bytes)
    for (tag = (struct multiboot_tag*)(mb_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) 
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            found_mmap = 1;
            struct multiboot_tag_mmap* mmap = (struct multiboot_tag_mmap*)tag;
            struct multiboot_mmap_entry* entry;

            for (entry = mmap->entries;
                 (uint8_t*)entry < (uint8_t*)tag + tag->size;
                 entry = (struct multiboot_mmap_entry*)((uint8_t*)entry + mmap->entry_size)) 
            {
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    uint64_t entry_end = entry->addr + entry->len;
                    uint64_t capped_end = entry_end;
                    if (capped_end > 1024ULL * 1024ULL * 1024ULL) {
                        capped_end = 1024ULL * 1024ULL * 1024ULL;
                    }
                    if (capped_end > total_blocks * 4096ULL) {
                        total_blocks = (capped_end + 4095ULL) / 4096ULL;
                    }
                    for (uint64_t addr = entry->addr; addr < entry->addr + entry->len; addr += 4096) {
                        if (addr >= 1024 * 1024 * 1024) break;
                        uint64_t block = addr / 4096;
                        uint8_t mask = (uint8_t)(1 << (block % 8));
                        if (pmm_bitmap[block / 8] & mask) {
                            pmm_bitmap[block / 8] &= (uint8_t)~mask;
                            free_blocks++;
                        }
                    }
                }
            }
        }
    }

    if (!found_mmap) {
        serial_print("PMM ERROR: No Memory Map found!\n");
        vga_print("PMM ERROR: No Memory Map found! Falling back to 128MB.", 0x0C, 0, 11);
        total_blocks = 32768;
        for(uint64_t i = 0; i < total_blocks; i++) {
            uint8_t mask = (uint8_t)(1 << (i % 8));
            if (pmm_bitmap[i / 8] & mask) {
                pmm_bitmap[i / 8] &= (uint8_t)~mask;
                free_blocks++;
            }
        }
    } else {
        if (total_blocks == 0) {
            total_blocks = 262144;
        }
        if (aos_boot_verbose) {
            serial_print("PMM: Memory Map parsed.\n");
            vga_print("PMM: Memory Map parsed successfully.", 0x0A, 0, 11);
        }
    }

    // 2. Mark the first 1MB as used (kernel and BIOS area)
    for(int i = 0; i < 256; i++) {
        uint8_t mask = (uint8_t)(1 << (i % 8));
        if ((pmm_bitmap[i / 8] & mask) == 0 && free_blocks > 0) {
            free_blocks--;
        }
        pmm_bitmap[i / 8] |= mask;
    }

    // 3. Mark kernel image pages as used so allocator cannot overwrite it.
    mark_used_range((uint64_t)__kernel_start, (uint64_t)__kernel_end);

    // 4. Mark module (e.g., initrd) pages as used.
    for (tag = (struct multiboot_tag*)(mb_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7)))
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module* mod = (struct multiboot_tag_module*)tag;
            mark_used_range((uint64_t)mod->mod_start, (uint64_t)mod->mod_end);
        }
    }
}

void* pmm_alloc_block() {
    for (uint64_t i = 0; i < total_blocks; i++) {
        uint8_t mask = (uint8_t)(1 << (i % 8));
        if (!(pmm_bitmap[i / 8] & mask)) {
            pmm_bitmap[i / 8] |= mask; // Mark as used
            if (free_blocks > 0) free_blocks--;
            return (void*)(i * 4096);
        }
    }
    serial_print("PMM: Out of memory!\n");
    return NULL; // Out of memory!
}

void pmm_free_block(void* addr) {
    uint64_t block = (uint64_t)addr / 4096;
    uint8_t mask = (uint8_t)(1 << (block % 8));
    if (block >= total_blocks) return;
    if (pmm_bitmap[block / 8] & mask) {
        pmm_bitmap[block / 8] &= (uint8_t)~mask;
        free_blocks++;
    }
}

uint64_t pmm_total_memory(void) {
    return total_blocks * 4096ULL;
}

uint64_t pmm_free_memory(void) {
    return free_blocks * 4096ULL;
}

uint64_t pmm_used_memory(void) {
    return pmm_total_memory() - pmm_free_memory();
}
