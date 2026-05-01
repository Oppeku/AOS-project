/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <fat32.h>
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);

struct fat32_boot_sector {
    uint8_t jump[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed));

struct fat32_dirent_raw {
    uint8_t name[11];
    uint8_t attr;
    uint8_t nt_reserved;
    uint8_t creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __attribute__((packed));

struct fat32_volume {
    uint8_t* image;
    uint32_t image_size;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t fat_size_sectors;
    uint32_t root_cluster;
    uint32_t fat_start;
    uint32_t data_start;
    uint8_t mounted;
};

struct fat32_node {
    uint32_t first_cluster;
    uint32_t size;
    uint8_t is_dir;
};

static struct fat32_volume g_volume;

static int fat32_has_prefix(const char* path);

static void local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
}

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static int local_strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) {
            return (unsigned char)ca - (unsigned char)cb;
        }
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static const char* skip_leading_slashes(const char* path) {
    while (path && *path == '/') {
        path++;
    }
    return path ? path : "";
}

static uint32_t read_u32(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32(void* ptr, uint32_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t cluster_size_bytes(void) {
    return g_volume.bytes_per_sector * g_volume.sectors_per_cluster;
}

static uint32_t cluster_to_offset(uint32_t cluster) {
    return g_volume.data_start + (cluster - 2U) * cluster_size_bytes();
}

static int volume_ok(void) {
    return g_volume.mounted && g_volume.image && g_volume.bytes_per_sector && g_volume.sectors_per_cluster;
}

static uint32_t first_cluster_from_entry(const struct fat32_dirent_raw* entry) {
    return ((uint32_t)entry->first_cluster_hi << 16) | (uint32_t)entry->first_cluster_lo;
}

static int cluster_is_eoc(uint32_t cluster) {
    return cluster >= 0x0FFFFFF8U;
}

static int fat32_next_cluster(uint32_t cluster, uint32_t* next_cluster) {
    uint64_t fat_offset = (uint64_t)g_volume.fat_start + ((uint64_t)cluster * 4ULL);
    if (!g_volume.image || fat_offset + 4ULL > g_volume.image_size) {
        return -1;
    }

    uint32_t value = read_u32(g_volume.image + fat_offset) & 0x0FFFFFFFU;
    if (value == 0x0FFFFFF7U) {
        return -1;
    }
    *next_cluster = value;
    return 0;
}

static int fat32_set_cluster(uint32_t cluster, uint32_t value) {
    for (uint32_t fat = 0; fat < g_volume.fat_count; fat++) {
        uint64_t fat_offset = (uint64_t)g_volume.fat_start +
            (uint64_t)fat * g_volume.fat_size_sectors * g_volume.bytes_per_sector +
            (uint64_t)cluster * 4ULL;
        if (fat_offset + 4ULL > g_volume.image_size) {
            return -1;
        }
        write_u32(g_volume.image + fat_offset, value & 0x0FFFFFFFU);
    }
    return 0;
}

static int fat32_get_cluster(uint32_t cluster, uint32_t* value) {
    uint64_t fat_offset = (uint64_t)g_volume.fat_start + (uint64_t)cluster * 4ULL;
    if (!value || fat_offset + 4ULL > g_volume.image_size) {
        return -1;
    }
    *value = read_u32(g_volume.image + fat_offset) & 0x0FFFFFFFU;
    return 0;
}

static uint32_t max_cluster_count(void) {
    if (g_volume.data_start >= g_volume.image_size) {
        return 0;
    }
    return ((g_volume.image_size - g_volume.data_start) / cluster_size_bytes()) + 2U;
}

static int fat32_find_free_cluster(uint32_t* out) {
    uint32_t max_cluster = max_cluster_count();
    for (uint32_t cluster = 2; cluster < max_cluster; cluster++) {
        uint32_t value = 0;
        if (fat32_get_cluster(cluster, &value) != 0) {
            return -1;
        }
        if (value == 0) {
            *out = cluster;
            return 0;
        }
    }
    return -1;
}

static int fat32_zero_cluster(uint32_t cluster) {
    uint32_t offset = cluster_to_offset(cluster);
    uint32_t bytes = cluster_size_bytes();
    if ((uint64_t)offset + bytes > g_volume.image_size) {
        return -1;
    }
    local_memset(g_volume.image + offset, 0, bytes);
    return 0;
}

static int fat32_read_cluster(uint32_t cluster, uint32_t cluster_offset, uint8_t* buffer, uint32_t len) {
    uint32_t offset = cluster_to_offset(cluster) + cluster_offset;
    if ((uint64_t)offset + len > g_volume.image_size) {
        return -1;
    }
    local_memcpy(buffer, g_volume.image + offset, len);
    return 0;
}

static int fat32_write_cluster(uint32_t cluster, uint32_t cluster_offset, const uint8_t* buffer, uint32_t len) {
    uint32_t offset = cluster_to_offset(cluster) + cluster_offset;
    if ((uint64_t)offset + len > g_volume.image_size) {
        return -1;
    }
    local_memcpy(g_volume.image + offset, buffer, len);
    return 0;
}

static void short_name_to_string(const uint8_t name[11], char* out, size_t out_size) {
    size_t pos = 0;
    if (out_size == 0) {
        return;
    }

    for (size_t i = 0; i < 8 && name[i] != ' '; i++) {
        if (pos + 1 >= out_size) {
            break;
        }
        out[pos++] = (char)name[i];
    }

    size_t ext_start = 8;
    size_t ext_len = 0;
    while (ext_start + ext_len < 11 && name[ext_start + ext_len] != ' ') {
        ext_len++;
    }

    if (ext_len > 0 && pos + 1 < out_size) {
        out[pos++] = '.';
        for (size_t i = 0; i < ext_len && pos + 1 < out_size; i++) {
            out[pos++] = (char)name[8 + i];
        }
    }

    out[pos] = '\0';
}

static void trim_component(char* dst, size_t dst_size, const char* src) {
    size_t pos = 0;
    while (src[pos] && src[pos] != '/' && pos + 1 < dst_size) {
        char c = src[pos];
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        dst[pos++] = c;
    }
    dst[pos] = '\0';
}

static int is_dot_component(const char* s) {
    return s[0] == '.' && s[1] == '\0';
}

static int is_dotdot_component(const char* s) {
    return s[0] == '.' && s[1] == '.' && s[2] == '\0';
}

static int component_matches_entry(const char* component, const struct fat32_dirent_raw* entry) {
    char short_name[16];
    char upper_component[64];

    short_name_to_string(entry->name, short_name, sizeof(short_name));
    trim_component(upper_component, sizeof(upper_component), component);
    return local_strcasecmp(short_name, upper_component) == 0;
}

static int make_short_name_83(const char* leaf, uint8_t out[11]) {
    size_t name_pos = 0;
    size_t ext_pos = 0;
    int seen_dot = 0;

    for (size_t i = 0; i < 11; i++) {
        out[i] = ' ';
    }
    if (!leaf || leaf[0] == '\0' || leaf[0] == '.') {
        return -1;
    }

    for (size_t i = 0; leaf[i] != '\0'; i++) {
        char c = leaf[i];
        if (c == '/') {
            return -1;
        }
        if (c == '.') {
            if (seen_dot) return -1;
            seen_dot = 1;
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '$' || c == '~')) {
            return -1;
        }
        if (!seen_dot) {
            if (name_pos >= 8) return -1;
            out[name_pos++] = (uint8_t)c;
        } else {
            if (ext_pos >= 3) return -1;
            out[8 + ext_pos++] = (uint8_t)c;
        }
    }
    return name_pos > 0 ? 0 : -1;
}

static void split_parent_leaf(const char* inner, char* parent, size_t parent_size, char* leaf, size_t leaf_size) {
    const char* normalized = skip_leading_slashes(inner);
    size_t last_slash = 0;
    size_t len = 0;

    while (normalized[len] != '\0') {
        if (normalized[len] == '/') {
            last_slash = len + 1;
        }
        len++;
    }

    if (last_slash == 0) {
        if (parent_size > 0) parent[0] = '\0';
    } else {
        size_t copy_len = last_slash - 1;
        if (copy_len >= parent_size) copy_len = parent_size - 1;
        for (size_t i = 0; i < copy_len; i++) parent[i] = normalized[i];
        parent[copy_len] = '\0';
    }

    size_t leaf_len = 0;
    while (normalized[last_slash + leaf_len] && leaf_len + 1 < leaf_size) {
        leaf[leaf_len] = normalized[last_slash + leaf_len];
        leaf_len++;
    }
    leaf[leaf_len] = '\0';
}

static int fat32_entry_location_in_dir(uint32_t start_cluster, const char* component, uint32_t* out_cluster, uint32_t* out_offset, struct fat32_dirent_raw* out_entry) {
    uint32_t cluster = start_cluster;
    uint32_t cluster_bytes = cluster_size_bytes();
    uint32_t entries_per_cluster = cluster_bytes / sizeof(struct fat32_dirent_raw);
    uint8_t entry_buf[sizeof(struct fat32_dirent_raw)];

    while (1) {
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            uint32_t entry_offset = i * sizeof(struct fat32_dirent_raw);
            if (fat32_read_cluster(cluster, entry_offset, entry_buf, sizeof(entry_buf)) != 0) return -1;
            struct fat32_dirent_raw* entry = (struct fat32_dirent_raw*)entry_buf;
            if (entry->name[0] == 0x00) return -1;
            if (entry->name[0] == 0xE5 || entry->attr == 0x0F || (entry->attr & 0x08)) continue;
            if (!component_matches_entry(component, entry)) continue;

            if (out_cluster) *out_cluster = cluster;
            if (out_offset) *out_offset = entry_offset;
            if (out_entry) local_memcpy(out_entry, entry, sizeof(*entry));
            return 0;
        }
        if (fat32_next_cluster(cluster, &cluster) != 0 || cluster_is_eoc(cluster)) return -1;
    }
}

