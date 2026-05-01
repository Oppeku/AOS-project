/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <ext4.h>

#define EXT4_SUPER_OFFSET 1024
#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_ROOT_INO 2
#define EXT4_EXTENTS_FL 0x00080000U
#define EXT4_EXTENT_MAGIC 0xF30A
#define EXT4_S_IFDIR 0x4000
#define EXT4_S_IFREG 0x8000
#define EXT4_FT_REG_FILE 1
#define EXT4_FT_DIR 2
#define EXT4_MODE_FILE 0100644U

#define LINUX_DTYPE_REG 8
#define LINUX_DTYPE_DIR 4

struct ext4_super_block {
    uint32_t inodes_count;
    uint32_t blocks_count_lo;
    uint32_t reserved_blocks_count_lo;
    uint32_t free_blocks_count_lo;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_cluster_size;
    uint32_t blocks_per_group;
    uint32_t clusters_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algorithm_usage_bitmap;
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t reserved_gdt_blocks;
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t jnl_backup_type;
    uint16_t desc_size;
} __attribute__((packed));

struct ext4_group_desc {
    uint32_t block_bitmap_lo;
    uint32_t inode_bitmap_lo;
    uint32_t inode_table_lo;
    uint16_t free_blocks_count_lo;
    uint16_t free_inodes_count_lo;
    uint16_t used_dirs_count_lo;
    uint16_t flags;
    uint32_t exclude_bitmap_lo;
    uint16_t block_bitmap_csum_lo;
    uint16_t inode_bitmap_csum_lo;
    uint16_t itable_unused_lo;
    uint16_t checksum;
} __attribute__((packed));

struct ext4_inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lo;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks_lo;
    uint32_t flags;
    uint32_t osd1;
    uint8_t block[60];
    uint32_t generation;
    uint32_t file_acl_lo;
    uint32_t size_high;
} __attribute__((packed));

struct ext4_extent_header {
    uint16_t magic;
    uint16_t entries;
    uint16_t max;
    uint16_t depth;
    uint32_t generation;
} __attribute__((packed));

struct ext4_extent {
    uint32_t block;
    uint16_t len;
    uint16_t start_hi;
    uint32_t start_lo;
} __attribute__((packed));

struct ext4_extent_idx {
    uint32_t block;
    uint32_t leaf_lo;
    uint16_t leaf_hi;
    uint16_t unused;
} __attribute__((packed));

struct ext4_volume {
    uint8_t* image;
    uint64_t image_size;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint16_t inode_size;
    uint16_t desc_size;
    uint32_t group_desc_table_block;
    uint8_t ready;
};

static struct ext4_volume g_ext4;

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) *d++ = (uint8_t)value;
}

static void local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}

static int local_str_eq_len(const char* a, const char* b, size_t b_len) {
    size_t i = 0;
    while (i < b_len) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0';
}

static void copy_string(char* dst, size_t dst_size, const char* src, size_t src_len) {
    size_t i = 0;
    if (!dst || dst_size == 0) return;
    while (i < src_len && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static size_t local_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len]) len++;
    return len;
}

static uint16_t dirent_rec_len(size_t name_len) {
    return (uint16_t)((8 + name_len + 3) & ~3U);
}

