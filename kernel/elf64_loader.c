/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <elf64_loader.h>
#include <pmm.h>
#include <vmm.h>
#include <stdint.h>
#include <stddef.h>

#define ELF_CLASS_64 2
#define ELF_DATA_LSB 1
#define ELF_TYPE_EXEC 2
#define ELF_TYPE_DYN 3
#define ELF_MACHINE_X86_64 62
#define ELF_PT_LOAD 1
#define ELF_PT_DYNAMIC 2
#define ELF_PF_W (1U << 1)
#define ELF_SHT_RELA 4
#define ELF_DT_NULL 0
#define ELF_DT_RELA 7
#define ELF_DT_RELASZ 8
#define ELF_DT_RELAENT 9
#define ELF_R_X86_64_RELATIVE 8
#define ELF_R_X86_64_IRELATIVE 37
#define USER_ELF_DYN_BASE 0x0000700001000000ULL
#define MAX_LOADER_PAGES 8192


struct elf64_ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed));

struct elf64_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} __attribute__((packed));

struct elf64_dyn {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} __attribute__((packed));

struct elf64_rela {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} __attribute__((packed));

struct mapped_page {
    uint64_t va;
    uint8_t* phys;
};

static void* local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

static void* local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
    return dst;
}

static uint64_t align_down_4k(uint64_t value) {
    return value & ~0xFFFULL;
}

static uint64_t align_up_4k(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
    return (a < b) ? a : b;
}

static int add_u64_overflow(uint64_t a, uint64_t b, uint64_t* out) {
    *out = a + b;
    return *out < a;
}

static int record_page(struct mapped_page* pages, size_t* page_count, uint64_t va, uint8_t* phys) {
    if (*page_count >= MAX_LOADER_PAGES) return -1;
    pages[*page_count].va = va;
    pages[*page_count].phys = phys;
    (*page_count)++;
    return 0;
}

static uint8_t* translate_va(struct mapped_page* pages, size_t page_count, uint64_t va) {
    uint64_t page_va = align_down_4k(va);
    uint64_t page_off = va - page_va;
    for (size_t i = 0; i < page_count; i++) {
        if (pages[i].va == page_va) {
            return pages[i].phys + page_off;
        }
    }
    return NULL;
}

static int read_va(struct mapped_page* pages, size_t page_count, uint64_t va, void* out, size_t n) {
    uint8_t* dst = (uint8_t*)out;
    for (size_t i = 0; i < n; i++) {
        uint8_t* src = translate_va(pages, page_count, va + i);
        if (!src) return -1;
        dst[i] = *src;
    }
    return 0;
}

static int write_u64_va(struct mapped_page* pages, size_t page_count, uint64_t va, uint64_t value) {
    uint8_t raw[8];
    for (size_t i = 0; i < 8; i++) {
        raw[i] = (uint8_t)(value >> (i * 8));
    }
    for (size_t i = 0; i < 8; i++) {
        uint8_t* dst = translate_va(pages, page_count, va + i);
        if (!dst) return -1;
        *dst = raw[i];
    }
    return 0;
}

static int apply_rela_entries(
    const struct elf64_rela* rela_entries,
    uint64_t rela_count,
    uint64_t load_bias,
    struct mapped_page* pages,
    size_t page_count
) {
    for (uint64_t i = 0; i < rela_count; i++) {
        const struct elf64_rela* rela = &rela_entries[i];
        uint32_t type = (uint32_t)(rela->r_info & 0xFFFFFFFFU);

        if (type == ELF_R_X86_64_RELATIVE) {
            uint64_t reloc_target = rela->r_offset + load_bias;
            uint64_t relocated_value = load_bias + (uint64_t)rela->r_addend;
            if (write_u64_va(pages, page_count, reloc_target, relocated_value) != 0) return -1;
            continue;
        }

        if (type == ELF_R_X86_64_IRELATIVE) {
            uint64_t reloc_target = rela->r_offset + load_bias;
            uint64_t resolver_va = load_bias + (uint64_t)rela->r_addend;
            uint64_t (*resolver)(void) = (uint64_t (*)(void))(uintptr_t)resolver_va;
            uint64_t resolved_value = resolver();
            if (write_u64_va(pages, page_count, reloc_target, resolved_value) != 0) return -1;
            continue;
        }

        if (type == 0) continue;
        return -1;
    }

    return 0;
}