static int fat32_find_free_entry(uint32_t start_cluster, uint32_t* out_cluster, uint32_t* out_offset) {
    uint32_t cluster = start_cluster;
    uint32_t cluster_bytes = cluster_size_bytes();
    uint32_t entries_per_cluster = cluster_bytes / sizeof(struct fat32_dirent_raw);
    uint8_t entry_buf[sizeof(struct fat32_dirent_raw)];

    while (1) {
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            uint32_t entry_offset = i * sizeof(struct fat32_dirent_raw);
            if (fat32_read_cluster(cluster, entry_offset, entry_buf, sizeof(entry_buf)) != 0) return -1;
            struct fat32_dirent_raw* entry = (struct fat32_dirent_raw*)entry_buf;
            if (entry->name[0] == 0x00 || entry->name[0] == 0xE5) {
                *out_cluster = cluster;
                *out_offset = entry_offset;
                return 0;
            }
        }
        uint32_t next = 0;
        if (fat32_next_cluster(cluster, &next) != 0) return -1;
        if (cluster_is_eoc(next)) {
            uint32_t new_cluster = 0;
            if (fat32_find_free_cluster(&new_cluster) != 0) return -1;
            if (fat32_set_cluster(cluster, new_cluster) != 0) return -1;
            if (fat32_set_cluster(new_cluster, 0x0FFFFFFFU) != 0) return -1;
            if (fat32_zero_cluster(new_cluster) != 0) return -1;
            *out_cluster = new_cluster;
            *out_offset = 0;
            return 0;
        }
        cluster = next;
    }
}