static uint32_t read_u32(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_u32(void* ptr, uint32_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_u16(void* ptr, uint16_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static int path_starts_with_component(const char* path, const char* prefix) {
    size_t i = 0;
    while (prefix[i]) {
        if (path[i] != prefix[i]) return 0;
        i++;
    }
    return path[i] == '\0' || path[i] == '/';
}

static const char* normalize_path(const char* path) {
    while (path && *path == '/') path++;
    return path ? path : "";
}

static int ext4_inner_from_path(const char* path, const char** inner_out) {
    const char* p = normalize_path(path);
    if (path_starts_with_component(p, "ext4")) {
        p += 4;
    } else if (path_starts_with_component(p, "mnt/ext4")) {
        p += 8;
    } else {
        return -1;
    }
    while (*p == '/') p++;
    *inner_out = p;
    return 0;
}

static int range_ok(uint64_t offset, uint64_t size) {
    return offset <= g_ext4.image_size && size <= g_ext4.image_size - offset;
}

static struct ext4_super_block* super_block_mut(void) {
    if (!range_ok(EXT4_SUPER_OFFSET, sizeof(struct ext4_super_block))) return NULL;
    return (struct ext4_super_block*)(g_ext4.image + EXT4_SUPER_OFFSET);
}

static struct ext4_group_desc* group_desc_mut(uint32_t group) {
    uint64_t off = (uint64_t)g_ext4.group_desc_table_block * g_ext4.block_size +
                   (uint64_t)group * g_ext4.desc_size;
    if (!range_ok(off, sizeof(struct ext4_group_desc))) return NULL;
    return (struct ext4_group_desc*)(g_ext4.image + off);
}

static const uint8_t* block_ptr(uint64_t block) {
    uint64_t offset = block * (uint64_t)g_ext4.block_size;
    if (!range_ok(offset, g_ext4.block_size)) return NULL;
    return g_ext4.image + offset;
}

static uint8_t* block_mut_ptr(uint64_t block) {
    uint64_t offset = block * (uint64_t)g_ext4.block_size;
    if (!range_ok(offset, g_ext4.block_size)) return NULL;
    return g_ext4.image + offset;
}

static uint64_t inode_size_bytes(const struct ext4_inode* inode) {
    uint64_t size = inode->size_lo;
    if ((inode->mode & 0xF000) == EXT4_S_IFREG) {
        size |= ((uint64_t)inode->size_high << 32);
    }
    return size;
}

static int inode_offset(uint32_t ino, uint64_t* out_offset) {
    if (!g_ext4.ready || ino == 0 || !out_offset) return -1;
    uint32_t group = (ino - 1) / g_ext4.inodes_per_group;
    uint32_t index = (ino - 1) % g_ext4.inodes_per_group;
    uint64_t desc_offset = (uint64_t)g_ext4.group_desc_table_block * g_ext4.block_size +
                           (uint64_t)group * g_ext4.desc_size;
    if (!range_ok(desc_offset, sizeof(struct ext4_group_desc))) return -1;

    const struct ext4_group_desc* desc = (const struct ext4_group_desc*)(g_ext4.image + desc_offset);
    uint64_t inode_offset = (uint64_t)desc->inode_table_lo * g_ext4.block_size +
                            (uint64_t)index * g_ext4.inode_size;
    if (!range_ok(inode_offset, sizeof(struct ext4_inode))) return -1;
    *out_offset = inode_offset;
    return 0;
}

static int read_inode(uint32_t ino, struct ext4_inode* out) {
    uint64_t off = 0;

    if (!out || inode_offset(ino, &off) != 0) return -1;

    local_memset(out, 0, sizeof(*out));
    local_memcpy(out, g_ext4.image + off, sizeof(*out));
    return 0;
}

static int write_inode(uint32_t ino, const struct ext4_inode* inode) {
    uint64_t off = 0;

    if (!inode || inode_offset(ino, &off) != 0) return -1;
    local_memcpy(g_ext4.image + off, inode, sizeof(*inode));
    return 0;
}

static int bitmap_test(const uint8_t* bitmap, uint32_t index) {
    return (bitmap[index / 8] >> (index % 8)) & 1U;
}

static void bitmap_set(uint8_t* bitmap, uint32_t index) {
    bitmap[index / 8] |= (uint8_t)(1U << (index % 8));
}

static void decrement_free_counts(uint32_t group, int inode_count, int block_count) {
    struct ext4_super_block* sb = super_block_mut();
    struct ext4_group_desc* gd = group_desc_mut(group);

    if (sb) {
        if (inode_count && sb->free_inodes_count > 0) sb->free_inodes_count--;
        if (block_count && sb->free_blocks_count_lo > 0) sb->free_blocks_count_lo--;
    }
    if (gd) {
        if (inode_count && gd->free_inodes_count_lo > 0) gd->free_inodes_count_lo--;
        if (block_count && gd->free_blocks_count_lo > 0) gd->free_blocks_count_lo--;
    }
}

static int alloc_inode(uint32_t* out_ino) {
    uint32_t groups;
    struct ext4_super_block* sb;

    if (!out_ino || !g_ext4.ready || !g_ext4.inodes_per_group) return -1;
    sb = super_block_mut();
    if (!sb) return -1;
    groups = (sb->inodes_count + g_ext4.inodes_per_group - 1) / g_ext4.inodes_per_group;

    for (uint32_t group = 0; group < groups; group++) {
        struct ext4_group_desc* gd = group_desc_mut(group);
        uint8_t* bitmap;
        uint32_t max_in_group = g_ext4.inodes_per_group;
        if (!gd || gd->free_inodes_count_lo == 0) continue;
        bitmap = block_mut_ptr(gd->inode_bitmap_lo);
        if (!bitmap) return -1;
        if (group == groups - 1) {
            uint32_t used_before = group * g_ext4.inodes_per_group;
            if (sb->inodes_count > used_before) {
                max_in_group = sb->inodes_count - used_before;
            }
        }
        for (uint32_t i = 0; i < max_in_group; i++) {
            uint32_t ino = group * g_ext4.inodes_per_group + i + 1;
            if (ino < sb->first_ino) continue;
            if (!bitmap_test(bitmap, i)) {
                bitmap_set(bitmap, i);
                decrement_free_counts(group, 1, 0);
                *out_ino = ino;
                return 0;
            }
        }
    }
    return -1;
}

static int alloc_block(uint32_t* out_block) {
    uint32_t groups;
    struct ext4_super_block* sb;

    if (!out_block || !g_ext4.ready || !g_ext4.blocks_per_group) return -1;
    sb = super_block_mut();
    if (!sb) return -1;
    groups = (sb->blocks_count_lo + g_ext4.blocks_per_group - 1) / g_ext4.blocks_per_group;

    for (uint32_t group = 0; group < groups; group++) {
        struct ext4_group_desc* gd = group_desc_mut(group);
        uint8_t* bitmap;
        uint32_t max_in_group = g_ext4.blocks_per_group;
        if (!gd || gd->free_blocks_count_lo == 0) continue;
        bitmap = block_mut_ptr(gd->block_bitmap_lo);
        if (!bitmap) return -1;
        if (group == groups - 1) {
            uint32_t used_before = group * g_ext4.blocks_per_group;
            if (sb->blocks_count_lo > used_before) {
                max_in_group = sb->blocks_count_lo - used_before;
            }
        }
        for (uint32_t i = 0; i < max_in_group; i++) {
            uint32_t block = group * g_ext4.blocks_per_group + i;
            if (block < sb->first_data_block) continue;
            if (block >= sb->blocks_count_lo) break;
            if (!bitmap_test(bitmap, i)) {
                uint8_t* data;
                bitmap_set(bitmap, i);
                decrement_free_counts(group, 0, 1);
                data = block_mut_ptr(block);
                if (!data) return -1;
                local_memset(data, 0, g_ext4.block_size);
                *out_block = block;
                return 0;
            }
        }
    }
    return -1;
}

static int map_extent_leaf(const struct ext4_extent_header* eh, uint32_t logical_block, uint64_t* phys_block) {
    const struct ext4_extent* ext = (const struct ext4_extent*)((const uint8_t*)eh + sizeof(*eh));
    for (uint16_t i = 0; i < eh->entries; i++) {
        uint16_t len = ext[i].len & 0x7FFF;
        if (logical_block >= ext[i].block && logical_block < ext[i].block + len) {
            uint64_t start = ((uint64_t)ext[i].start_hi << 32) | ext[i].start_lo;
            *phys_block = start + (logical_block - ext[i].block);
            return 0;
        }
    }
    return -1;
}

static int map_extent_block(const struct ext4_extent_header* eh, uint32_t logical_block, uint64_t* phys_block) {
    if (eh->magic != EXT4_EXTENT_MAGIC || eh->entries > eh->max) return -1;
    if (eh->depth == 0) return map_extent_leaf(eh, logical_block, phys_block);
    if (eh->depth > 3) return -1;

    const struct ext4_extent_idx* idx = (const struct ext4_extent_idx*)((const uint8_t*)eh + sizeof(*eh));
    const struct ext4_extent_idx* chosen = NULL;
    for (uint16_t i = 0; i < eh->entries; i++) {
        if (logical_block >= idx[i].block) {
            chosen = &idx[i];
        } else {
            break;
        }
    }
    if (!chosen) return -1;

    uint64_t leaf_block = ((uint64_t)chosen->leaf_hi << 32) | chosen->leaf_lo;
    const uint8_t* block = block_ptr(leaf_block);
    if (!block) return -1;
    return map_extent_block((const struct ext4_extent_header*)block, logical_block, phys_block);
}

static int map_file_block(const struct ext4_inode* inode, uint32_t logical_block, uint64_t* phys_block) {
    if (inode->flags & EXT4_EXTENTS_FL) {
        return map_extent_block((const struct ext4_extent_header*)inode->block, logical_block, phys_block);
    }
    if (logical_block < 12) {
        const uint32_t* direct = (const uint32_t*)inode->block;
        if (direct[logical_block] == 0) return -1;
        *phys_block = direct[logical_block];
        return 0;
    }
    return -1;
}

static int map_file_block_for_write(struct ext4_inode* inode, uint32_t logical_block, uint64_t* phys_block) {
    uint32_t* direct = (uint32_t*)inode->block;
    uint32_t block = 0;

    if (!inode || !phys_block) return -1;
    if (inode->flags & EXT4_EXTENTS_FL) {
        return map_file_block(inode, logical_block, phys_block);
    }
    if (logical_block >= 12) {
        return -1;
    }
    if (direct[logical_block] == 0) {
        if (alloc_block(&block) != 0) return -1;
        direct[logical_block] = block;
        inode->blocks_lo += g_ext4.block_size / 512U;
    }
    *phys_block = direct[logical_block];
    return 0;
}

static int read_inode_data(const struct ext4_inode* inode, uint64_t offset, uint8_t* buffer, uint64_t len) {
    uint64_t size = inode_size_bytes(inode);
    uint64_t done = 0;

    if (offset > size || len > size - offset) return -1;
    while (done < len) {
        uint64_t absolute = offset + done;
        uint32_t logical_block = (uint32_t)(absolute / g_ext4.block_size);
        uint32_t in_block = (uint32_t)(absolute % g_ext4.block_size);
        uint32_t chunk = g_ext4.block_size - in_block;
        uint64_t phys_block = 0;
        const uint8_t* src;

        if (chunk > len - done) chunk = (uint32_t)(len - done);
        if (map_file_block(inode, logical_block, &phys_block) != 0) return -1;
        src = block_ptr(phys_block);
        if (!src) return -1;
        local_memcpy(buffer + done, src + in_block, chunk);
        done += chunk;
    }
    return 0;
}

static int write_inode_data_existing(const struct ext4_inode* inode, uint64_t offset, const uint8_t* buffer, uint64_t len) {
    uint64_t done = 0;

    while (done < len) {
        uint64_t absolute = offset + done;
        uint32_t logical_block = (uint32_t)(absolute / g_ext4.block_size);
        uint32_t in_block = (uint32_t)(absolute % g_ext4.block_size);
        uint32_t chunk = g_ext4.block_size - in_block;
        uint64_t phys_block = 0;
        uint8_t* dst;

        if (chunk > len - done) chunk = (uint32_t)(len - done);
        if (map_file_block_for_write((struct ext4_inode*)inode, logical_block, &phys_block) != 0) return -1;
        dst = block_mut_ptr(phys_block);
        if (!dst) return -1;
        local_memcpy(dst + in_block, buffer + done, chunk);
        done += chunk;
    }
    return 0;
}

static void inode_set_size(struct ext4_inode* inode, uint64_t size) {
    inode->size_lo = (uint32_t)size;
    if ((inode->mode & 0xF000) == EXT4_S_IFREG) {
        inode->size_high = (uint32_t)(size >> 32);
    }
}

static void split_parent_leaf(const char* inner, char* parent, size_t parent_size, char* leaf, size_t leaf_size) {
    size_t len = local_strlen(inner);
    size_t slash = len;

    while (slash > 0 && inner[slash - 1] != '/') slash--;
    if (slash == 0) {
        copy_string(parent, parent_size, "", 0);
        copy_string(leaf, leaf_size, inner, len);
        return;
    }
    copy_string(parent, parent_size, inner, slash - 1);
    copy_string(leaf, leaf_size, inner + slash, len - slash);
}

static int add_dir_entry(uint32_t dir_ino, const char* name, uint32_t child_ino, uint8_t file_type) {
    struct ext4_inode dir;
    uint64_t dir_size;
    uint64_t off = 0;
    size_t name_len = local_strlen(name);
    uint16_t needed = dirent_rec_len(name_len);
    uint8_t block_buf[4096];

    if (!name || name_len == 0 || name_len > 255) return -1;
    if (read_inode(dir_ino, &dir) != 0 || (dir.mode & 0xF000) != EXT4_S_IFDIR) return -1;
    if (g_ext4.block_size > sizeof(block_buf)) return -1;
    dir_size = inode_size_bytes(&dir);

    while (off < dir_size) {
        uint64_t remaining = dir_size - off;
        uint32_t chunk = remaining < g_ext4.block_size ? (uint32_t)remaining : g_ext4.block_size;
        uint32_t pos = 0;
        if (read_inode_data(&dir, off, block_buf, chunk) != 0) return -1;

        while (pos + 8 <= chunk) {
            uint32_t ino = read_u32(block_buf + pos);
            uint16_t rec_len = read_u16(block_buf + pos + 4);
            uint8_t existing_name_len = block_buf[pos + 6];
            uint16_t actual_len = ino ? dirent_rec_len(existing_name_len) : 8;

            if (rec_len < 8 || pos + rec_len > chunk) break;

            if (ino == 0 && rec_len >= needed) {
                write_u32(block_buf + pos, child_ino);
                write_u16(block_buf + pos + 4, rec_len);
                block_buf[pos + 6] = (uint8_t)name_len;
                block_buf[pos + 7] = file_type;
                local_memcpy(block_buf + pos + 8, name, name_len);
                return write_inode_data_existing(&dir, off, block_buf, chunk);
            }

            if (rec_len >= actual_len + needed) {
                uint16_t new_rec_len = rec_len - actual_len;
                uint32_t new_pos = pos + actual_len;
                write_u16(block_buf + pos + 4, actual_len);
                write_u32(block_buf + new_pos, child_ino);
                write_u16(block_buf + new_pos + 4, new_rec_len);
                block_buf[new_pos + 6] = (uint8_t)name_len;
                block_buf[new_pos + 7] = file_type;
                local_memcpy(block_buf + new_pos + 8, name, name_len);
                return write_inode_data_existing(&dir, off, block_buf, chunk);
            }

            pos += rec_len;
        }
        off += chunk;
    }

    return -1;
}

static int find_dir_entry(uint32_t dir_ino, const char* name, uint32_t* out_ino, uint8_t* out_type) {
    struct ext4_inode dir;
    uint64_t size;
    uint64_t off = 0;
    uint8_t block_buf[4096];

    if (read_inode(dir_ino, &dir) != 0 || (dir.mode & 0xF000) != EXT4_S_IFDIR) return -1;
    if (g_ext4.block_size > sizeof(block_buf)) return -1;
    size = inode_size_bytes(&dir);

    while (off < size) {
        uint64_t remaining = size - off;
        uint32_t chunk = remaining < g_ext4.block_size ? (uint32_t)remaining : g_ext4.block_size;
        uint32_t pos = 0;
        if (read_inode_data(&dir, off, block_buf, chunk) != 0) return -1;
        while (pos + 8 <= chunk) {
            uint32_t ino = *(uint32_t*)(block_buf + pos);
            uint16_t rec_len = *(uint16_t*)(block_buf + pos + 4);
            uint8_t name_len = *(uint8_t*)(block_buf + pos + 6);
            uint8_t file_type = *(uint8_t*)(block_buf + pos + 7);
            if (rec_len < 8 || pos + rec_len > chunk) break;
            if (ino && name_len <= rec_len - 8 && local_str_eq_len(name, (const char*)block_buf + pos + 8, name_len)) {
                if (out_ino) *out_ino = ino;
                if (out_type) *out_type = file_type;
                return 0;
            }
            pos += rec_len;
        }
        off += chunk;
    }
    return -1;
}

static int resolve_path(const char* path, uint32_t* out_ino, struct ext4_inode* out_inode) {
    const char* inner = NULL;
    uint32_t current = EXT4_ROOT_INO;
    struct ext4_inode inode;

    if (ext4_inner_from_path(path, &inner) != 0) return -1;
    if (read_inode(current, &inode) != 0) return -1;

    while (*inner) {
        char component[128];
        size_t len = 0;
        uint32_t next_ino = 0;

        while (*inner == '/') inner++;
        if (!*inner) break;
        while (inner[len] && inner[len] != '/') len++;
        if (len == 0 || len >= sizeof(component)) return -1;
        copy_string(component, sizeof(component), inner, len);
        if (find_dir_entry(current, component, &next_ino, NULL) != 0) return -1;
        current = next_ino;
        if (read_inode(current, &inode) != 0) return -1;
        inner += len;
    }

    if (out_ino) *out_ino = current;
    if (out_inode) local_memcpy(out_inode, &inode, sizeof(inode));
    return 0;
}

int ext4_init(uint32_t mod_start, uint32_t mod_end) {
    const struct ext4_super_block* sb;
    uint32_t block_size;

    local_memset(&g_ext4, 0, sizeof(g_ext4));
    if (mod_end <= mod_start || (uint64_t)(mod_end - mod_start) < EXT4_SUPER_OFFSET + sizeof(*sb)) {
        return -1;
    }

    sb = (const struct ext4_super_block*)(uintptr_t)(mod_start + EXT4_SUPER_OFFSET);
    if (sb->magic != EXT4_SUPER_MAGIC || sb->log_block_size > 2) {
        return -1;
    }

    block_size = 1024U << sb->log_block_size;
    g_ext4.image = (uint8_t*)(uintptr_t)mod_start;
    g_ext4.image_size = (uint64_t)(mod_end - mod_start);
    g_ext4.block_size = block_size;
    g_ext4.blocks_per_group = sb->blocks_per_group;
    g_ext4.inodes_per_group = sb->inodes_per_group;
    g_ext4.inode_size = sb->inode_size ? sb->inode_size : 128;
    g_ext4.desc_size = sb->desc_size >= sizeof(struct ext4_group_desc) ? sb->desc_size : sizeof(struct ext4_group_desc);
    g_ext4.group_desc_table_block = (block_size == 1024) ? 2 : 1;
    if (!g_ext4.blocks_per_group || !g_ext4.inodes_per_group || g_ext4.block_size > 4096) {
        local_memset(&g_ext4, 0, sizeof(g_ext4));
        return -1;
    }
    g_ext4.ready = 1;
    return 0;
}

int ext4_is_ready(void) {
    return g_ext4.ready;
}

int ext4_is_ext4_path(const char* path) {
    const char* inner;
    return path && ext4_inner_from_path(path, &inner) == 0;
}

int ext4_lookup_path(const char* path, uint8_t* is_dir, uint32_t* inode_no, uint32_t* size) {
    struct ext4_inode inode;
    uint32_t ino = 0;
    uint16_t kind;

    if (!g_ext4.ready || resolve_path(path, &ino, &inode) != 0) return -1;
    kind = inode.mode & 0xF000;
    if (kind != EXT4_S_IFDIR && kind != EXT4_S_IFREG) return -1;
    if (is_dir) *is_dir = (kind == EXT4_S_IFDIR);
    if (inode_no) *inode_no = ino;
    if (size) *size = (uint32_t)inode_size_bytes(&inode);
    return 0;
}

int ext4_read_file(uint32_t inode_no, uint32_t file_size, uint64_t offset, uint8_t* buffer, uint64_t len) {
    struct ext4_inode inode;
    (void)file_size;
    if (!buffer || read_inode(inode_no, &inode) != 0 || (inode.mode & 0xF000) != EXT4_S_IFREG) return -1;
    return read_inode_data(&inode, offset, buffer, len);
}

int ext4_create_path(const char* path, uint8_t* is_dir, uint32_t* inode_no, uint32_t* size) {
    const char* inner = NULL;
    char parent_inner[256];
    char parent_path[256];
    char leaf[128];
    struct ext4_inode parent_inode;
    struct ext4_inode new_inode;
    uint32_t parent_ino = 0;
    uint32_t new_ino = 0;
    size_t parent_len;

    if (!path || !is_dir || !inode_no || !size) return -1;
    if (ext4_inner_from_path(path, &inner) != 0 || inner[0] == '\0') return -1;

    split_parent_leaf(inner, parent_inner, sizeof(parent_inner), leaf, sizeof(leaf));
    if (leaf[0] == '\0') return -1;

    copy_string(parent_path, sizeof(parent_path), "ext4", 4);
    parent_len = 4;
    if (parent_inner[0] != '\0') {
        if (parent_len + 1 >= sizeof(parent_path)) return -1;
        parent_path[parent_len++] = '/';
        parent_path[parent_len] = '\0';
        copy_string(parent_path + parent_len, sizeof(parent_path) - parent_len, parent_inner, local_strlen(parent_inner));
    }

    if (resolve_path(parent_path, &parent_ino, &parent_inode) != 0 || (parent_inode.mode & 0xF000) != EXT4_S_IFDIR) {
        return -1;
    }
    if (find_dir_entry(parent_ino, leaf, NULL, NULL) == 0) {
        return -1;
    }
    if (alloc_inode(&new_ino) != 0) {
        return -1;
    }

    local_memset(&new_inode, 0, sizeof(new_inode));
    new_inode.mode = EXT4_MODE_FILE;
    new_inode.links_count = 1;
    new_inode.blocks_lo = 0;
    inode_set_size(&new_inode, 0);
    if (write_inode(new_ino, &new_inode) != 0) {
        return -1;
    }
    if (add_dir_entry(parent_ino, leaf, new_ino, EXT4_FT_REG_FILE) != 0) {
        return -1;
    }

    *is_dir = 0;
    *inode_no = new_ino;
    *size = 0;
    return 0;
}

int ext4_truncate_path(const char* path, uint32_t* size) {
    struct ext4_inode inode;
    uint32_t ino = 0;

    if (!size || resolve_path(path, &ino, &inode) != 0 || (inode.mode & 0xF000) != EXT4_S_IFREG) {
        return -1;
    }

    inode_set_size(&inode, 0);
    if (write_inode(ino, &inode) != 0) {
        return -1;
    }

    *size = 0;
    return 0;
}

int ext4_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size) {
    struct ext4_inode inode;
    uint32_t ino = 0;
    uint64_t old_size;
    uint64_t end;

    if (!path || !buffer || !written) {
        return -1;
    }
    *written = 0;

    if (resolve_path(path, &ino, &inode) != 0 || (inode.mode & 0xF000) != EXT4_S_IFREG) {
        return -1;
    }
    if (len == 0) {
        if (new_size) *new_size = (uint32_t)inode_size_bytes(&inode);
        return 0;
    }
    if (offset > UINT64_MAX - len) {
        return -1;
    }

    old_size = inode_size_bytes(&inode);
    end = offset + len;
    if (end > UINT32_MAX) {
        return -1;
    }

    if (offset > old_size) {
        uint8_t zero = 0;
        for (uint64_t pos = old_size; pos < offset; pos++) {
            if (write_inode_data_existing(&inode, pos, &zero, 1) != 0) {
                return -1;
            }
        }
    }

    if (write_inode_data_existing(&inode, offset, buffer, len) != 0) {
        return -1;
    }

    *written = len;
    if (end > old_size) {
        inode_set_size(&inode, end);
        if (write_inode(ino, &inode) != 0) {
            return -1;
        }
    }

    if (new_size) {
        *new_size = (uint32_t)((end > old_size) ? end : old_size);
    }
    return 0;
}

int ext4_dirent_at_index(const char* path, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    struct ext4_inode dir;
    uint32_t dir_ino = 0;
    uint64_t dir_size;
    uint64_t off = 0;
    uint64_t seen = 0;
    uint8_t block_buf[4096];

    if (!g_ext4.ready || !name_buf || name_buf_size == 0) return -1;
    if (resolve_path(path, &dir_ino, &dir) != 0 || (dir.mode & 0xF000) != EXT4_S_IFDIR) return -1;
    if (g_ext4.block_size > sizeof(block_buf)) return -1;
    dir_size = inode_size_bytes(&dir);

    while (off < dir_size) {
        uint64_t remaining = dir_size - off;
        uint32_t chunk = remaining < g_ext4.block_size ? (uint32_t)remaining : g_ext4.block_size;
        uint32_t pos = 0;
        if (read_inode_data(&dir, off, block_buf, chunk) != 0) return -1;
        while (pos + 8 <= chunk) {
            uint32_t ino = *(uint32_t*)(block_buf + pos);
            uint16_t rec_len = *(uint16_t*)(block_buf + pos + 4);
            uint8_t name_len = *(uint8_t*)(block_buf + pos + 6);
            uint8_t file_type = *(uint8_t*)(block_buf + pos + 7);
            if (rec_len < 8 || pos + rec_len > chunk) break;
            if (ino && name_len <= rec_len - 8) {
                if (seen == index) {
                    struct ext4_inode child;
                    copy_string(name_buf, name_buf_size, (const char*)block_buf + pos + 8, name_len);
                    if (read_inode(ino, &child) == 0) {
                        if (size) *size = (uint32_t)inode_size_bytes(&child);
                    } else if (size) {
                        *size = 0;
                    }
                    if (d_type) *d_type = (file_type == 2) ? LINUX_DTYPE_DIR : LINUX_DTYPE_REG;
                    return 0;
                }
                seen++;
            }
            pos += rec_len;
        }
        off += chunk;
    }
    return -1;
}