static int apply_section_relocations(
    const struct elf64_ehdr* ehdr,
    const uint8_t* image,
    size_t image_size,
    uint64_t load_bias,
    struct mapped_page* pages,
    size_t page_count,
    int* applied_any
) {
    if (applied_any) {
        *applied_any = 0;
    }
    if (!ehdr->e_shoff || ehdr->e_shnum == 0) {
        return 0;
    }
    if (ehdr->e_shentsize != sizeof(struct elf64_shdr)) {
        return -1;
    }

    uint64_t sh_table_end = ehdr->e_shoff + (uint64_t)ehdr->e_shnum * sizeof(struct elf64_shdr);
    if (sh_table_end > image_size || sh_table_end < ehdr->e_shoff) {
        return -1;
    }

    const struct elf64_shdr* shdrs = (const struct elf64_shdr*)(image + ehdr->e_shoff);
    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        const struct elf64_shdr* shdr = &shdrs[i];
        if (shdr->sh_type != ELF_SHT_RELA || shdr->sh_size == 0) {
            continue;
        }
        if (shdr->sh_entsize != sizeof(struct elf64_rela) || (shdr->sh_size % shdr->sh_entsize) != 0) {
            return -1;
        }
        if (shdr->sh_offset + shdr->sh_size > image_size || shdr->sh_offset + shdr->sh_size < shdr->sh_offset) {
            return -1;
        }

        const struct elf64_rela* rela_entries = (const struct elf64_rela*)(image + shdr->sh_offset);
        uint64_t rela_count = shdr->sh_size / shdr->sh_entsize;
        if (apply_rela_entries(rela_entries, rela_count, load_bias, pages, page_count) != 0) {
            return -1;
        }
        if (applied_any) {
            *applied_any = 1;
        }
    }

    return 0;
}

static int apply_dynamic_relocations(
    const struct elf64_phdr* phdrs,
    uint16_t phnum,
    uint64_t load_bias,
    struct mapped_page* pages,
    size_t page_count
) {
    const struct elf64_phdr* dyn_phdr = NULL;
    for (uint16_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == ELF_PT_DYNAMIC && phdrs[i].p_memsz >= sizeof(struct elf64_dyn)) {
            dyn_phdr = &phdrs[i];
            break;
        }
    }
    if (!dyn_phdr) return 0;

    uint64_t dyn_va = dyn_phdr->p_vaddr + load_bias;
    uint64_t dyn_entries = dyn_phdr->p_memsz / sizeof(struct elf64_dyn);
    uint64_t rela_va = 0;
    uint64_t rela_sz = 0;
    uint64_t rela_ent = sizeof(struct elf64_rela);

    for (uint64_t i = 0; i < dyn_entries; i++) {
        struct elf64_dyn dyn;
        if (read_va(pages, page_count, dyn_va + i * sizeof(struct elf64_dyn), &dyn, sizeof(dyn)) != 0) return -1;
        if (dyn.d_tag == ELF_DT_NULL) break;

        if (dyn.d_tag == ELF_DT_RELA) {
            rela_va = dyn.d_un.d_ptr + load_bias;
        } else if (dyn.d_tag == ELF_DT_RELASZ) {
            rela_sz = dyn.d_un.d_val;
        } else if (dyn.d_tag == ELF_DT_RELAENT) {
            rela_ent = dyn.d_un.d_val;
        }
    }

    if (!rela_va || !rela_sz) return 0;
    if (rela_ent != sizeof(struct elf64_rela)) return -1;
    if (rela_sz % rela_ent != 0) return -1;

    uint64_t rela_count = rela_sz / rela_ent;
    struct elf64_rela rela_entries[64];
    if (rela_count > 64) {
        return -1;
    }
    for (uint64_t i = 0; i < rela_count; i++) {
        if (read_va(pages, page_count, rela_va + i * sizeof(struct elf64_rela), &rela_entries[i], sizeof(struct elf64_rela)) != 0) {
            return -1;
        }
    }
    return apply_rela_entries(rela_entries, rela_count, load_bias, pages, page_count);
}