static int fat32_find_entry_in_dir(uint32_t start_cluster, const char* component, struct fat32_node* out) {
    uint32_t cluster = start_cluster;
    uint32_t cluster_bytes = cluster_size_bytes();
    uint8_t entry_buf[sizeof(struct fat32_dirent_raw)];
    uint32_t entries_per_cluster = cluster_bytes / sizeof(struct fat32_dirent_raw);

    while (1) {
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            uint32_t entry_offset = i * sizeof(struct fat32_dirent_raw);
            if (fat32_read_cluster(cluster, entry_offset, entry_buf, sizeof(entry_buf)) != 0) {
                return -1;
            }

            struct fat32_dirent_raw* entry = (struct fat32_dirent_raw*)entry_buf;
            if (entry->name[0] == 0x00) {
                return -1;
            }
            if (entry->name[0] == 0xE5) {
                continue;
            }
            if (entry->attr == 0x0F) {
                continue;
            }
            if (entry->attr & 0x08) {
                continue;
            }
            if (!component_matches_entry(component, entry)) {
                continue;
            }

            out->first_cluster = first_cluster_from_entry(entry);
            out->size = entry->file_size;
            out->is_dir = (entry->attr & 0x10) ? 1 : 0;
            return 0;
        }

        if (fat32_next_cluster(cluster, &cluster) != 0 || cluster_is_eoc(cluster)) {
            return -1;
        }
    }
}

static int fat32_resolve_inner_path(const char* inner, struct fat32_node* out) {
    char component[64];
    const char* cursor = skip_leading_slashes(inner);
    struct fat32_node current;

    if (!volume_ok()) {
        return -1;
    }

    current.first_cluster = g_volume.root_cluster;
    current.size = 0;
    current.is_dir = 1;

    if (!cursor || cursor[0] == '\0') {
        *out = current;
        return 0;
    }

    while (cursor && *cursor) {
        const char* next = cursor;
        size_t len = 0;
        while (next[len] && next[len] != '/') {
            len++;
        }

        if (len >= sizeof(component)) {
            return -1;
        }

        for (size_t i = 0; i < len; i++) {
            char c = next[i];
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 'A');
            }
            component[i] = c;
        }
        component[len] = '\0';

        if (is_dot_component(component)) {
            /* Stay on the current node. */
        } else if (is_dotdot_component(component)) {
            current.first_cluster = g_volume.root_cluster;
            current.size = 0;
            current.is_dir = 1;
        } else {
            if (!current.is_dir) {
                return -1;
            }
            if (fat32_find_entry_in_dir(current.first_cluster, component, &current) != 0) {
                return -1;
            }
        }

        cursor += len;
        while (*cursor == '/') {
            cursor++;
        }
    }

    *out = current;
    return 0;
}

static int fat32_dirent_by_index(uint32_t start_cluster, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    uint32_t cluster = start_cluster;
    uint32_t cluster_bytes = cluster_size_bytes();
    uint32_t entries_per_cluster = cluster_bytes / sizeof(struct fat32_dirent_raw);
    uint64_t current_index = 0;
    uint8_t entry_buf[sizeof(struct fat32_dirent_raw)];

    if (index == 0) {
        if (name_buf_size >= 2) {
            name_buf[0] = '.';
            name_buf[1] = '\0';
        }
        if (size) *size = 0;
        if (d_type) *d_type = 4;
        return 0;
    }

    if (index == 1) {
        if (name_buf_size >= 3) {
            name_buf[0] = '.';
            name_buf[1] = '.';
            name_buf[2] = '\0';
        }
        if (size) *size = 0;
        if (d_type) *d_type = 4;
        return 0;
    }

    index -= 2;

    while (1) {
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            uint32_t entry_offset = i * sizeof(struct fat32_dirent_raw);
            if (fat32_read_cluster(cluster, entry_offset, entry_buf, sizeof(entry_buf)) != 0) {
                return -1;
            }

            struct fat32_dirent_raw* entry = (struct fat32_dirent_raw*)entry_buf;
            if (entry->name[0] == 0x00) {
                return -1;
            }
            if (entry->name[0] == 0xE5 || entry->attr == 0x0F || (entry->attr & 0x08)) {
                continue;
            }

            if (current_index == index) {
                short_name_to_string(entry->name, name_buf, name_buf_size);
                if (size) *size = entry->file_size;
                if (d_type) *d_type = (entry->attr & 0x10) ? 4 : 8;
                return 0;
            }
            current_index++;
        }

        if (fat32_next_cluster(cluster, &cluster) != 0 || cluster_is_eoc(cluster)) {
            return -1;
        }
    }
}