int elf64_load_image(uint64_t* pml4, const uint8_t* image, size_t image_size, uint64_t* entry_out) {
    if (!pml4 || !image || !entry_out) return -1;
    if (image_size < sizeof(struct elf64_ehdr)) return -1;

    const struct elf64_ehdr* ehdr = (const struct elf64_ehdr*)image;
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') return -1;
    if (ehdr->e_ident[4] != ELF_CLASS_64 || ehdr->e_ident[5] != ELF_DATA_LSB) return -1;
    if ((ehdr->e_type != ELF_TYPE_EXEC && ehdr->e_type != ELF_TYPE_DYN) || ehdr->e_machine != ELF_MACHINE_X86_64) return -1;
    if (ehdr->e_phentsize != sizeof(struct elf64_phdr)) return -1;

    uint64_t ph_table_end = ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(struct elf64_phdr);
    if (ph_table_end > image_size || ph_table_end < ehdr->e_phoff) return -1;
    const struct elf64_phdr* phdrs = (const struct elf64_phdr*)(image + ehdr->e_phoff);

    uint64_t load_bias = 0;
    if (ehdr->e_type == ELF_TYPE_DYN) {
        uint64_t min_vaddr = UINT64_MAX;
        int found_load = 0;
        for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
            const struct elf64_phdr* phdr = &phdrs[i];
            if (phdr->p_type != ELF_PT_LOAD || phdr->p_memsz == 0) continue;
            uint64_t seg_start = align_down_4k(phdr->p_vaddr);
            if (seg_start < min_vaddr) min_vaddr = seg_start;
            found_load = 1;
        }
        if (!found_load) return -1;
        if (USER_ELF_DYN_BASE < min_vaddr) return -1;
        load_bias = USER_ELF_DYN_BASE - min_vaddr;
    }

    static struct mapped_page pages[MAX_LOADER_PAGES];
    size_t page_count = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const struct elf64_phdr* phdr = &phdrs[i];
        if (phdr->p_type != ELF_PT_LOAD || phdr->p_memsz == 0) continue;

        if (phdr->p_filesz > phdr->p_memsz) return -1;
        if (phdr->p_offset + phdr->p_filesz > image_size || phdr->p_offset + phdr->p_filesz < phdr->p_offset) return -1;

        uint64_t seg_vaddr = 0;
        uint64_t seg_file_end = 0;
        uint64_t seg_mem_end = 0;
        if (add_u64_overflow(phdr->p_vaddr, load_bias, &seg_vaddr)) return -1;
        if (add_u64_overflow(seg_vaddr, phdr->p_filesz, &seg_file_end)) return -1;
        if (add_u64_overflow(seg_vaddr, phdr->p_memsz, &seg_mem_end)) return -1;

        uint64_t seg_start = align_down_4k(seg_vaddr);
        uint64_t seg_end = align_up_4k(seg_mem_end);

        uint64_t map_flags = PAGE_PRESENT | PAGE_USER;
        if (phdr->p_flags & ELF_PF_W) {
            map_flags |= PAGE_WRITABLE;
        }

        for (uint64_t va = seg_start; va < seg_end; va += 4096) {
            void* phys = pmm_alloc_block();
            if (!phys) return -1;

            local_memset(phys, 0, 4096);
            if (record_page(pages, &page_count, va, (uint8_t*)phys) != 0) return -1;

            uint64_t page_start = va;
            uint64_t page_end = va + 4096;
            uint64_t copy_start_va = (page_start > seg_vaddr) ? page_start : seg_vaddr;
            uint64_t copy_end_va = min_u64(page_end, seg_file_end);

            if (copy_end_va > copy_start_va) {
                uint64_t src_off = phdr->p_offset + (copy_start_va - seg_vaddr);
                uint64_t dst_off = copy_start_va - page_start;
                uint64_t copy_len = copy_end_va - copy_start_va;
                local_memcpy((uint8_t*)phys + dst_off, image + src_off, (size_t)copy_len);
            }

            if (page_start >= seg_mem_end) continue;
            vmm_map_page(pml4, va, (uint64_t)phys, map_flags);
        }
    }

    int applied_section_relocs = 0;
    if (apply_section_relocations(ehdr, image, image_size, load_bias, pages, page_count, &applied_section_relocs) != 0) {
        return -1;
    }
    if (!applied_section_relocs) {
        if (apply_dynamic_relocations(phdrs, ehdr->e_phnum, load_bias, pages, page_count) != 0) return -1;
    }

    uint64_t entry = 0;
    if (add_u64_overflow(ehdr->e_entry, load_bias, &entry)) return -1;
    *entry_out = entry;
    return 0;
}