static int fat32_read_node(uint32_t first_cluster, uint32_t file_size, uint64_t offset, uint8_t* buffer, uint64_t len) {
    uint32_t cluster_bytes = cluster_size_bytes();
    if (offset >= file_size) {
        return 0;
    }

    if (offset + len > file_size) {
        len = file_size - offset;
    }

    uint32_t cluster_index = (uint32_t)(offset / cluster_bytes);
    uint32_t intra_cluster = (uint32_t)(offset % cluster_bytes);
    uint32_t cluster = first_cluster;

    for (uint32_t i = 0; i < cluster_index; i++) {
        if (fat32_next_cluster(cluster, &cluster) != 0 || cluster_is_eoc(cluster)) {
            return -1;
        }
    }

    uint64_t remaining = len;
    uint8_t* dst = buffer;
    while (remaining > 0) {
        uint32_t chunk = cluster_bytes - intra_cluster;
        if (chunk > remaining) {
            chunk = (uint32_t)remaining;
        }
        if (fat32_read_cluster(cluster, intra_cluster, dst, chunk) != 0) {
            return -1;
        }

        remaining -= chunk;
        dst += chunk;
        intra_cluster = 0;
        if (remaining == 0) {
            break;
        }

        if (fat32_next_cluster(cluster, &cluster) != 0 || cluster_is_eoc(cluster)) {
            return -1;
        }
    }

    return 0;
}

static int fat32_get_cluster_for_offset(uint32_t* first_cluster, uint32_t cluster_index, int allocate, uint32_t* out_cluster) {
    uint32_t cluster = *first_cluster;
    if (cluster == 0) {
        if (!allocate) return -1;
        if (fat32_find_free_cluster(&cluster) != 0) return -1;
        if (fat32_set_cluster(cluster, 0x0FFFFFFFU) != 0) return -1;
        if (fat32_zero_cluster(cluster) != 0) return -1;
        *first_cluster = cluster;
    }

    for (uint32_t i = 0; i < cluster_index; i++) {
        uint32_t next = 0;
        if (fat32_next_cluster(cluster, &next) != 0) return -1;
        if (cluster_is_eoc(next)) {
            if (!allocate) return -1;
            if (fat32_find_free_cluster(&next) != 0) return -1;
            if (fat32_set_cluster(cluster, next) != 0) return -1;
            if (fat32_set_cluster(next, 0x0FFFFFFFU) != 0) return -1;
            if (fat32_zero_cluster(next) != 0) return -1;
        }
        cluster = next;
    }
    *out_cluster = cluster;
    return 0;
}

static int fat32_update_dirent(uint32_t entry_cluster, uint32_t entry_offset, const struct fat32_dirent_raw* entry) {
    return fat32_write_cluster(entry_cluster, entry_offset, (const uint8_t*)entry, sizeof(*entry));
}

static int fat32_inner_from_path(const char* path, const char** out_inner) {
    const char* inner = skip_leading_slashes(path);
    if (!fat32_has_prefix(inner)) return -1;
    inner += 5;
    if (*inner == '/') inner++;
    else if (*inner != '\0') return -1;
    *out_inner = inner;
    return 0;
}

static int fat32_has_prefix(const char* path) {
    const char* p = skip_leading_slashes(path);
    return p[0] == 'f' && p[1] == 'a' && p[2] == 't' && p[3] == '3' && p[4] == '2' &&
           (p[5] == '\0' || p[5] == '/');
}

int fat32_init(uint32_t mod_start, uint32_t mod_end) {
    local_memset(&g_volume, 0, sizeof(g_volume));
    if (mod_start >= mod_end) {
        serial_print("FAT32: invalid module bounds\n");
        return -1;
    }

    const struct fat32_boot_sector* bs = (const struct fat32_boot_sector*)(uintptr_t)mod_start;
    if (bs->bytes_per_sector == 0 || bs->sectors_per_cluster == 0 || bs->fat_size_32 == 0) {
        serial_print("FAT32: invalid boot sector\n");
        return -1;
    }
    if (mod_end - mod_start < 512 || ((const uint8_t*)(uintptr_t)mod_start)[510] != 0x55 || ((const uint8_t*)(uintptr_t)mod_start)[511] != 0xAA) {
        serial_print("FAT32: missing boot signature\n");
        return -1;
    }

    g_volume.image = (uint8_t*)(uintptr_t)mod_start;
    g_volume.image_size = mod_end - mod_start;
    g_volume.bytes_per_sector = bs->bytes_per_sector;
    g_volume.sectors_per_cluster = bs->sectors_per_cluster;
    g_volume.reserved_sectors = bs->reserved_sector_count;
    g_volume.fat_count = bs->num_fats;
    g_volume.fat_size_sectors = bs->fat_size_32;
    g_volume.root_cluster = bs->root_cluster;
    g_volume.fat_start = g_volume.reserved_sectors * g_volume.bytes_per_sector;
    g_volume.data_start = (g_volume.reserved_sectors + g_volume.fat_count * g_volume.fat_size_sectors) * g_volume.bytes_per_sector;
    g_volume.mounted = 1;

    serial_print("FAT32: mounted image\n");
    return 0;
}

int fat32_is_ready(void) {
    return volume_ok();
}

int fat32_is_fat32_path(const char* path) {
    return path && fat32_has_prefix(path);
}

int fat32_lookup_path(const char* path, uint8_t* is_dir, uint32_t* first_cluster, uint32_t* size) {
    struct fat32_node node;
    const char* inner;

    if (!volume_ok() || !path || !is_dir || !first_cluster || !size) {
        return -1;
    }

    inner = skip_leading_slashes(path);
    if (!fat32_has_prefix(inner)) {
        return -1;
    }

    inner += 5;
    if (*inner == '/') {
        inner++;
    } else if (*inner != '\0') {
        return -1;
    }

    if (fat32_resolve_inner_path(inner, &node) != 0) {
        return -1;
    }

    *is_dir = node.is_dir;
    *first_cluster = node.first_cluster;
    *size = node.size;
    return 0;
}

int fat32_read_file(uint32_t first_cluster, uint32_t file_size, uint64_t offset, uint8_t* buffer, uint64_t len) {
    if (!volume_ok() || !buffer) {
        return -1;
    }
    return fat32_read_node(first_cluster, file_size, offset, buffer, len);
}

int fat32_create_path(const char* path, uint8_t* is_dir, uint32_t* first_cluster, uint32_t* size) {
    const char* inner = NULL;
    char parent_path[256];
    char leaf[64];
    uint8_t short_name[11];
    struct fat32_node parent;
    struct fat32_dirent_raw entry;
    uint32_t entry_cluster = 0;
    uint32_t entry_offset = 0;

    if (!volume_ok() || !path || !is_dir || !first_cluster || !size) return -1;
    if (fat32_inner_from_path(path, &inner) != 0 || inner[0] == '\0') return -1;
    split_parent_leaf(inner, parent_path, sizeof(parent_path), leaf, sizeof(leaf));
    if (make_short_name_83(leaf, short_name) != 0) return -1;
    if (fat32_resolve_inner_path(parent_path, &parent) != 0 || !parent.is_dir) return -1;
    if (fat32_entry_location_in_dir(parent.first_cluster, leaf, NULL, NULL, NULL) == 0) return -1;
    if (fat32_find_free_entry(parent.first_cluster, &entry_cluster, &entry_offset) != 0) return -1;

    local_memset(&entry, 0, sizeof(entry));
    local_memcpy(entry.name, short_name, sizeof(entry.name));
    entry.attr = 0x20;
    if (fat32_update_dirent(entry_cluster, entry_offset, &entry) != 0) return -1;

    *is_dir = 0;
    *first_cluster = 0;
    *size = 0;
    return 0;
}

int fat32_truncate_path(const char* path, uint32_t* first_cluster, uint32_t* size) {
    const char* inner = NULL;
    char parent_path[256];
    char leaf[64];
    struct fat32_node parent;
    struct fat32_dirent_raw entry;
    uint32_t entry_cluster = 0;
    uint32_t entry_offset = 0;

    if (!volume_ok() || !path || !first_cluster || !size) return -1;
    if (fat32_inner_from_path(path, &inner) != 0 || inner[0] == '\0') return -1;
    split_parent_leaf(inner, parent_path, sizeof(parent_path), leaf, sizeof(leaf));
    if (fat32_resolve_inner_path(parent_path, &parent) != 0 || !parent.is_dir) return -1;
    if (fat32_entry_location_in_dir(parent.first_cluster, leaf, &entry_cluster, &entry_offset, &entry) != 0) return -1;
    if (entry.attr & 0x10) return -1;
    entry.file_size = 0;
    if (fat32_update_dirent(entry_cluster, entry_offset, &entry) != 0) return -1;
    *first_cluster = first_cluster_from_entry(&entry);
    *size = 0;
    return 0;
}

int fat32_write_path(const char* path, uint64_t offset, const uint8_t* buffer, uint64_t len, uint64_t* written, uint32_t* new_size) {
    const char* inner = NULL;
    char parent_path[256];
    char leaf[64];
    struct fat32_node parent;
    struct fat32_dirent_raw entry;
    uint32_t entry_cluster = 0;
    uint32_t entry_offset = 0;
    uint32_t cluster_bytes = cluster_size_bytes();
    uint32_t first_cluster = 0;
    uint64_t remaining = len;
    const uint8_t* src = buffer;

    if (!volume_ok() || !path || !buffer || !written) return -1;
    *written = 0;
    if (fat32_inner_from_path(path, &inner) != 0 || inner[0] == '\0') return -1;
    split_parent_leaf(inner, parent_path, sizeof(parent_path), leaf, sizeof(leaf));
    if (fat32_resolve_inner_path(parent_path, &parent) != 0 || !parent.is_dir) return -1;
    if (fat32_entry_location_in_dir(parent.first_cluster, leaf, &entry_cluster, &entry_offset, &entry) != 0) return -1;
    if (entry.attr & 0x10) return -1;

    first_cluster = first_cluster_from_entry(&entry);
    while (remaining > 0) {
        uint32_t cluster_index = (uint32_t)((offset + *written) / cluster_bytes);
        uint32_t intra_cluster = (uint32_t)((offset + *written) % cluster_bytes);
        uint32_t cluster = 0;
        uint32_t chunk = cluster_bytes - intra_cluster;
        if (chunk > remaining) chunk = (uint32_t)remaining;
        if (fat32_get_cluster_for_offset(&first_cluster, cluster_index, 1, &cluster) != 0) return -1;
        if (fat32_write_cluster(cluster, intra_cluster, src, chunk) != 0) return -1;
        src += chunk;
        *written += chunk;
        remaining -= chunk;
    }

    entry.first_cluster_hi = (uint16_t)(first_cluster >> 16);
    entry.first_cluster_lo = (uint16_t)(first_cluster & 0xFFFF);
    if (offset + *written > entry.file_size) {
        entry.file_size = (uint32_t)(offset + *written);
    }
    if (fat32_update_dirent(entry_cluster, entry_offset, &entry) != 0) return -1;
    if (new_size) *new_size = entry.file_size;
    return 0;
}

int fat32_dirent_at_index(const char* path, uint64_t index, char* name_buf, size_t name_buf_size, uint32_t* size, uint8_t* d_type) {
    struct fat32_node node;
    uint8_t is_dir = 0;

    if (!volume_ok() || !path || !name_buf || name_buf_size == 0) {
        return -1;
    }

    if (fat32_lookup_path(path, &is_dir, &node.first_cluster, &node.size) != 0) {
        return -1;
    }
    node.is_dir = is_dir;
    if (!node.is_dir) {
        return -1;
    }

    return fat32_dirent_by_index(node.first_cluster, index, name_buf, name_buf_size, size, d_type);
}

int fat32_access_path(const char* path, uint64_t mode) {
    uint8_t is_dir = 0;
    uint32_t first_cluster = 0;
    uint32_t size = 0;

    if (!volume_ok() || !path) {
        return -1;
    }
    if (mode & ~((uint64_t)1 | (uint64_t)2 | (uint64_t)4)) {
        return -1;
    }
    if (fat32_lookup_path(path, &is_dir, &first_cluster, &size) != 0) {
        return -1;
    }
    (void)first_cluster;
    (void)size;
    return 0;
}
