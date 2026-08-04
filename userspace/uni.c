/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stddef.h>
#include <stdint.h>

#include "lib/uni_inflate.h"
#include "lib/uni_xz.h"

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_GETEUID 107
#define SYS_GETDENTS64 217
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258
#define SYS_UNLINKAT 263

#define AT_FDCWD -100
#define AT_REMOVEDIR 0x200
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_EXCL 128
#define O_TRUNC 512
#define O_DIRECTORY 65536
#define SEEK_SET 0
#define SEEK_END 2
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 32

#define UNI_ROOT "/main/Applications"
#define UNI_STATE_ROOT "/var/lib/uni"
#define UNI_MANIFEST_ROOT UNI_STATE_ROOT "/packages"
#define UNI_DB UNI_STATE_ROOT "/installed.db"
#define UNI_FORMAT_VERSION 1
#define UNI_PATH_CAP 128
#define UNI_ID_CAP 48
#define UNI_NAME_CAP 64
#define UNI_VERSION_CAP 32
#define UNI_ARCH_CAP 24
#define UNI_DESCRIPTION_CAP 160
#define UNI_DEPENDS_CAP 1024
#define UNI_PROVIDES_CAP 256
#define UNI_ENTRY_CAP 128
#define UNI_DB_CAP (128U * 1024U)
#define UNI_MANIFEST_CAP                                                    \
    (4096U + UNI_MAX_CREATED_PATHS * (UNI_PATH_CAP + 16U))
#define UNI_CONTROL_CAP 8192
#define UNI_MAX_ARCHIVE_SIZE (64U * 1024U * 1024U)
#define UNI_MAX_EXTRACTED_SIZE (128U * 1024U * 1024U)
#define UNI_MAX_CREATED_PATHS 1800
#define UNI_STREAM_BUFFER_SIZE (64U * 1024U)
#define UNI_TAR_METADATA_CAP 4096U
#define UNI_ELF_PROBE_CAP 4096U
#define UNI_MAX_PACKAGE_FILE_SIZE (1024ULL * 1024ULL * 1024ULL)
#define UNI_MAX_PACKAGE_BYTES (2ULL * 1024ULL * 1024ULL * 1024ULL)
#define UNI_XZ_DICTIONARY_LIMIT (256U * 1024U * 1024U)
#define UNI_XZ_WORKSPACE_SIZE (258U * 1024U * 1024U)
#define UNI_MAX_INSTALL_PACKAGES 24

#define ELF_PT_INTERP 3
#define ELF_MACHINE_X86_64 62

enum package_kind {
    PACKAGE_KIND_UNKNOWN,
    PACKAGE_KIND_AINSTALL,
    PACKAGE_KIND_DEB,
    PACKAGE_KIND_APPIMAGE,
};

enum compatibility_kind {
    COMPAT_DATA_ONLY,
    COMPAT_AOS_NATIVE,
    COMPAT_LINUX_STATIC,
    COMPAT_LINUX_DYNAMIC,
};

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

struct ar_member {
    uint64_t offset;
    uint64_t size;
    char name[32];
};

struct deb_archive {
    struct ar_member control;
    struct ar_member data;
    int has_debian_binary;
};

struct mapped_blob {
    uint8_t* data;
    uint64_t size;
    uint64_t mapping_size;
    uint8_t* compressed;
    uint64_t compressed_mapping_size;
};

struct package_info {
    enum package_kind kind;
    enum compatibility_kind compatibility;
    char id[UNI_ID_CAP];
    char name[UNI_NAME_CAP];
    char version[UNI_VERSION_CAP];
    char architecture[UNI_ARCH_CAP];
    char description[UNI_DESCRIPTION_CAP];
    char depends[UNI_DEPENDS_CAP];
    char pre_depends[UNI_DEPENDS_CAP];
    char provides[UNI_PROVIDES_CAP];
    char entry_hint[UNI_ENTRY_CAP];
    char entry[UNI_ENTRY_CAP];
    char compression[16];
    uint32_t file_count;
    uint32_t skipped_count;
    uint64_t installed_bytes;
    int system_install;
};

enum tar_stream_state {
    TAR_STREAM_HEADER,
    TAR_STREAM_DATA,
    TAR_STREAM_PADDING,
    TAR_STREAM_DONE,
};

struct tar_extract_stream {
    struct package_info* info;
    enum tar_stream_state state;
    uint8_t header[512];
    uint64_t header_used;
    uint64_t data_remaining;
    uint64_t padding_remaining;
    uint64_t entry_size;
    uint64_t entry_mode;
    char entry_type;
    char normalized[UNI_PATH_CAP];
    char full_path[UNI_PATH_CAP];
    char pending_name[UNI_PATH_CAP];
    uint8_t metadata[UNI_TAR_METADATA_CAP];
    uint64_t metadata_used;
    uint8_t elf_probe[UNI_ELF_PROBE_CAP];
    uint64_t elf_probe_used;
    int output_fd;
    int capture_metadata;
    int regular_file;
    int executable;
    int zero_blocks;
};

struct installed_record {
    char id[UNI_ID_CAP];
    char name[UNI_NAME_CAP];
    char version[UNI_VERSION_CAP];
    char kind[16];
    char compatibility[24];
    char entry[UNI_ENTRY_CAP];
    char description[UNI_DESCRIPTION_CAP];
    char provides[UNI_PROVIDES_CAP];
};

struct install_plan_item {
    const char* path;
    struct package_info info;
};

struct dependency_requirement {
    char name[UNI_ID_CAP];
    char relation[3];
    char version[UNI_VERSION_CAP];
};

struct elf64_ehdr {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

static char db_buffer[UNI_DB_CAP];
static char db_next_buffer[UNI_DB_CAP];
static char control_buffer[UNI_CONTROL_CAP];
static char created_paths[UNI_MAX_CREATED_PATHS][UNI_PATH_CAP];
static uint8_t created_is_dir[UNI_MAX_CREATED_PATHS];
static uint16_t created_count;
static char io_buffer[4096];
static char remove_buffers[16][4096];
static char number_buffer[32];
static uint8_t stream_input_buffer[UNI_STREAM_BUFFER_SIZE];
static uint8_t stream_output_buffer[UNI_STREAM_BUFFER_SIZE];
static struct tar_extract_stream tar_stream;
static struct install_plan_item install_plan[UNI_MAX_INSTALL_PACKAGES];

static long syscall1(long number, long a) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a)
                     : "rcx", "r11", "memory");
    return result;
}

static long syscall2(long number, long a, long b) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b)
                     : "rcx", "r11", "memory");
    return result;
}

static long syscall3(long number, long a, long b, long c) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return result;
}

static long syscall4(long number, long a, long b, long c, long d) {
    long result;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c), "r"(r10)
                     : "rcx", "r11", "memory");
    return result;
}

static long syscall6(long number, long a, long b, long c,
                     long d, long e, long f) {
    long result;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c),
                       "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return result;
}

static void exit_code(int code) {
    syscall1(SYS_EXIT, code);
    for (;;) {}
}

static uint64_t cstrlen(const char* value) {
    uint64_t length = 0;
    while (value && value[length]) length++;
    return length;
}

static int streq(const char* left, const char* right) {
    uint64_t i = 0;
    if (!left || !right) return 0;
    while (left[i] && right[i] && left[i] == right[i]) i++;
    return left[i] == 0 && right[i] == 0;
}

static int starts_with(const char* value, const char* prefix) {
    uint64_t i = 0;
    if (!value || !prefix) return 0;
    while (prefix[i]) {
        if (value[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static int ends_with(const char* value, const char* suffix) {
    uint64_t value_len = cstrlen(value);
    uint64_t suffix_len = cstrlen(suffix);
    if (suffix_len > value_len) return 0;
    return streq(value + value_len - suffix_len, suffix);
}

static void memory_zero(void* value, uint64_t size) {
    uint8_t* out = (uint8_t*)value;
    while (size--) *out++ = 0;
}

static void memory_copy(void* destination, const void* source, uint64_t size) {
    uint8_t* out = (uint8_t*)destination;
    const uint8_t* in = (const uint8_t*)source;
    while (size--) *out++ = *in++;
}

static int copy_bytes(char* destination, uint64_t capacity,
                      const char* source, uint64_t size) {
    if (!destination || !source || size + 1 > capacity) return -1;
    for (uint64_t i = 0; i < size; i++) destination[i] = source[i];
    destination[size] = 0;
    return 0;
}

static int copy_text(char* destination, uint64_t capacity,
                     const char* source) {
    return copy_bytes(destination, capacity, source, cstrlen(source));
}

static int append_text(char* destination, uint64_t capacity,
                       const char* source) {
    uint64_t length = cstrlen(destination);
    return copy_bytes(destination + length, capacity - length,
                      source, cstrlen(source));
}

static int append_char(char* destination, uint64_t capacity, char value) {
    uint64_t length = cstrlen(destination);
    if (length + 2 > capacity) return -1;
    destination[length] = value;
    destination[length + 1] = 0;
    return 0;
}

static int write_all_fd_checked(int fd, const void* data, uint64_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t done = 0;
    while (done < size) {
        long written = syscall3(SYS_WRITE, fd,
                                (long)(bytes + done), (long)(size - done));
        if (written <= 0) return -1;
        done += (uint64_t)written;
    }
    return 0;
}

static void write_all_fd(int fd, const void* data, uint64_t size) {
    (void)write_all_fd_checked(fd, data, size);
}

static void write_text(const char* text) {
    write_all_fd(1, text, cstrlen(text));
}

static const char* u64_text(uint64_t value) {
    char* at = number_buffer + sizeof(number_buffer) - 1;
    *at = 0;
    if (value == 0) {
        *--at = '0';
    } else {
        while (value) {
            *--at = (char)('0' + value % 10);
            value /= 10;
        }
    }
    return at;
}

static void print_error(const char* text) {
    write_text("uni: ");
    write_text(text);
    write_text("\n");
}

static void* map_memory(uint64_t size) {
    long address;
    if (size == 0 || size > UNI_MAX_EXTRACTED_SIZE) return NULL;
    address = syscall6(SYS_MMAP, 0, (long)size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return address < 0 ? NULL : (void*)(uintptr_t)address;
}

static void* map_large_workspace(uint64_t size) {
    long address;
    if (size == 0 || size > UNI_XZ_WORKSPACE_SIZE) return NULL;
    address = syscall6(SYS_MMAP, 0, (long)size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return address < 0 ? NULL : (void*)(uintptr_t)address;
}

static void unmap_memory(void* address, uint64_t size) {
    if (address && size) syscall2(SYS_MUNMAP, (long)address, (long)size);
}

static int read_exact(int fd, void* data, uint64_t size) {
    uint8_t* bytes = (uint8_t*)data;
    uint64_t done = 0;
    while (done < size) {
        long got = syscall3(SYS_READ, fd,
                            (long)(bytes + done), (long)(size - done));
        if (got <= 0) return -1;
        done += (uint64_t)got;
    }
    return 0;
}

static int read_at(int fd, uint64_t offset, void* data, uint64_t size) {
    if (offset > 0x7fffffffffffffffULL ||
        syscall3(SYS_LSEEK, fd, (long)offset, SEEK_SET) < 0) {
        return -1;
    }
    return read_exact(fd, data, size);
}

static int file_size(int fd, uint64_t* size) {
    long value = syscall3(SYS_LSEEK, fd, 0, SEEK_END);
    if (value < 0 || syscall3(SYS_LSEEK, fd, 0, SEEK_SET) < 0) return -1;
    *size = (uint64_t)value;
    return 0;
}

static int parse_decimal(const char* text, uint64_t length, uint64_t* value) {
    uint64_t result = 0;
    uint64_t digits = 0;
    for (uint64_t i = 0; i < length; i++) {
        char c = text[i];
        if (c == ' ' || c == 0) {
            if (digits) break;
            continue;
        }
        if (c < '0' || c > '9' || result > (UINT64_MAX - 9) / 10) return -1;
        result = result * 10 + (uint64_t)(c - '0');
        digits++;
    }
    if (!digits) return -1;
    *value = result;
    return 0;
}

static int parse_octal(const uint8_t* text, uint64_t length, uint64_t* value) {
    uint64_t result = 0;
    uint64_t digits = 0;
    for (uint64_t i = 0; i < length; i++) {
        uint8_t c = text[i];
        if (c == ' ' || c == 0) {
            if (digits) break;
            continue;
        }
        if (c < '0' || c > '7' || result > (UINT64_MAX - 7) / 8) return -1;
        result = result * 8 + (uint64_t)(c - '0');
        digits++;
    }
    *value = result;
    return digits ? 0 : -1;
}

static const char* path_basename(const char* path) {
    const char* result = path;
    for (uint64_t i = 0; path && path[i]; i++) {
        if (path[i] == '/') result = path + i + 1;
    }
    return result ? result : "";
}

static void id_from_text(char* out, uint64_t capacity, const char* text) {
    uint64_t at = 0;
    int previous_dash = 0;
    for (uint64_t i = 0; text && text[i] && at + 1 < capacity; i++) {
        char c = text[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + ('a' - 'A'));
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '+') {
            out[at++] = c;
            previous_dash = 0;
        } else if (!previous_dash && at > 0) {
            out[at++] = '-';
            previous_dash = 1;
        }
    }
    while (at && (out[at - 1] == '-' || out[at - 1] == '.')) at--;
    out[at] = 0;
    if (!out[0]) copy_text(out, capacity, "package");
}

static const char* kind_name(enum package_kind kind) {
    if (kind == PACKAGE_KIND_AINSTALL) return "ainstall";
    if (kind == PACKAGE_KIND_DEB) return "deb";
    if (kind == PACKAGE_KIND_APPIMAGE) return "appimage";
    return "unknown";
}

static const char* compatibility_name(enum compatibility_kind compatibility) {
    if (compatibility == COMPAT_AOS_NATIVE) return "aos-native";
    if (compatibility == COMPAT_LINUX_STATIC) return "linux-static";
    if (compatibility == COMPAT_LINUX_DYNAMIC) return "needs-linux-runtime";
    return "data-only";
}

static void free_blob(struct mapped_blob* blob) {
    if (!blob) return;
    if (blob->data && blob->data != blob->compressed) {
        unmap_memory(blob->data, blob->mapping_size);
    }
    if (blob->compressed) {
        unmap_memory(blob->compressed, blob->compressed_mapping_size);
    }
    memory_zero(blob, sizeof(*blob));
}

static int ar_member_name(char* out, uint64_t capacity,
                          const char raw[16]) {
    uint64_t length = 16;
    while (length && (raw[length - 1] == ' ' || raw[length - 1] == '/')) {
        length--;
    }
    return length ? copy_bytes(out, capacity, raw, length) : -1;
}

static int scan_deb_archive(int fd, uint64_t total_size,
                            struct deb_archive* archive) {
    uint8_t magic[8];
    uint64_t offset = 8;

    memory_zero(archive, sizeof(*archive));
    if (total_size < sizeof(magic) ||
        read_at(fd, 0, magic, sizeof(magic)) != 0 ||
        magic[0] != '!' || magic[1] != '<' || magic[2] != 'a' ||
        magic[3] != 'r' || magic[4] != 'c' || magic[5] != 'h' ||
        magic[6] != '>' || magic[7] != '\n') {
        print_error("not a Debian ar archive");
        return -1;
    }

    while (offset + 60 <= total_size) {
        char header[60];
        char name[32];
        uint64_t member_size;
        uint64_t data_offset;
        struct ar_member* target = NULL;

        if (read_at(fd, offset, header, sizeof(header)) != 0 ||
            header[58] != '`' || header[59] != '\n' ||
            ar_member_name(name, sizeof(name), header) != 0 ||
            parse_decimal(header + 48, 10, &member_size) != 0) {
            print_error("corrupt ar member header");
            return -1;
        }
        data_offset = offset + 60;
        if (member_size > total_size - data_offset) {
            print_error("truncated ar member");
            return -1;
        }

        if (streq(name, "debian-binary")) {
            char version[4];
            if (member_size < 4 || read_at(fd, data_offset, version, 4) != 0 ||
                version[0] != '2' || version[1] != '.' ||
                version[2] != '0' || version[3] != '\n') {
                print_error("unsupported debian-binary version");
                return -1;
            }
            archive->has_debian_binary = 1;
        } else if (starts_with(name, "control.tar")) {
            target = &archive->control;
        } else if (starts_with(name, "data.tar")) {
            target = &archive->data;
        }

        if (target) {
            target->offset = data_offset;
            target->size = member_size;
            copy_text(target->name, sizeof(target->name), name);
        }
        if (member_size > UINT64_MAX - data_offset - (member_size & 1U)) {
            return -1;
        }
        offset = data_offset + member_size + (member_size & 1U);
    }

    if (!archive->has_debian_binary || !archive->control.size ||
        !archive->data.size) {
        print_error("archive is missing debian-binary, control.tar, or data.tar");
        return -1;
    }
    return 0;
}

static int gzip_expected_size(const uint8_t* data, uint64_t size,
                              uint64_t* expected) {
    uint32_t value;
    if (!data || size < 18) return -1;
    value = (uint32_t)data[size - 4] |
            ((uint32_t)data[size - 3] << 8) |
            ((uint32_t)data[size - 2] << 16) |
            ((uint32_t)data[size - 1] << 24);
    if (value == 0 || value > UNI_MAX_EXTRACTED_SIZE) return -1;
    *expected = value;
    return 0;
}

static int load_archive_member(int fd, const struct ar_member* member,
                               struct mapped_blob* blob,
                               char* compression,
                               uint64_t compression_capacity) {
    uint64_t output_capacity = 0;
    size_t output_size = 0;

    memory_zero(blob, sizeof(*blob));
    if (!member || member->size == 0 ||
        member->size > UNI_MAX_ARCHIVE_SIZE) {
        print_error("archive member is too large");
        return -1;
    }

    blob->compressed = map_memory(member->size);
    if (!blob->compressed) {
        print_error("not enough memory for archive member");
        return -1;
    }
    blob->compressed_mapping_size = member->size;
    if (read_at(fd, member->offset, blob->compressed, member->size) != 0) {
        print_error("could not read archive member");
        free_blob(blob);
        return -1;
    }

    if (ends_with(member->name, ".tar")) {
        blob->data = blob->compressed;
        blob->size = member->size;
        blob->mapping_size = member->size;
        copy_text(compression, compression_capacity, "none");
        return 0;
    }
    if (ends_with(member->name, ".tar.gz")) {
        if (gzip_expected_size(blob->compressed, member->size,
                               &output_capacity) != 0) {
            print_error("invalid gzip size or package exceeds 128 MiB");
            free_blob(blob);
            return -1;
        }
        blob->data = map_memory(output_capacity);
        blob->mapping_size = output_capacity;
        if (!blob->data ||
            uni_inflate_gzip(blob->compressed, (size_t)member->size,
                             blob->data, (size_t)output_capacity,
                             &output_size) != 0) {
            print_error("gzip payload is corrupt");
            free_blob(blob);
            return -1;
        }
        blob->size = output_size;
        copy_text(compression, compression_capacity, "gzip");
        return 0;
    }
    if (ends_with(member->name, ".tar.xz")) {
        if (uni_xz_output_size(blob->compressed, (size_t)member->size,
                               &output_size) != 0 ||
            output_size == 0 || output_size > UNI_MAX_EXTRACTED_SIZE) {
            print_error("invalid XZ size or package exceeds 128 MiB");
            free_blob(blob);
            return -1;
        }
        output_capacity = output_size;
        blob->data = map_memory(output_capacity);
        blob->mapping_size = output_capacity;
        if (!blob->data ||
            uni_xz_decode(blob->compressed, (size_t)member->size,
                          blob->data, (size_t)output_capacity,
                          &output_size) != 0) {
            print_error("XZ payload is corrupt or uses unsupported filters");
            free_blob(blob);
            return -1;
        }
        blob->size = output_size;
        copy_text(compression, compression_capacity, "xz");
        return 0;
    } else if (ends_with(member->name, ".tar.zst")) {
        print_error("Zstandard-compressed deb payloads are not enabled yet");
    } else {
        print_error("unsupported Debian member compression");
    }
    free_blob(blob);
    return -1;
}

static int tar_header_is_zero(const uint8_t* header) {
    for (uint64_t i = 0; i < 512; i++) {
        if (header[i]) return 0;
    }
    return 1;
}

static int tar_checksum_valid(const uint8_t* header) {
    uint64_t expected;
    uint64_t sum = 0;
    if (parse_octal(header + 148, 8, &expected) != 0) return 0;
    for (uint64_t i = 0; i < 512; i++) {
        sum += (i >= 148 && i < 156) ? (uint8_t)' ' : header[i];
    }
    return sum == expected;
}

static int tar_header_name(const uint8_t* header,
                           char* out, uint64_t capacity) {
    uint64_t name_length = 0;
    uint64_t prefix_length = 0;
    while (name_length < 100 && header[name_length]) name_length++;
    while (prefix_length < 155 && header[345 + prefix_length]) prefix_length++;
    out[0] = 0;
    if (prefix_length) {
        if (copy_bytes(out, capacity, (const char*)header + 345,
                       prefix_length) != 0 ||
            append_char(out, capacity, '/') != 0) {
            return -1;
        }
    }
    if (name_length == 0 ||
        copy_bytes(out + cstrlen(out), capacity - cstrlen(out),
                   (const char*)header, name_length) != 0) {
        return -1;
    }
    return 0;
}

static int tar_next(const uint8_t* tar, uint64_t tar_size,
                    uint64_t* offset, const uint8_t** header,
                    const uint8_t** data, uint64_t* data_size,
                    char* name, uint64_t name_capacity) {
    uint64_t rounded;

    if (*offset + 512 > tar_size) return 0;
    *header = tar + *offset;
    if (tar_header_is_zero(*header)) return 0;
    if (!tar_checksum_valid(*header) ||
        tar_header_name(*header, name, name_capacity) != 0 ||
        parse_octal(*header + 124, 12, data_size) != 0) {
        return -1;
    }
    if (*data_size > UINT64_MAX - 511) return -1;
    rounded = (*data_size + 511) & ~511ULL;
    if (*offset + 512 > tar_size ||
        rounded > tar_size - (*offset + 512)) {
        return -1;
    }
    *data = *header + 512;
    *offset += 512 + rounded;
    return 1;
}

static int control_field(const char* text, const char* field,
                         char* out, uint64_t capacity) {
    uint64_t field_length = cstrlen(field);
    uint64_t at = 0;
    while (text[at]) {
        uint64_t line_start = at;
        uint64_t line_end;
        while (text[at] && text[at] != '\n' && text[at] != '\r') at++;
        line_end = at;
        while (text[at] == '\n' || text[at] == '\r') at++;
        if (line_end <= line_start + field_length + 1) continue;
        if (!starts_with(text + line_start, field) ||
            text[line_start + field_length] != ':') continue;

        line_start += field_length + 1;
        while (line_start < line_end &&
               (text[line_start] == ' ' || text[line_start] == '\t')) {
            line_start++;
        }
        while (line_end > line_start &&
               (text[line_end - 1] == ' ' || text[line_end - 1] == '\t')) {
            line_end--;
        }
        return copy_bytes(out, capacity, text + line_start,
                          line_end - line_start);
    }
    return -1;
}

static int read_deb_control(const struct mapped_blob* control,
                            struct package_info* info) {
    uint64_t offset = 0;
    const uint8_t* header;
    const uint8_t* data;
    uint64_t data_size;
    char name[UNI_PATH_CAP];

    while (1) {
        int result = tar_next(control->data, control->size, &offset,
                              &header, &data, &data_size,
                              name, sizeof(name));
        (void)header;
        if (result <= 0) break;
        while (starts_with(name, "./")) {
            for (uint64_t i = 0;; i++) {
                name[i] = name[i + 2];
                if (!name[i]) break;
            }
        }
        if (streq(name, "control")) {
            if (data_size == 0 || data_size >= sizeof(control_buffer)) {
                print_error("Debian control file is too large");
                return -1;
            }
            memory_copy(control_buffer, data, data_size);
            control_buffer[data_size] = 0;
            if (control_field(control_buffer, "Package",
                              info->id, sizeof(info->id)) != 0) {
                print_error("Debian control file has no Package field");
                return -1;
            }
            id_from_text(info->id, sizeof(info->id), info->id);
            if (control_field(control_buffer, "Name",
                              info->name, sizeof(info->name)) != 0) {
                copy_text(info->name, sizeof(info->name), info->id);
            }
            if (control_field(control_buffer, "Version",
                              info->version, sizeof(info->version)) != 0) {
                copy_text(info->version, sizeof(info->version), "unknown");
            }
            if (control_field(control_buffer, "Architecture",
                              info->architecture,
                              sizeof(info->architecture)) != 0) {
                copy_text(info->architecture,
                          sizeof(info->architecture), "all");
            }
            control_field(control_buffer, "Description",
                          info->description, sizeof(info->description));
            control_field(control_buffer, "Depends",
                          info->depends, sizeof(info->depends));
            control_field(control_buffer, "Pre-Depends",
                          info->pre_depends, sizeof(info->pre_depends));
            control_field(control_buffer, "Provides",
                          info->provides, sizeof(info->provides));
            control_field(control_buffer, "X-AOS-Entry",
                          info->entry_hint, sizeof(info->entry_hint));
            return 0;
        }
    }
    print_error("control.tar does not contain a control file");
    return -1;
}

static int path_exists(const char* path, int directory) {
    int flags = O_RDONLY | (directory ? O_DIRECTORY : 0);
    long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, flags, 0);
    if (fd < 0) return 0;
    syscall1(SYS_CLOSE, fd);
    return 1;
}

static int remember_created(const char* path, int is_dir) {
    if (created_count >= UNI_MAX_CREATED_PATHS ||
        copy_text(created_paths[created_count], UNI_PATH_CAP, path) != 0) {
        return -1;
    }
    created_is_dir[created_count] = is_dir ? 1 : 0;
    created_count++;
    return 0;
}

static int ensure_directory(const char* path) {
    long result;
    if (path_exists(path, 1)) return 0;
    result = syscall3(SYS_MKDIRAT, AT_FDCWD, (long)path, 0755);
    if (result < 0 || !path_exists(path, 1)) return -1;
    if (remember_created(path, 1) != 0) {
        syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, AT_REMOVEDIR);
        return -1;
    }
    return 0;
}

static int ensure_directory_tree(const char* path, int include_leaf) {
    char work[UNI_PATH_CAP];
    uint64_t length;

    if (copy_text(work, sizeof(work), path) != 0) return -1;
    length = cstrlen(work);
    for (uint64_t i = 1; i < length; i++) {
        if (work[i] != '/') continue;
        work[i] = 0;
        if (ensure_directory(work) != 0) return -1;
        work[i] = '/';
    }
    return include_leaf ? ensure_directory(work) : 0;
}

static void cleanup_created(void) {
    while (created_count) {
        uint16_t index = --created_count;
        syscall3(SYS_UNLINKAT, AT_FDCWD,
                 (long)created_paths[index],
                 created_is_dir[index] ? AT_REMOVEDIR : 0);
    }
}

static int normalize_archive_path(const char* input,
                                  char* output, uint64_t capacity) {
    uint64_t in = 0;
    uint64_t out = 0;

    if (!input || !output || capacity == 0) return -1;
    while (input[in] == '.' && input[in + 1] == '/') in += 2;
    if (!input[in] || input[in] == '/' || input[in] == '\\') return -1;

    while (input[in]) {
        uint64_t component_start = in;
        uint64_t component_size = 0;
        while (input[in] && input[in] != '/') {
            if (input[in] == '\\' || input[in] == '\n' ||
                input[in] == '\r' || input[in] == '\t') {
                return -1;
            }
            in++;
            component_size++;
        }
        if ((component_size == 1 && input[component_start] == '.') ||
            component_size == 0) {
            if (input[in] == '/') in++;
            continue;
        }
        if (component_size == 2 && input[component_start] == '.' &&
            input[component_start + 1] == '.') {
            return -1;
        }
        if (out && out + 1 < capacity) output[out++] = '/';
        if (component_size >= capacity - out) return -1;
        for (uint64_t i = 0; i < component_size; i++) {
            output[out++] = input[component_start + i];
        }
        if (input[in] == '/') in++;
    }
    if (!out) return -1;
    output[out] = 0;
    return 0;
}

static int join_install_path(char* out, uint64_t capacity,
                             const char* package_id,
                             const char* archive_path) {
    out[0] = 0;
    if (append_text(out, capacity, UNI_ROOT "/") != 0 ||
        append_text(out, capacity, package_id) != 0) return -1;
    if (archive_path && archive_path[0]) {
        if (append_char(out, capacity, '/') != 0 ||
            append_text(out, capacity, archive_path) != 0) return -1;
    }
    return 0;
}

static int package_destination_path(char* out, uint64_t capacity,
                                    const struct package_info* info,
                                    const char* archive_path) {
    if (info->system_install) {
        out[0] = 0;
        if (append_char(out, capacity, '/') != 0 ||
            append_text(out, capacity, archive_path) != 0) {
            return -1;
        }
        return 0;
    }
    return join_install_path(out, capacity, info->id, archive_path);
}

static int write_package_file(const char* path,
                              const uint8_t* data, uint64_t size,
                              uint64_t mode) {
    int fd;
    uint64_t done = 0;

    if (path_exists(path, 0) || path_exists(path, 1) ||
        ensure_directory_tree(path, 0) != 0) {
        return -1;
    }
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path,
                       O_WRONLY | O_CREAT | O_EXCL, (long)(mode & 07777U));
    if (fd < 0) return -1;
    while (done < size) {
        uint64_t chunk = size - done;
        long written;
        if (chunk > sizeof(io_buffer)) chunk = sizeof(io_buffer);
        written = syscall3(SYS_WRITE, fd,
                           (long)(data + done), (long)chunk);
        if (written <= 0) {
            syscall1(SYS_CLOSE, fd);
            syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0);
            return -1;
        }
        done += (uint64_t)written;
    }
    syscall1(SYS_CLOSE, fd);
    if (remember_created(path, 0) != 0) {
        syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0);
        return -1;
    }
    return 0;
}

static int interpreter_matches(const uint8_t* data, uint64_t size,
                               const struct elf64_phdr* phdr,
                               const char* expected) {
    uint64_t expected_size = cstrlen(expected) + 1;
    if (phdr->offset > size || phdr->filesz > size - phdr->offset ||
        phdr->filesz != expected_size) return 0;
    for (uint64_t i = 0; i < expected_size; i++) {
        if (data[phdr->offset + i] != (uint8_t)expected[i]) return 0;
    }
    return 1;
}

static int classify_elf(const uint8_t* data, uint64_t size,
                        enum compatibility_kind* compatibility) {
    const struct elf64_ehdr* header;
    const struct elf64_phdr* programs;
    int has_interpreter = 0;

    if (!data || size < sizeof(struct elf64_ehdr)) return -1;
    header = (const struct elf64_ehdr*)data;
    if (header->ident[0] != 0x7f || header->ident[1] != 'E' ||
        header->ident[2] != 'L' || header->ident[3] != 'F' ||
        header->ident[4] != 2 || header->ident[5] != 1 ||
        header->machine != ELF_MACHINE_X86_64 ||
        header->phentsize != sizeof(struct elf64_phdr) ||
        header->phoff > size ||
        (uint64_t)header->phnum >
            (size - header->phoff) / sizeof(struct elf64_phdr)) {
        return -1;
    }

    programs = (const struct elf64_phdr*)(data + header->phoff);
    for (uint16_t i = 0; i < header->phnum; i++) {
        if (programs[i].type != ELF_PT_INTERP) continue;
        has_interpreter = 1;
        if (interpreter_matches(data, size, &programs[i],
                                "/lib/ld64.so.1")) {
            *compatibility = COMPAT_AOS_NATIVE;
            return 0;
        }
    }
    *compatibility = has_interpreter
                         ? COMPAT_LINUX_DYNAMIC
                         : COMPAT_LINUX_STATIC;
    return 0;
}

static int compatibility_rank(enum compatibility_kind compatibility) {
    if (compatibility == COMPAT_AOS_NATIVE) return 3;
    if (compatibility == COMPAT_LINUX_STATIC) return 2;
    if (compatibility == COMPAT_LINUX_DYNAMIC) return 1;
    return 0;
}

static int archive_path_matches_hint(const char* path, const char* hint) {
    while (starts_with(hint, "./")) hint += 2;
    while (*hint == '/') hint++;
    return hint[0] && streq(path, hint);
}

static int pax_path(const uint8_t* data, uint64_t size,
                    char* out, uint64_t capacity) {
    uint64_t at = 0;
    while (at < size) {
        uint64_t record_length = 0;
        uint64_t digits = 0;
        uint64_t record_start = at;
        uint64_t value_start;
        while (at < size && data[at] >= '0' && data[at] <= '9') {
            record_length = record_length * 10 + (uint64_t)(data[at] - '0');
            at++;
            digits++;
        }
        if (!digits || at >= size || data[at] != ' ' ||
            record_length == 0 || record_length > size - record_start) {
            return -1;
        }
        at++;
        value_start = at;
        if (record_length >= digits + 6 &&
            starts_with((const char*)data + value_start, "path=")) {
            uint64_t value_size = record_start + record_length -
                                  (value_start + 5);
            while (value_size &&
                   (data[value_start + 5 + value_size - 1] == '\n' ||
                    data[value_start + 5 + value_size - 1] == 0)) {
                value_size--;
            }
            return copy_bytes(out, capacity,
                              (const char*)data + value_start + 5,
                              value_size);
        }
        at = record_start + record_length;
    }
    return -1;
}

static int extract_data_tar(const struct mapped_blob* payload,
                            struct package_info* info) {
    uint64_t offset = 0;
    const uint8_t* header;
    const uint8_t* data;
    uint64_t data_size;
    char header_name[UNI_PATH_CAP];
    char pending_name[UNI_PATH_CAP];
    char normalized[UNI_PATH_CAP];
    char full_path[UNI_PATH_CAP];

    pending_name[0] = 0;
    while (1) {
        uint64_t mode = 0;
        char type;
        int result = tar_next(payload->data, payload->size, &offset,
                              &header, &data, &data_size,
                              header_name, sizeof(header_name));
        if (result == 0) break;
        if (result < 0) {
            print_error("corrupt tar payload");
            return -1;
        }
        type = (char)header[156];

        if (type == 'x') {
            if (pax_path(data, data_size, pending_name,
                         sizeof(pending_name)) != 0) {
                pending_name[0] = 0;
            }
            continue;
        }
        if (type == 'L') {
            uint64_t name_size = data_size;
            while (name_size && (data[name_size - 1] == 0 ||
                                 data[name_size - 1] == '\n')) {
                name_size--;
            }
            if (copy_bytes(pending_name, sizeof(pending_name),
                           (const char*)data, name_size) != 0) {
                print_error("tar long path exceeds AOS path limit");
                return -1;
            }
            continue;
        }
        if (pending_name[0]) {
            copy_text(header_name, sizeof(header_name), pending_name);
            pending_name[0] = 0;
        }

        /* Tar commonly emits a synthetic "./" entry for the archive root. */
        if (streq(header_name, ".") || streq(header_name, "./")) {
            continue;
        }

        if (normalize_archive_path(header_name, normalized,
                                   sizeof(normalized)) != 0 ||
            package_destination_path(full_path, sizeof(full_path),
                                     info, normalized) != 0) {
            print_error("unsafe or overlong path in package");
            return -1;
        }

        if (type == '5') {
            if (ensure_directory_tree(full_path, 1) != 0) {
                print_error("could not create package directory");
                return -1;
            }
            continue;
        }
        if (type != 0 && type != '0' && type != '7') {
            info->skipped_count++;
            continue;
        }
        if (data_size > UNI_MAX_EXTRACTED_SIZE ||
            parse_octal(header + 100, 8, &mode) != 0) {
            print_error("invalid file entry in package");
            return -1;
        }
        if (write_package_file(full_path, data, data_size, mode) != 0) {
            print_error("could not write package file");
            return -1;
        }
        info->file_count++;
        info->installed_bytes += data_size;

        if ((mode & 0111U) != 0 || archive_path_matches_hint(
                normalized, info->entry_hint)) {
            enum compatibility_kind candidate;
            if (classify_elf(data, data_size, &candidate) == 0 &&
                (archive_path_matches_hint(normalized, info->entry_hint) ||
                 compatibility_rank(candidate) >
                     compatibility_rank(info->compatibility))) {
                info->compatibility = candidate;
                copy_text(info->entry, sizeof(info->entry), full_path);
            }
        }
    }
    return 0;
}

static void tar_extract_abort(struct tar_extract_stream* stream) {
    if (stream->output_fd >= 0) {
        syscall1(SYS_CLOSE, stream->output_fd);
        stream->output_fd = -1;
    }
}

static int tar_extract_write_file(struct tar_extract_stream* stream,
                                  const uint8_t* data, uint64_t size) {
    uint64_t done = 0;

    if (stream->elf_probe_used < sizeof(stream->elf_probe)) {
        uint64_t probe_size = sizeof(stream->elf_probe) -
                              stream->elf_probe_used;
        if (probe_size > size) probe_size = size;
        memory_copy(stream->elf_probe + stream->elf_probe_used,
                    data, probe_size);
        stream->elf_probe_used += probe_size;
    }
    while (done < size) {
        long written = syscall3(SYS_WRITE, stream->output_fd,
                                (long)(data + done),
                                (long)(size - done));
        if (written <= 0) return -1;
        done += (uint64_t)written;
    }
    return 0;
}

static int tar_extract_finish_entry(struct tar_extract_stream* stream) {
    if (stream->output_fd >= 0) {
        syscall1(SYS_CLOSE, stream->output_fd);
        stream->output_fd = -1;
    }

    if (stream->capture_metadata) {
        if (stream->entry_type == 'L') {
            uint64_t size = stream->metadata_used;
            while (size && (stream->metadata[size - 1] == 0 ||
                            stream->metadata[size - 1] == '\n')) {
                size--;
            }
            if (copy_bytes(stream->pending_name,
                           sizeof(stream->pending_name),
                           (const char*)stream->metadata, size) != 0) {
                return -1;
            }
        } else if (pax_path(stream->metadata, stream->metadata_used,
                            stream->pending_name,
                            sizeof(stream->pending_name)) != 0) {
            stream->pending_name[0] = 0;
        }
    } else if (stream->regular_file) {
        enum compatibility_kind candidate;

        stream->info->file_count++;
        stream->info->installed_bytes += stream->entry_size;
        if (stream->executable &&
            classify_elf(stream->elf_probe, stream->elf_probe_used,
                         &candidate) == 0 &&
            (archive_path_matches_hint(stream->normalized,
                                       stream->info->entry_hint) ||
             compatibility_rank(candidate) >
                 compatibility_rank(stream->info->compatibility))) {
            stream->info->compatibility = candidate;
            copy_text(stream->info->entry, sizeof(stream->info->entry),
                      stream->full_path);
        }
    }

    stream->capture_metadata = 0;
    stream->regular_file = 0;
    stream->executable = 0;
    stream->metadata_used = 0;
    stream->elf_probe_used = 0;
    if (stream->padding_remaining) {
        stream->state = TAR_STREAM_PADDING;
    } else {
        stream->state = TAR_STREAM_HEADER;
        stream->header_used = 0;
    }
    return 0;
}

static int tar_extract_begin_entry(struct tar_extract_stream* stream) {
    char header_name[UNI_PATH_CAP];
    uint64_t mode;
    uint64_t size;
    char type;

    if (tar_header_is_zero(stream->header)) {
        stream->zero_blocks++;
        stream->header_used = 0;
        if (stream->zero_blocks >= 2) {
            stream->state = TAR_STREAM_DONE;
        }
        return 0;
    }
    if (stream->zero_blocks != 0 ||
        !tar_checksum_valid(stream->header) ||
        tar_header_name(stream->header, header_name,
                        sizeof(header_name)) != 0 ||
        parse_octal(stream->header + 100, 8, &mode) != 0 ||
        parse_octal(stream->header + 124, 12, &size) != 0) {
        return -1;
    }

    type = (char)stream->header[156];
    stream->entry_type = type;
    stream->entry_mode = mode;
    stream->entry_size = size;
    stream->data_remaining = size;
    stream->padding_remaining = (512ULL - (size & 511ULL)) & 511ULL;
    stream->metadata_used = 0;
    stream->elf_probe_used = 0;
    stream->capture_metadata = type == 'x' || type == 'L';
    stream->regular_file = 0;
    stream->executable = 0;
    stream->normalized[0] = 0;
    stream->full_path[0] = 0;

    if (stream->capture_metadata) {
        if (size > sizeof(stream->metadata)) return -1;
    } else {
        if (stream->pending_name[0]) {
            if (copy_text(header_name, sizeof(header_name),
                          stream->pending_name) != 0) {
                return -1;
            }
            stream->pending_name[0] = 0;
        }

        if (!streq(header_name, ".") && !streq(header_name, "./")) {
            if (normalize_archive_path(header_name, stream->normalized,
                                       sizeof(stream->normalized)) != 0 ||
                package_destination_path(stream->full_path,
                                         sizeof(stream->full_path),
                                         stream->info,
                                         stream->normalized) != 0) {
                return -1;
            }

            if (type == '5') {
                if (ensure_directory_tree(stream->full_path, 1) != 0) {
                    return -1;
                }
            } else if (type == 0 || type == '0' || type == '7') {
                if (size > UNI_MAX_PACKAGE_FILE_SIZE ||
                    stream->info->installed_bytes >
                        UNI_MAX_PACKAGE_BYTES - size ||
                    created_count >= UNI_MAX_CREATED_PATHS ||
                    path_exists(stream->full_path, 0) ||
                    path_exists(stream->full_path, 1) ||
                    ensure_directory_tree(stream->full_path, 0) != 0) {
                    return -1;
                }
                stream->output_fd = (int)syscall4(
                    SYS_OPENAT, AT_FDCWD, (long)stream->full_path,
                    O_WRONLY | O_CREAT | O_EXCL, (long)(mode & 07777U));
                if (stream->output_fd < 0) return -1;
                if (remember_created(stream->full_path, 0) != 0) {
                    syscall1(SYS_CLOSE, stream->output_fd);
                    stream->output_fd = -1;
                    syscall3(SYS_UNLINKAT, AT_FDCWD,
                             (long)stream->full_path, 0);
                    return -1;
                }
                stream->regular_file = 1;
                stream->executable =
                    (mode & 0111U) != 0 ||
                    archive_path_matches_hint(stream->normalized,
                                              stream->info->entry_hint);
            } else {
                stream->info->skipped_count++;
            }
        }
    }

    stream->state = TAR_STREAM_DATA;
    if (stream->data_remaining == 0) {
        return tar_extract_finish_entry(stream);
    }
    return 0;
}

static int tar_extract_feed(struct tar_extract_stream* stream,
                            const uint8_t* data, uint64_t size) {
    uint64_t at = 0;

    while (at < size) {
        if (stream->state == TAR_STREAM_DONE) {
            while (at < size) {
                if (data[at++] != 0) return -1;
            }
            continue;
        }
        if (stream->state == TAR_STREAM_HEADER) {
            uint64_t take = sizeof(stream->header) - stream->header_used;
            if (take > size - at) take = size - at;
            memory_copy(stream->header + stream->header_used,
                        data + at, take);
            stream->header_used += take;
            at += take;
            if (stream->header_used == sizeof(stream->header) &&
                tar_extract_begin_entry(stream) != 0) {
                return -1;
            }
            continue;
        }
        if (stream->state == TAR_STREAM_DATA) {
            uint64_t take = stream->data_remaining;
            if (take > size - at) take = size - at;
            if (stream->capture_metadata) {
                memory_copy(stream->metadata + stream->metadata_used,
                            data + at, take);
                stream->metadata_used += take;
            } else if (stream->regular_file &&
                       tar_extract_write_file(stream, data + at, take) != 0) {
                return -1;
            }
            stream->data_remaining -= take;
            at += take;
            if (stream->data_remaining == 0 &&
                tar_extract_finish_entry(stream) != 0) {
                return -1;
            }
            continue;
        }
        if (stream->state == TAR_STREAM_PADDING) {
            uint64_t take = stream->padding_remaining;
            if (take > size - at) take = size - at;
            at += take;
            stream->padding_remaining -= take;
            if (stream->padding_remaining == 0) {
                stream->state = TAR_STREAM_HEADER;
                stream->header_used = 0;
            }
            continue;
        }
        return -1;
    }
    return 0;
}

static int tar_extract_complete(struct tar_extract_stream* stream) {
    if (stream->output_fd >= 0 ||
        stream->state == TAR_STREAM_DATA ||
        stream->state == TAR_STREAM_PADDING ||
        stream->header_used != 0 ||
        stream->zero_blocks == 0) {
        tar_extract_abort(stream);
        return -1;
    }
    return 0;
}

static void tar_extract_init(struct tar_extract_stream* stream,
                             struct package_info* info) {
    memory_zero(stream, sizeof(*stream));
    stream->info = info;
    stream->state = TAR_STREAM_HEADER;
    stream->output_fd = -1;
}

static int extract_plain_member_stream(int fd,
                                       const struct ar_member* member,
                                       struct package_info* info) {
    uint64_t remaining = member->size;

    if (syscall3(SYS_LSEEK, fd, (long)member->offset, SEEK_SET) < 0) {
        return -1;
    }
    tar_extract_init(&tar_stream, info);
    while (remaining) {
        uint64_t chunk = remaining;
        if (chunk > sizeof(stream_input_buffer)) {
            chunk = sizeof(stream_input_buffer);
        }
        if (read_exact(fd, stream_input_buffer, chunk) != 0 ||
            tar_extract_feed(&tar_stream,
                             stream_input_buffer, chunk) != 0) {
            tar_extract_abort(&tar_stream);
            return -1;
        }
        remaining -= chunk;
    }
    return tar_extract_complete(&tar_stream);
}

static int extract_xz_member_stream(int fd,
                                    const struct ar_member* member,
                                    struct package_info* info) {
    struct uni_xz_stream decoder;
    uint64_t compressed_remaining;
    uint64_t input_size;
    uint64_t workspace_size;
    uint32_t dictionary_size;
    void* workspace = NULL;
    int result = -1;

    if (member->size < 24 ||
        syscall3(SYS_LSEEK, fd, (long)member->offset, SEEK_SET) < 0) {
        return -1;
    }
    input_size = member->size;
    if (input_size > sizeof(stream_input_buffer)) {
        input_size = sizeof(stream_input_buffer);
    }
    if (read_exact(fd, stream_input_buffer, input_size) != 0 ||
        uni_xz_dictionary_size(stream_input_buffer, input_size,
                               &dictionary_size) != 0 ||
        dictionary_size > UNI_XZ_DICTIONARY_LIMIT) {
        print_error("invalid XZ header or dictionary exceeds 256 MiB");
        return -1;
    }
    compressed_remaining = member->size - input_size;
    workspace_size = (uint64_t)dictionary_size + 2U * 1024U * 1024U;
    workspace_size = (workspace_size + 4095U) & ~4095ULL;
    workspace = map_large_workspace(workspace_size);
    if (!workspace) {
        print_error("not enough temporary memory for the XZ dictionary");
        return -1;
    }
    if (uni_xz_stream_init(&decoder, workspace,
                           workspace_size, dictionary_size) != 0) {
        print_error("could not initialize the XZ stream decoder");
        unmap_memory(workspace, workspace_size);
        return -1;
    }

    tar_extract_init(&tar_stream, info);
    while (!decoder.finished) {
        uint64_t input_at = 0;

        while (input_at < input_size && !decoder.finished) {
            size_t input_used = 0;
            size_t output_used = 0;

            if (uni_xz_stream_decode(
                    &decoder, stream_input_buffer + input_at,
                    (size_t)(input_size - input_at), &input_used,
                    stream_output_buffer, sizeof(stream_output_buffer),
                    &output_used) != 0 ||
                (input_used == 0 && output_used == 0) ||
                (output_used &&
                 tar_extract_feed(&tar_stream, stream_output_buffer,
                                  output_used) != 0)) {
                goto done;
            }
            input_at += input_used;
        }
        if (input_at != input_size) goto done;
        if (decoder.finished) break;
        if (compressed_remaining == 0) goto done;

        input_size = compressed_remaining;
        if (input_size > sizeof(stream_input_buffer)) {
            input_size = sizeof(stream_input_buffer);
        }
        if (read_exact(fd, stream_input_buffer, input_size) != 0) goto done;
        compressed_remaining -= input_size;
    }

    if (!decoder.finished || compressed_remaining != 0 ||
        tar_extract_complete(&tar_stream) != 0) {
        goto done;
    }
    result = 0;

done:
    if (result != 0) tar_extract_abort(&tar_stream);
    uni_xz_stream_end(&decoder);
    unmap_memory(workspace, workspace_size);
    return result;
}

static int extract_archive_member_stream(int fd,
                                         const struct ar_member* member,
                                         struct package_info* info) {
    if (ends_with(member->name, ".tar")) {
        copy_text(info->compression, sizeof(info->compression), "none");
        return extract_plain_member_stream(fd, member, info);
    }
    if (ends_with(member->name, ".tar.xz")) {
        copy_text(info->compression, sizeof(info->compression), "xz");
        return extract_xz_member_stream(fd, member, info);
    }
    if (ends_with(member->name, ".tar.gz")) {
        struct mapped_blob payload;
        int result;

        if (load_archive_member(fd, member, &payload,
                                info->compression,
                                sizeof(info->compression)) != 0) {
            return -1;
        }
        result = extract_data_tar(&payload, info);
        free_blob(&payload);
        return result;
    } else if (ends_with(member->name, ".tar.zst")) {
        print_error("Zstandard-compressed deb payloads are not enabled yet");
    } else {
        print_error("unsupported Debian member compression");
    }
    return -1;
}

static void sanitize_record_text(char* text) {
    for (uint64_t i = 0; text && text[i]; i++) {
        if (text[i] == '|' || text[i] == '\n' || text[i] == '\r' ||
            text[i] == '\t') {
            text[i] = ' ';
        }
    }
}

static int load_database(void) {
    int fd = (int)syscall4(SYS_OPENAT, AT_FDCWD,
                           (long)UNI_DB, O_RDONLY, 0);
    uint64_t total = 0;

    db_buffer[0] = 0;
    if (fd < 0) return 0;
    while (total + 1 < sizeof(db_buffer)) {
        long got = syscall3(SYS_READ, fd,
                            (long)(db_buffer + total),
                            (long)(sizeof(db_buffer) - total - 1));
        if (got < 0) {
            syscall1(SYS_CLOSE, fd);
            return -1;
        }
        if (got == 0) break;
        total += (uint64_t)got;
    }
    syscall1(SYS_CLOSE, fd);
    db_buffer[total] = 0;
    return 0;
}

static int write_database(const char* data, uint64_t size) {
    int fd;
    uint64_t done = 0;
    if (ensure_directory_tree(UNI_STATE_ROOT, 1) != 0) return -1;
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)UNI_DB,
                       O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    while (done < size) {
        long written = syscall3(SYS_WRITE, fd,
                                (long)(data + done),
                                (long)(size - done));
        if (written <= 0) {
            syscall1(SYS_CLOSE, fd);
            return -1;
        }
        done += (uint64_t)written;
    }
    syscall1(SYS_CLOSE, fd);
    return 0;
}

static int field_copy(const char* start, const char* end,
                      char* out, uint64_t capacity) {
    const char* at = start;
    while (at < end && *at != '|') at++;
    if (copy_bytes(out, capacity, start, (uint64_t)(at - start)) != 0) {
        return -1;
    }
    return at < end ? (int)(at - start + 1) : (int)(at - start);
}

static int parse_installed_line(const char* line, const char* end,
                                struct installed_record* record) {
    const char* at = line;
    int used;
    char ignored[32];

#define NEXT_FIELD(target)                                                     \
    do {                                                                       \
        used = field_copy(at, end, (target), sizeof(target));                  \
        if (used < 0) return -1;                                               \
        at += used;                                                            \
    } while (0)

    memory_zero(record, sizeof(*record));
    NEXT_FIELD(record->id);
    NEXT_FIELD(record->name);
    NEXT_FIELD(record->version);
    NEXT_FIELD(record->kind);
    NEXT_FIELD(record->compatibility);
    NEXT_FIELD(record->entry);
    NEXT_FIELD(record->description);
    NEXT_FIELD(ignored);
    NEXT_FIELD(ignored);
    if (at < end) NEXT_FIELD(record->provides);
#undef NEXT_FIELD
    return record->id[0] ? 0 : -1;
}

static int find_installed_record(const char* id,
                                 struct installed_record* record) {
    uint64_t at = 0;
    if (load_database() != 0) return -1;
    while (db_buffer[at]) {
        uint64_t start = at;
        struct installed_record candidate;
        while (db_buffer[at] && db_buffer[at] != '\n' &&
               db_buffer[at] != '\r') at++;
        if (parse_installed_line(db_buffer + start,
                                 db_buffer + at, &candidate) == 0 &&
            streq(candidate.id, id)) {
            if (record) *record = candidate;
            return 0;
        }
        while (db_buffer[at] == '\n' || db_buffer[at] == '\r') at++;
    }
    return -1;
}

static int append_buffer(char* buffer, uint64_t capacity,
                         uint64_t* length, const char* text) {
    uint64_t size = cstrlen(text);
    if (size > capacity - *length - 1) return -1;
    memory_copy(buffer + *length, text, size);
    *length += size;
    buffer[*length] = 0;
    return 0;
}

static int database_without(const char* id, uint64_t* output_length) {
    uint64_t at = 0;
    uint64_t out = 0;

    db_next_buffer[0] = 0;
    if (load_database() != 0) return -1;
    while (db_buffer[at]) {
        uint64_t start = at;
        struct installed_record candidate;
        while (db_buffer[at] && db_buffer[at] != '\n' &&
               db_buffer[at] != '\r') at++;
        if (parse_installed_line(db_buffer + start,
                                 db_buffer + at, &candidate) != 0 ||
            !streq(candidate.id, id)) {
            uint64_t line_size = at - start;
            if (line_size + 1 > sizeof(db_next_buffer) - out - 1) return -1;
            memory_copy(db_next_buffer + out, db_buffer + start, line_size);
            out += line_size;
            db_next_buffer[out++] = '\n';
            db_next_buffer[out] = 0;
        }
        while (db_buffer[at] == '\n' || db_buffer[at] == '\r') at++;
    }
    memory_copy(db_buffer, db_next_buffer, out + 1);
    *output_length = out;
    return 0;
}

static int record_package(struct package_info* info) {
    uint64_t length;
    const char* fields[7];

    sanitize_record_text(info->id);
    sanitize_record_text(info->name);
    sanitize_record_text(info->version);
    sanitize_record_text(info->description);
    sanitize_record_text(info->entry);
    sanitize_record_text(info->provides);
    if (database_without(info->id, &length) != 0) return -1;

    fields[0] = info->id;
    fields[1] = info->name;
    fields[2] = info->version;
    fields[3] = kind_name(info->kind);
    fields[4] = compatibility_name(info->compatibility);
    fields[5] = info->entry;
    fields[6] = info->description;

    for (uint64_t i = 0; i < 7; i++) {
        if (append_buffer(db_buffer, sizeof(db_buffer), &length,
                          fields[i]) != 0 ||
            append_buffer(db_buffer, sizeof(db_buffer), &length,
                          "|") != 0) {
            return -1;
        }
    }
    if (append_buffer(db_buffer, sizeof(db_buffer), &length,
                      u64_text(info->file_count)) != 0 ||
        append_buffer(db_buffer, sizeof(db_buffer), &length, "|") != 0 ||
        append_buffer(db_buffer, sizeof(db_buffer), &length,
                      u64_text(info->installed_bytes)) != 0 ||
        append_buffer(db_buffer, sizeof(db_buffer), &length, "|") != 0 ||
        append_buffer(db_buffer, sizeof(db_buffer), &length,
                      info->provides) != 0 ||
        append_buffer(db_buffer, sizeof(db_buffer), &length, "\n") != 0) {
        return -1;
    }
    return write_database(db_buffer, length);
}

static int write_manifest(struct package_info* info) {
    char manifest[4096];
    char path[UNI_PATH_CAP];
    char line[UNI_PATH_CAP + 16];
    uint64_t length = 0;
    uint16_t payload_path_count = created_count;
    int fd;

    sanitize_record_text(info->id);
    sanitize_record_text(info->name);
    sanitize_record_text(info->version);
    sanitize_record_text(info->architecture);
    sanitize_record_text(info->description);
    sanitize_record_text(info->depends);
    sanitize_record_text(info->pre_depends);
    sanitize_record_text(info->provides);
    sanitize_record_text(info->entry);
    manifest[0] = 0;
#define MANIFEST_TEXT(key, value)                                              \
    do {                                                                       \
        if (append_buffer(manifest, sizeof(manifest), &length, key "=") != 0 ||\
            append_buffer(manifest, sizeof(manifest), &length, value) != 0 ||  \
            append_buffer(manifest, sizeof(manifest), &length, "\n") != 0)     \
            return -1;                                                         \
    } while (0)
    MANIFEST_TEXT("format", "uni-1");
    MANIFEST_TEXT("package", info->id);
    MANIFEST_TEXT("name", info->name);
    MANIFEST_TEXT("version", info->version);
    MANIFEST_TEXT("architecture", info->architecture);
    MANIFEST_TEXT("source", kind_name(info->kind));
    MANIFEST_TEXT("compression", info->compression);
    MANIFEST_TEXT("compatibility", compatibility_name(info->compatibility));
    MANIFEST_TEXT("entry", info->entry);
    MANIFEST_TEXT("description", info->description);
    MANIFEST_TEXT("depends", info->depends);
    MANIFEST_TEXT("pre-depends", info->pre_depends);
    MANIFEST_TEXT("provides", info->provides);
    MANIFEST_TEXT("files", u64_text(info->file_count));
    MANIFEST_TEXT("bytes", u64_text(info->installed_bytes));
#undef MANIFEST_TEXT

    path[0] = 0;
    if (append_text(path, sizeof(path), UNI_MANIFEST_ROOT "/") != 0 ||
        append_text(path, sizeof(path), info->id) != 0 ||
        append_text(path, sizeof(path), ".manifest") != 0 ||
        ensure_directory_tree(path, 0) != 0 ||
        path_exists(path, 0) || path_exists(path, 1)) {
        return -1;
    }

    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path,
                       O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    if (write_all_fd_checked(fd, manifest, length) != 0) {
        syscall1(SYS_CLOSE, fd);
        syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0);
        return -1;
    }
    for (uint16_t i = payload_path_count; i > 0; i--) {
        uint16_t index = i - 1;
        uint64_t line_length;

        line[0] = 0;
        if (append_text(line, sizeof(line),
                        created_is_dir[index] ? "path=d|" : "path=f|") != 0 ||
            append_text(line, sizeof(line), created_paths[index]) != 0 ||
            append_char(line, sizeof(line), '\n') != 0) {
            syscall1(SYS_CLOSE, fd);
            syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0);
            return -1;
        }
        line_length = cstrlen(line);
        if (write_all_fd_checked(fd, line, line_length) != 0) {
            syscall1(SYS_CLOSE, fd);
            syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0);
            return -1;
        }
    }
    syscall1(SYS_CLOSE, fd);
    return remember_created(path, 0);
}

static int architecture_supported(const char* architecture) {
    return !architecture[0] || streq(architecture, "all") ||
           streq(architecture, "amd64") ||
           streq(architecture, "x86_64");
}

static void package_defaults(struct package_info* info,
                             enum package_kind kind,
                             const char* source_path) {
    const char* leaf = path_basename(source_path);
    char base[UNI_ID_CAP];
    uint64_t length;

    memory_zero(info, sizeof(*info));
    info->kind = kind;
    info->system_install = kind == PACKAGE_KIND_DEB;
    info->compatibility = COMPAT_DATA_ONLY;
    copy_text(base, sizeof(base), leaf);
    length = cstrlen(base);
    if (kind == PACKAGE_KIND_DEB && ends_with(base, ".deb")) {
        base[length - 4] = 0;
    } else if (kind == PACKAGE_KIND_AINSTALL &&
               ends_with(base, ".ainstall")) {
        base[length - 9] = 0;
    }
    id_from_text(info->id, sizeof(info->id), base);
    copy_text(info->name, sizeof(info->name), info->id);
    copy_text(info->version, sizeof(info->version), "unknown");
    copy_text(info->architecture, sizeof(info->architecture), "all");
    copy_text(info->description, sizeof(info->description),
              "Locally installed application");
}

static void print_package_info(const struct package_info* info) {
    write_text("Package:       ");
    write_text(info->id);
    write_text("\nName:          ");
    write_text(info->name);
    write_text("\nVersion:       ");
    write_text(info->version);
    write_text("\nArchitecture:  ");
    write_text(info->architecture);
    write_text("\nFormat:        ");
    write_text(kind_name(info->kind));
    write_text("\nCompression:   ");
    write_text(info->compression[0] ? info->compression : "unknown");
    write_text("\nCompatibility: ");
    write_text(compatibility_name(info->compatibility));
    write_text("\nEntry:         ");
    write_text(info->entry[0] ? info->entry :
               (info->entry_hint[0] ? info->entry_hint : "(none)"));
    write_text("\nDescription:   ");
    write_text(info->description[0] ? info->description : "(none)");
    write_text("\nDepends:       ");
    write_text(info->depends[0] ? info->depends : "(none)");
    write_text("\nPre-Depends:   ");
    write_text(info->pre_depends[0] ? info->pre_depends : "(none)");
    write_text("\nProvides:      ");
    write_text(info->provides[0] ? info->provides : "(none)");
    write_text("\n");
}

static int open_deb_archive(const char* source_path,
                            struct package_info* info,
                            struct deb_archive* archive) {
    int fd;
    uint64_t size;
    struct mapped_blob control;
    char control_compression[16];
    const char* suffix;

    package_defaults(info, PACKAGE_KIND_DEB, source_path);
    memory_zero(&control, sizeof(control));
    memory_zero(archive, sizeof(*archive));

    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD,
                       (long)source_path, O_RDONLY, 0);
    if (fd < 0 || file_size(fd, &size) != 0 ||
        scan_deb_archive(fd, size, archive) != 0) {
        if (fd >= 0) syscall1(SYS_CLOSE, fd);
        return -1;
    }
    if (load_archive_member(fd, &archive->control, &control,
                            control_compression,
                            sizeof(control_compression)) != 0 ||
        read_deb_control(&control, info) != 0) {
        free_blob(&control);
        syscall1(SYS_CLOSE, fd);
        return -1;
    }
    free_blob(&control);
    if (!architecture_supported(info->architecture)) {
        print_error("package architecture is not amd64, x86_64, or all");
        syscall1(SYS_CLOSE, fd);
        return -1;
    }

    suffix = ends_with(archive->data.name, ".tar.gz")
                 ? "gzip"
                 : ends_with(archive->data.name, ".tar.xz")
                       ? "xz"
                       : ends_with(archive->data.name, ".tar.zst")
                             ? "zstd"
                             : ends_with(archive->data.name, ".tar")
                                   ? "none" : "unsupported";
    copy_text(info->compression, sizeof(info->compression), suffix);
    return fd;
}

static int inspect_deb(const char* source_path,
                       struct package_info* info) {
    struct deb_archive archive;
    int fd = open_deb_archive(source_path, info, &archive);
    if (fd < 0) return -1;
    syscall1(SYS_CLOSE, fd);
    return 0;
}

static int find_ainstall_header_end(const char* header, uint64_t size,
                                    uint64_t* end) {
    for (uint64_t i = 0; i + 1 < size; i++) {
        if (header[i] == '\n' && header[i + 1] == '\n') {
            *end = i + 2;
            return 0;
        }
        if (i + 3 < size && header[i] == '\r' &&
            header[i + 1] == '\n' && header[i + 2] == '\r' &&
            header[i + 3] == '\n') {
            *end = i + 4;
            return 0;
        }
    }
    return -1;
}

static int inspect_ainstall(const char* source_path,
                            struct package_info* info,
                            struct mapped_blob* payload,
                            int load_payload) {
    int fd;
    uint64_t total_size;
    uint64_t header_read;
    uint64_t payload_offset;
    char payload_format[24];
    struct ar_member member;

    package_defaults(info, PACKAGE_KIND_AINSTALL, source_path);
    if (payload) memory_zero(payload, sizeof(*payload));
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD,
                       (long)source_path, O_RDONLY, 0);
    if (fd < 0 || file_size(fd, &total_size) != 0 || total_size < 16) {
        if (fd >= 0) syscall1(SYS_CLOSE, fd);
        print_error("could not read .ainstall package");
        return -1;
    }
    header_read = total_size;
    if (header_read >= sizeof(control_buffer)) {
        header_read = sizeof(control_buffer) - 1;
    }
    if (read_at(fd, 0, control_buffer, header_read) != 0) {
        syscall1(SYS_CLOSE, fd);
        return -1;
    }
    control_buffer[header_read] = 0;
    if (!starts_with(control_buffer, "AINSTALL1\n") ||
        find_ainstall_header_end(control_buffer, header_read,
                                 &payload_offset) != 0) {
        syscall1(SYS_CLOSE, fd);
        print_error("invalid AINSTALL1 header");
        return -1;
    }

    if (control_field(control_buffer, "Package",
                      info->id, sizeof(info->id)) == 0) {
        id_from_text(info->id, sizeof(info->id), info->id);
    }
    if (control_field(control_buffer, "Name",
                      info->name, sizeof(info->name)) != 0) {
        copy_text(info->name, sizeof(info->name), info->id);
    }
    control_field(control_buffer, "Version",
                  info->version, sizeof(info->version));
    control_field(control_buffer, "Architecture",
                  info->architecture, sizeof(info->architecture));
    control_field(control_buffer, "Description",
                  info->description, sizeof(info->description));
    control_field(control_buffer, "Depends",
                  info->depends, sizeof(info->depends));
    control_field(control_buffer, "Pre-Depends",
                  info->pre_depends, sizeof(info->pre_depends));
    control_field(control_buffer, "Provides",
                  info->provides, sizeof(info->provides));
    control_field(control_buffer, "Entry",
                  info->entry_hint, sizeof(info->entry_hint));
    if (control_field(control_buffer, "Payload",
                      payload_format, sizeof(payload_format)) != 0) {
        copy_text(payload_format, sizeof(payload_format), "tar");
    }
    if (!architecture_supported(info->architecture) ||
        payload_offset >= total_size) {
        syscall1(SYS_CLOSE, fd);
        print_error("invalid .ainstall architecture or payload");
        return -1;
    }

    memory_zero(&member, sizeof(member));
    member.offset = payload_offset;
    member.size = total_size - payload_offset;
    if (streq(payload_format, "tar")) {
        copy_text(member.name, sizeof(member.name), "data.tar");
    } else if (streq(payload_format, "tar.gz") ||
               streq(payload_format, "gzip")) {
        copy_text(member.name, sizeof(member.name), "data.tar.gz");
    } else if (streq(payload_format, "tar.xz") ||
               streq(payload_format, "xz")) {
        copy_text(member.name, sizeof(member.name), "data.tar.xz");
    } else {
        syscall1(SYS_CLOSE, fd);
        print_error("unsupported .ainstall payload type");
        return -1;
    }

    if (load_payload) {
        if (!payload ||
            load_archive_member(fd, &member, payload,
                                info->compression,
                                sizeof(info->compression)) != 0) {
            syscall1(SYS_CLOSE, fd);
            return -1;
        }
    } else {
        copy_text(info->compression, sizeof(info->compression),
                  ends_with(member.name, ".gz") ? "gzip" :
                  ends_with(member.name, ".xz") ? "xz" : "none");
    }
    syscall1(SYS_CLOSE, fd);
    return 0;
}

static int install_archive_payload(struct package_info* info,
                                   const struct mapped_blob* payload) {
    char base_path[UNI_PATH_CAP];

    if (find_installed_record(info->id, NULL) == 0) {
        print_error("package is already installed; remove it before updating");
        return -1;
    }
    if (!info->system_install) {
        if (join_install_path(base_path, sizeof(base_path),
                              info->id, NULL) != 0 ||
            path_exists(base_path, 1)) {
            print_error("package directory already exists");
            return -1;
        }
    }

    created_count = 0;
    if ((!info->system_install &&
         ensure_directory_tree(base_path, 1) != 0) ||
        extract_data_tar(payload, info) != 0 ||
        write_manifest(info) != 0 ||
        record_package(info) != 0) {
        cleanup_created();
        return -1;
    }
    created_count = 0;

    write_text("Installed ");
    write_text(info->id);
    write_text(" ");
    write_text(info->version);
    write_text(" (");
    write_text(u64_text(info->file_count));
    write_text(" files, ");
    write_text(u64_text(info->installed_bytes));
    write_text(" bytes)\nCompatibility: ");
    write_text(compatibility_name(info->compatibility));
    write_text("\n");
    if (info->compatibility == COMPAT_LINUX_DYNAMIC) {
        write_text("The files are installed locally, but this app needs the Linux runtime before it can launch.\n");
    } else if (info->compatibility == COMPAT_LINUX_STATIC) {
        write_text("This static Linux binary is staged for the experimental Linux ABI lane.\n");
    } else if (info->compatibility == COMPAT_DATA_ONLY) {
        write_text("This package contains data only or has no recognized executable entry.\n");
    }
    return 0;
}

static int install_deb(const char* source_path) {
    struct package_info info;
    struct deb_archive archive;
    int fd;

    if (syscall1(SYS_GETEUID, 0) != 0) {
        print_error("system .deb installation requires root");
        return -1;
    }
    fd = open_deb_archive(source_path, &info, &archive);
    if (fd < 0) return -1;
    if (find_installed_record(info.id, NULL) == 0) {
        syscall1(SYS_CLOSE, fd);
        print_error("package is already installed; remove it before updating");
        return -1;
    }

    created_count = 0;
    if (extract_archive_member_stream(fd, &archive.data, &info) != 0 ||
        write_manifest(&info) != 0 ||
        record_package(&info) != 0) {
        syscall1(SYS_CLOSE, fd);
        cleanup_created();
        print_error("Debian package transaction rolled back");
        return -1;
    }
    syscall1(SYS_CLOSE, fd);
    created_count = 0;

    write_text("Installed ");
    write_text(info.id);
    write_text(" ");
    write_text(info.version);
    write_text(" (");
    write_text(u64_text(info.file_count));
    write_text(" files, ");
    write_text(u64_text(info.installed_bytes));
    write_text(" bytes)\nCompatibility: ");
    write_text(compatibility_name(info.compatibility));
    write_text("\n");
    if (info.skipped_count) {
        write_text("Skipped unsupported archive entries: ");
        write_text(u64_text(info.skipped_count));
        write_text("\n");
    }
    if (info.compatibility == COMPAT_LINUX_DYNAMIC) {
        write_text("Linux runtime compatibility is required to launch this application.\n");
    }
    return 0;
}

static int install_ainstall(const char* source_path) {
    struct package_info info;
    struct mapped_blob payload;
    int result;

    if (inspect_ainstall(source_path, &info, &payload, 1) != 0) return -1;
    result = install_archive_payload(&info, &payload);
    free_blob(&payload);
    return result;
}

static int load_whole_file(const char* path, uint8_t** data,
                           uint64_t* size) {
    int fd = (int)syscall4(SYS_OPENAT, AT_FDCWD,
                           (long)path, O_RDONLY, 0);
    if (fd < 0 || file_size(fd, size) != 0 ||
        *size == 0 || *size > UNI_MAX_ARCHIVE_SIZE) {
        if (fd >= 0) syscall1(SYS_CLOSE, fd);
        return -1;
    }
    *data = map_memory(*size);
    if (!*data || read_exact(fd, *data, *size) != 0) {
        if (*data) unmap_memory(*data, *size);
        *data = NULL;
        syscall1(SYS_CLOSE, fd);
        return -1;
    }
    syscall1(SYS_CLOSE, fd);
    return 0;
}

static int install_appimage(const char* source_path) {
    struct package_info info;
    enum compatibility_kind candidate;
    uint8_t* data = NULL;
    uint64_t size = 0;
    char base_path[UNI_PATH_CAP];
    char app_path[UNI_PATH_CAP];
    int result = -1;

    package_defaults(&info, PACKAGE_KIND_APPIMAGE, source_path);
    copy_text(info.compression, sizeof(info.compression), "squashfs");
    if (load_whole_file(source_path, &data, &size) != 0 ||
        classify_elf(data, size, &candidate) != 0) {
        print_error("AppImage is not a supported x86_64 ELF image");
        goto done;
    }
    info.compatibility = candidate == COMPAT_AOS_NATIVE
                             ? COMPAT_AOS_NATIVE
                             : COMPAT_LINUX_DYNAMIC;
    if (join_install_path(base_path, sizeof(base_path),
                          info.id, NULL) != 0 ||
        join_install_path(app_path, sizeof(app_path),
                          info.id, "app.AppImage") != 0 ||
        path_exists(base_path, 1) ||
        find_installed_record(info.id, NULL) == 0) {
        print_error("AppImage is already installed");
        goto done;
    }
    created_count = 0;
    if (ensure_directory_tree(base_path, 1) != 0 ||
        write_package_file(app_path, data, size, 0755) != 0) {
        cleanup_created();
        goto done;
    }
    copy_text(info.entry, sizeof(info.entry), app_path);
    info.file_count = 1;
    info.installed_bytes = size;
    if (write_manifest(&info) != 0 || record_package(&info) != 0) {
        cleanup_created();
        goto done;
    }
    created_count = 0;
    write_text("Installed ");
    write_text(info.id);
    write_text(" (AppImage; ");
    write_text(compatibility_name(info.compatibility));
    write_text(")\n");
    result = 0;
done:
    if (data) unmap_memory(data, size);
    return result;
}

static int valid_package_id(const char* id) {
    uint64_t length = cstrlen(id);
    if (length == 0 || length >= UNI_ID_CAP ||
        id[0] == '.' || id[length - 1] == '.') return 0;
    for (uint64_t i = 0; i < length; i++) {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '+' || c == '-')) {
            return 0;
        }
        if (c == '.' && i + 1 < length && id[i + 1] == '.') return 0;
    }
    return 1;
}

static int remove_tree(const char* path, unsigned depth) {
    int fd;
    char child[UNI_PATH_CAP];

    if (depth >= 16) return -1;
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path,
                       O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        return syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0) < 0
                   ? -1 : 0;
    }

    for (;;) {
        long got = syscall3(SYS_GETDENTS64, fd,
                            (long)remove_buffers[depth],
                            sizeof(remove_buffers[depth]));
        uint64_t at = 0;
        if (got < 0) {
            syscall1(SYS_CLOSE, fd);
            return -1;
        }
        if (got == 0) break;
        while (at < (uint64_t)got) {
            struct linux_dirent64* entry =
                (struct linux_dirent64*)(remove_buffers[depth] + at);
            if (entry->d_reclen < 20 ||
                at + entry->d_reclen > (uint64_t)got) {
                syscall1(SYS_CLOSE, fd);
                return -1;
            }
            if (!streq(entry->d_name, ".") && !streq(entry->d_name, "..")) {
                if (copy_text(child, sizeof(child), path) != 0 ||
                    append_char(child, sizeof(child), '/') != 0 ||
                    append_text(child, sizeof(child), entry->d_name) != 0 ||
                    remove_tree(child, depth + 1) != 0) {
                    syscall1(SYS_CLOSE, fd);
                    return -1;
                }
            }
            at += entry->d_reclen;
        }
    }
    syscall1(SYS_CLOSE, fd);
    return syscall3(SYS_UNLINKAT, AT_FDCWD,
                    (long)path, AT_REMOVEDIR) < 0 ? -1 : 0;
}

static int manifest_path_for(char* path, uint64_t capacity,
                             const char* id) {
    path[0] = 0;
    return append_text(path, capacity, UNI_MANIFEST_ROOT "/") != 0 ||
           append_text(path, capacity, id) != 0 ||
           append_text(path, capacity, ".manifest") != 0
               ? -1 : 0;
}

static int load_manifest(const char* id, char** data,
                         uint64_t* size, char* path) {
    int fd;
    uint64_t mapping_size;

    *data = NULL;
    *size = 0;
    if (manifest_path_for(path, UNI_PATH_CAP, id) != 0) return -1;
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
    if (fd < 0 || file_size(fd, size) != 0 ||
        *size == 0 || *size >= UNI_MANIFEST_CAP) {
        if (fd >= 0) syscall1(SYS_CLOSE, fd);
        return -1;
    }
    mapping_size = *size + 1;
    *data = (char*)map_memory(mapping_size);
    if (!*data || read_exact(fd, *data, *size) != 0) {
        if (*data) unmap_memory(*data, mapping_size);
        *data = NULL;
        syscall1(SYS_CLOSE, fd);
        return -1;
    }
    syscall1(SYS_CLOSE, fd);
    (*data)[*size] = 0;
    return 0;
}

static int path_has_prefix_component(const char* path, const char* prefix) {
    uint64_t prefix_size = cstrlen(prefix);
    return starts_with(path, prefix) &&
           (path[prefix_size] == 0 || path[prefix_size] == '/');
}

static int validate_manifest_payload_path(const char* path,
                                          const char* package_id,
                                          int system_install) {
    char normalized[UNI_PATH_CAP];
    char expected[UNI_PATH_CAP];
    char private_root[UNI_PATH_CAP];

    if (!path || path[0] != '/' ||
        normalize_archive_path(path + 1, normalized,
                               sizeof(normalized)) != 0) {
        return -1;
    }
    expected[0] = '/';
    expected[1] = 0;
    if (append_text(expected, sizeof(expected), normalized) != 0 ||
        !streq(expected, path) ||
        path_has_prefix_component(path, UNI_STATE_ROOT)) {
        return -1;
    }
    if (system_install) return 0;
    if (join_install_path(private_root, sizeof(private_root),
                          package_id, NULL) != 0 ||
        !path_has_prefix_component(path, private_root)) {
        return -1;
    }
    return 0;
}

static int manifest_matches_package(const char* data, uint64_t size,
                                    const char* id) {
    const char prefix[] = "package=";
    uint64_t at = 0;

    while (at < size) {
        uint64_t start = at;
        while (at < size && data[at] != '\n' && data[at] != '\r') at++;
        if (at - start > sizeof(prefix) - 1 &&
            starts_with(data + start, prefix)) {
            uint64_t value_size = at - start - (sizeof(prefix) - 1);
            return value_size == cstrlen(id) &&
                   copy_bytes(io_buffer, sizeof(io_buffer),
                              data + start + sizeof(prefix) - 1,
                              value_size) == 0 &&
                   streq(io_buffer, id);
        }
        while (at < size && (data[at] == '\n' || data[at] == '\r')) at++;
    }
    return 0;
}

static int validate_manifest_paths(const char* data, uint64_t size,
                                   const char* id, int system_install) {
    uint64_t at = 0;

    if (!manifest_matches_package(data, size, id)) return -1;
    while (at < size) {
        uint64_t start = at;
        while (at < size && data[at] != '\n' && data[at] != '\r') at++;
        if (at - start >= 7 &&
            starts_with(data + start, "path=")) {
            if ((data[start + 5] != 'f' && data[start + 5] != 'd') ||
                data[start + 6] != '|' ||
                copy_bytes(io_buffer, sizeof(io_buffer),
                           data + start + 7, at - start - 7) != 0 ||
                validate_manifest_payload_path(io_buffer, id,
                                               system_install) != 0) {
                return -1;
            }
        }
        while (at < size && (data[at] == '\n' || data[at] == '\r')) at++;
    }
    return 0;
}

static int remove_manifest_paths(const char* data, uint64_t size) {
    uint64_t at = 0;

    while (at < size) {
        uint64_t start = at;
        while (at < size && data[at] != '\n' && data[at] != '\r') at++;
        if (at - start >= 7 && starts_with(data + start, "path=")) {
            int is_dir = data[start + 5] == 'd';
            if (copy_bytes(io_buffer, sizeof(io_buffer),
                           data + start + 7, at - start - 7) != 0) {
                return -1;
            }
            if (is_dir) {
                /*
                 * A later package may now use a directory this package
                 * created. Keep nonempty shared directories in that case.
                 */
                syscall3(SYS_UNLINKAT, AT_FDCWD,
                         (long)io_buffer, AT_REMOVEDIR);
            } else if (path_exists(io_buffer, 0) &&
                       syscall3(SYS_UNLINKAT, AT_FDCWD,
                                (long)io_buffer, 0) < 0) {
                return -1;
            }
        }
        while (at < size && (data[at] == '\n' || data[at] == '\r')) at++;
    }
    return 0;
}

static int remove_package(const char* id) {
    struct installed_record record;
    char base_path[UNI_PATH_CAP];
    char manifest_path[UNI_PATH_CAP];
    char* manifest = NULL;
    uint64_t manifest_size = 0;
    uint64_t database_size;
    int system_install;

    if (!valid_package_id(id) ||
        find_installed_record(id, &record) != 0) {
        print_error("package is not installed");
        return -1;
    }
    system_install = streq(record.kind, "deb");
    if (system_install && syscall1(SYS_GETEUID, 0) != 0) {
        print_error("removing a system .deb package requires root");
        return -1;
    }

    if (load_manifest(id, &manifest, &manifest_size, manifest_path) == 0) {
        if (validate_manifest_paths(manifest, manifest_size, id,
                                    system_install) != 0 ||
            remove_manifest_paths(manifest, manifest_size) != 0) {
            unmap_memory(manifest, manifest_size + 1);
            print_error("package manifest is invalid or a file could not be removed");
            return -1;
        }
        unmap_memory(manifest, manifest_size + 1);
        if (syscall3(SYS_UNLINKAT, AT_FDCWD,
                     (long)manifest_path, 0) < 0) {
            print_error("could not remove package manifest");
            return -1;
        }
    } else {
        if (system_install) {
            print_error("system package manifest is missing; refusing unsafe removal");
            return -1;
        }
        if (join_install_path(base_path, sizeof(base_path), id, NULL) != 0) {
            return -1;
        }
        if (path_exists(base_path, 1) && remove_tree(base_path, 0) != 0) {
            print_error("could not remove all package files");
            return -1;
        }
    }
    if (database_without(id, &database_size) != 0 ||
        write_database(db_buffer, database_size) != 0) {
        print_error("could not update installed package database");
        return -1;
    }
    write_text("Removed ");
    write_text(record.id);
    write_text("\n");
    return 0;
}

static void list_packages(void) {
    uint64_t at = 0;
    int count = 0;

    if (load_database() != 0 || !db_buffer[0]) {
        write_text("No Uni packages are installed.\n");
        return;
    }
    write_text("Installed Uni packages\n");
    while (db_buffer[at]) {
        uint64_t start = at;
        struct installed_record record;
        while (db_buffer[at] && db_buffer[at] != '\n' &&
               db_buffer[at] != '\r') at++;
        if (parse_installed_line(db_buffer + start,
                                 db_buffer + at, &record) == 0) {
            write_text("  ");
            write_text(record.id);
            write_text(" ");
            write_text(record.version);
            write_text("  [");
            write_text(record.compatibility);
            write_text("]\n      ");
            write_text(record.description[0] ? record.description :
                       record.name);
            write_text("\n");
            count++;
        }
        while (db_buffer[at] == '\n' || db_buffer[at] == '\r') at++;
    }
    if (!count) write_text("  (database contains no valid records)\n");
}

static int print_installed_info(const char* id) {
    struct installed_record record;
    if (find_installed_record(id, &record) != 0) return -1;
    write_text("Package:       ");
    write_text(record.id);
    write_text("\nName:          ");
    write_text(record.name);
    write_text("\nVersion:       ");
    write_text(record.version);
    write_text("\nFormat:        ");
    write_text(record.kind);
    write_text("\nCompatibility: ");
    write_text(record.compatibility);
    write_text("\nEntry:         ");
    write_text(record.entry[0] ? record.entry : "(none)");
    write_text("\nDescription:   ");
    write_text(record.description[0] ? record.description : "(none)");
    write_text("\n");
    return 0;
}

static int run_package(uint64_t argc, char** argv) {
    struct installed_record record;
    char* exec_argv[24];
    uint64_t exec_argc = 1;
    uint64_t source_at = 3;
    int experimental = 0;

    if (argc < 3 || !valid_package_id(argv[2]) ||
        find_installed_record(argv[2], &record) != 0) {
        print_error("package is not installed");
        return -1;
    }
    if (source_at < argc && streq(argv[source_at], "--experimental")) {
        experimental = 1;
        source_at++;
    }
    if (!record.entry[0]) {
        print_error("package has no executable entry");
        return -1;
    }
    if (!streq(record.compatibility, "aos-native") &&
        !(experimental &&
          streq(record.compatibility, "linux-static"))) {
        if (streq(record.compatibility, "linux-static")) {
            print_error("static Linux launch requires --experimental");
        } else {
            print_error("package needs the Linux runtime and cannot launch yet");
        }
        return -1;
    }

    exec_argv[0] = record.entry;
    while (source_at < argc && exec_argc + 1 < 24) {
        exec_argv[exec_argc++] = argv[source_at++];
    }
    exec_argv[exec_argc] = NULL;
    syscall3(SYS_EXECVE, (long)record.entry, (long)exec_argv, 0);
    print_error("execve rejected the installed entry");
    return -1;
}

static enum package_kind detect_package_kind(const char* path) {
    if (ends_with(path, ".deb")) return PACKAGE_KIND_DEB;
    if (ends_with(path, ".ainstall")) return PACKAGE_KIND_AINSTALL;
    if (ends_with(path, ".AppImage") ||
        ends_with(path, ".appimage")) return PACKAGE_KIND_APPIMAGE;
    return PACKAGE_KIND_UNKNOWN;
}

static int ascii_is_digit(char value) {
    return value >= '0' && value <= '9';
}

static int ascii_is_alpha(char value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z');
}

static int version_character_order(char value) {
    if (value == '~') return -1;
    if (value == 0) return 0;
    if (ascii_is_alpha(value)) return (unsigned char)value;
    return (unsigned char)value + 256;
}

static int version_part_compare(const char* left, uint64_t left_size,
                                const char* right, uint64_t right_size) {
    uint64_t left_at = 0;
    uint64_t right_at = 0;

    while (left_at < left_size || right_at < right_size) {
        while ((left_at < left_size &&
                !ascii_is_digit(left[left_at])) ||
               (right_at < right_size &&
                !ascii_is_digit(right[right_at]))) {
            char left_char =
                left_at < left_size && !ascii_is_digit(left[left_at])
                    ? left[left_at] : 0;
            char right_char =
                right_at < right_size && !ascii_is_digit(right[right_at])
                    ? right[right_at] : 0;
            int left_order = version_character_order(left_char);
            int right_order = version_character_order(right_char);

            if (left_order != right_order) {
                return left_order < right_order ? -1 : 1;
            }
            if (left_char) left_at++;
            if (right_char) right_at++;
        }

        while (left_at < left_size && left[left_at] == '0') left_at++;
        while (right_at < right_size && right[right_at] == '0') right_at++;
        {
            uint64_t left_end = left_at;
            uint64_t right_end = right_at;
            uint64_t left_digits;
            uint64_t right_digits;

            while (left_end < left_size &&
                   ascii_is_digit(left[left_end])) left_end++;
            while (right_end < right_size &&
                   ascii_is_digit(right[right_end])) right_end++;
            left_digits = left_end - left_at;
            right_digits = right_end - right_at;
            if (left_digits != right_digits) {
                return left_digits < right_digits ? -1 : 1;
            }
            for (uint64_t i = 0; i < left_digits; i++) {
                if (left[left_at + i] != right[right_at + i]) {
                    return left[left_at + i] < right[right_at + i] ? -1 : 1;
                }
            }
            left_at = left_end;
            right_at = right_end;
        }
    }
    return 0;
}

static uint64_t version_epoch(const char* version, uint64_t* body_at) {
    uint64_t epoch = 0;
    uint64_t at = 0;

    while (ascii_is_digit(version[at])) {
        if (epoch <= (UINT64_MAX - 9U) / 10U) {
            epoch = epoch * 10U + (uint64_t)(version[at] - '0');
        }
        at++;
    }
    if (at != 0 && version[at] == ':') {
        *body_at = at + 1;
        return epoch;
    }
    *body_at = 0;
    return 0;
}

static int debian_version_compare(const char* left, const char* right) {
    uint64_t left_body;
    uint64_t right_body;
    uint64_t left_epoch = version_epoch(left, &left_body);
    uint64_t right_epoch = version_epoch(right, &right_body);
    uint64_t left_size = cstrlen(left + left_body);
    uint64_t right_size = cstrlen(right + right_body);
    uint64_t left_revision = left_size;
    uint64_t right_revision = right_size;
    int result;

    if (left_epoch != right_epoch) return left_epoch < right_epoch ? -1 : 1;
    for (uint64_t i = 0; i < left_size; i++) {
        if (left[left_body + i] == '-') left_revision = i;
    }
    for (uint64_t i = 0; i < right_size; i++) {
        if (right[right_body + i] == '-') right_revision = i;
    }
    result = version_part_compare(left + left_body, left_revision,
                                  right + right_body, right_revision);
    if (result != 0) return result;
    return version_part_compare(
        left_revision < left_size
            ? left + left_body + left_revision + 1 : "0",
        left_revision < left_size ? left_size - left_revision - 1 : 1,
        right_revision < right_size
            ? right + right_body + right_revision + 1 : "0",
        right_revision < right_size ? right_size - right_revision - 1 : 1);
}

static int parse_dependency_requirement(
    const char* start, const char* end,
    struct dependency_requirement* requirement) {
    const char* at;
    const char* relation_start = NULL;
    const char* relation_end = NULL;
    const char* version_start = NULL;
    const char* version_end = NULL;

    memory_zero(requirement, sizeof(*requirement));
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    at = start;
    while (at < end && *at != ' ' && *at != '\t' &&
           *at != '(' && *at != ':' && *at != '[' && *at != '<') {
        at++;
    }
    if (at == start ||
        copy_bytes(requirement->name, sizeof(requirement->name),
                   start, (uint64_t)(at - start)) != 0 ||
        !valid_package_id(requirement->name)) {
        return -1;
    }

    for (at = start; at < end; at++) {
        if (*at == '(') {
            relation_start = at + 1;
            break;
        }
    }
    if (!relation_start) return 0;
    while (relation_start < end &&
           (*relation_start == ' ' || *relation_start == '\t')) {
        relation_start++;
    }
    relation_end = relation_start;
    while (relation_end < end &&
           (*relation_end == '<' || *relation_end == '>' ||
            *relation_end == '=')) {
        relation_end++;
    }
    if (relation_end == relation_start ||
        relation_end - relation_start > 2 ||
        copy_bytes(requirement->relation,
                   sizeof(requirement->relation), relation_start,
                   (uint64_t)(relation_end - relation_start)) != 0) {
        return -1;
    }
    version_start = relation_end;
    while (version_start < end &&
           (*version_start == ' ' || *version_start == '\t')) {
        version_start++;
    }
    version_end = version_start;
    while (version_end < end && *version_end != ')' &&
           *version_end != ' ' && *version_end != '\t') {
        version_end++;
    }
    if (version_end == version_start ||
        copy_bytes(requirement->version, sizeof(requirement->version),
                   version_start,
                   (uint64_t)(version_end - version_start)) != 0) {
        return -1;
    }
    return 0;
}

static int dependency_version_matches(
    const struct dependency_requirement* requirement,
    const char* candidate_version) {
    int comparison;

    if (!requirement->relation[0]) return 1;
    if (!candidate_version || !candidate_version[0] ||
        streq(candidate_version, "unknown")) {
        return 0;
    }
    comparison = debian_version_compare(candidate_version,
                                        requirement->version);
    if (streq(requirement->relation, "=")) return comparison == 0;
    if (streq(requirement->relation, ">=")) return comparison >= 0;
    if (streq(requirement->relation, "<=")) return comparison <= 0;
    if (streq(requirement->relation, ">>") ||
        streq(requirement->relation, ">")) return comparison > 0;
    if (streq(requirement->relation, "<<") ||
        streq(requirement->relation, "<")) return comparison < 0;
    return 0;
}

static int provides_name(const char* provides, const char* name) {
    const char* at = provides;

    while (at && *at) {
        const char* end = at;
        struct dependency_requirement provided;
        while (*end && *end != ',') end++;
        if (parse_dependency_requirement(at, end, &provided) == 0 &&
            streq(provided.name, name)) {
            return 1;
        }
        at = *end ? end + 1 : end;
    }
    return 0;
}

static int package_satisfies(
    const struct package_info* package,
    const struct dependency_requirement* requirement) {
    if (!streq(package->id, requirement->name) &&
        !provides_name(package->provides, requirement->name)) {
        return 0;
    }
    return dependency_version_matches(requirement, package->version);
}

static int record_satisfies(
    const struct installed_record* record,
    const struct dependency_requirement* requirement) {
    if (!streq(record->id, requirement->name) &&
        !provides_name(record->provides, requirement->name)) {
        return 0;
    }
    return dependency_version_matches(requirement, record->version);
}

static int builtin_satisfies(
    const struct dependency_requirement* requirement) {
    struct builtin_capability {
        const char* name;
        const char* version;
        const char* path;
    };
    static const struct builtin_capability capabilities[] = {
        { "dpkg", "1.22.0-uni1", "/uni" },
        { "libc6", "2.39", "/lib/x86_64-linux-gnu/libc.so.6" },
        { "wget", "1.21.0-aos1", "/wget" },
    };

    for (uint64_t i = 0;
         i < sizeof(capabilities) / sizeof(capabilities[0]); i++) {
        if (streq(requirement->name, capabilities[i].name) &&
            path_exists(capabilities[i].path, 0) &&
            dependency_version_matches(requirement,
                                       capabilities[i].version)) {
            return 1;
        }
    }
    return 0;
}

static int dependency_available(
    const struct dependency_requirement* requirement,
    uint64_t plan_count) {
    uint64_t at = 0;

    if (builtin_satisfies(requirement)) return 1;
    for (uint64_t i = 0; i < plan_count; i++) {
        if (package_satisfies(&install_plan[i].info, requirement)) return 1;
    }
    while (db_buffer[at]) {
        uint64_t start = at;
        struct installed_record record;
        while (db_buffer[at] && db_buffer[at] != '\n' &&
               db_buffer[at] != '\r') at++;
        if (parse_installed_line(db_buffer + start,
                                 db_buffer + at, &record) == 0 &&
            record_satisfies(&record, requirement)) {
            return 1;
        }
        while (db_buffer[at] == '\n' || db_buffer[at] == '\r') at++;
    }
    return 0;
}

static int dependency_group_satisfied(const char* start, const char* end,
                                      uint64_t plan_count) {
    const char* alternative = start;
    int depth = 0;

    for (const char* at = start;; at++) {
        char value = at < end ? *at : 0;
        if (value == '(') depth++;
        if (value == ')' && depth > 0) depth--;
        if ((value == '|' && depth == 0) || at == end) {
            struct dependency_requirement requirement;
            if (parse_dependency_requirement(alternative, at,
                                             &requirement) == 0 &&
                dependency_available(&requirement, plan_count)) {
                return 1;
            }
            alternative = at + 1;
        }
        if (at == end) break;
    }
    return 0;
}

static int validate_dependency_field(const char* package_id,
                                     const char* field,
                                     uint64_t plan_count) {
    const char* group = field;
    int depth = 0;
    int missing = 0;

    if (!field || !field[0]) return 0;
    for (const char* at = field;; at++) {
        char value = *at;
        if (value == '(') depth++;
        if (value == ')' && depth > 0) depth--;
        if ((value == ',' && depth == 0) || value == 0) {
            const char* end = at;
            while (group < end && (*group == ' ' || *group == '\t')) group++;
            while (end > group && (end[-1] == ' ' || end[-1] == '\t')) end--;
            if (end > group &&
                !dependency_group_satisfied(group, end, plan_count)) {
                write_text("Missing dependency for ");
                write_text(package_id);
                write_text(": ");
                write_all_fd(1, group, (uint64_t)(end - group));
                write_text("\n");
                missing++;
            }
            if (value == 0) break;
            group = at + 1;
        }
    }
    return missing;
}

static int inspect_package_metadata(const char* path,
                                    struct package_info* info) {
    enum package_kind kind = detect_package_kind(path);

    if (kind == PACKAGE_KIND_DEB) return inspect_deb(path, info);
    if (kind == PACKAGE_KIND_AINSTALL) {
        return inspect_ainstall(path, info, NULL, 0);
    }
    if (kind == PACKAGE_KIND_APPIMAGE) {
        package_defaults(info, PACKAGE_KIND_APPIMAGE, path);
        copy_text(info->compression, sizeof(info->compression), "squashfs");
        return path_exists(path, 0) ? 0 : -1;
    }
    return -1;
}

static int prepare_install_plan(uint64_t count, char** paths) {
    int missing = 0;

    if (count == 0 || count > UNI_MAX_INSTALL_PACKAGES) {
        print_error("an install transaction accepts 1 to 24 package files");
        return -1;
    }
    for (uint64_t i = 0; i < count; i++) {
        install_plan[i].path = paths[i];
        if (inspect_package_metadata(paths[i], &install_plan[i].info) != 0) {
            print_error("could not inspect every package in the transaction");
            return -1;
        }
        if (find_installed_record(install_plan[i].info.id, NULL) == 0) {
            print_error("a transaction package is already installed");
            return -1;
        }
        for (uint64_t previous = 0; previous < i; previous++) {
            if (streq(install_plan[previous].info.id,
                      install_plan[i].info.id)) {
                print_error("the transaction contains a duplicate package");
                return -1;
            }
        }
    }
    if (load_database() != 0) {
        print_error("could not read the installed package database");
        return -1;
    }
    for (uint64_t i = 0; i < count; i++) {
        missing += validate_dependency_field(
            install_plan[i].info.id,
            install_plan[i].info.pre_depends, count);
        missing += validate_dependency_field(
            install_plan[i].info.id,
            install_plan[i].info.depends, count);
    }
    if (missing) {
        print_error("dependency check failed; add the missing .deb files to the same install command");
        return -1;
    }
    return 0;
}

static int install_package(const char* path) {
    enum package_kind kind = detect_package_kind(path);
    if (kind == PACKAGE_KIND_DEB) return install_deb(path);
    if (kind == PACKAGE_KIND_AINSTALL) return install_ainstall(path);
    if (kind == PACKAGE_KIND_APPIMAGE) return install_appimage(path);
    print_error("expected a .deb, .ainstall, or .AppImage file");
    return -1;
}

static int install_packages(uint64_t count, char** paths) {
    uint64_t installed = 0;

    if (prepare_install_plan(count, paths) != 0) return -1;
    for (; installed < count; installed++) {
        if (install_package(install_plan[installed].path) != 0) {
            write_text("Rolling back package transaction\n");
            while (installed) {
                installed--;
                if (remove_package(install_plan[installed].info.id) != 0) {
                    print_error("rollback could not remove an installed package");
                }
            }
            return -1;
        }
    }
    return 0;
}

static int inspect_package(const char* path) {
    enum package_kind kind = detect_package_kind(path);
    struct package_info info;

    if (kind == PACKAGE_KIND_DEB) {
        if (inspect_deb(path, &info) != 0) return -1;
        print_package_info(&info);
        return 0;
    }
    if (kind == PACKAGE_KIND_AINSTALL) {
        if (inspect_ainstall(path, &info, NULL, 0) != 0) return -1;
        print_package_info(&info);
        return 0;
    }
    if (kind == PACKAGE_KIND_APPIMAGE) {
        uint8_t* data = NULL;
        uint64_t size = 0;
        enum compatibility_kind candidate;
        package_defaults(&info, PACKAGE_KIND_APPIMAGE, path);
        copy_text(info.compression, sizeof(info.compression), "squashfs");
        if (load_whole_file(path, &data, &size) != 0 ||
            classify_elf(data, size, &candidate) != 0) {
            if (data) unmap_memory(data, size);
            return -1;
        }
        info.compatibility = candidate == COMPAT_AOS_NATIVE
                                 ? COMPAT_AOS_NATIVE
                                 : COMPAT_LINUX_DYNAMIC;
        print_package_info(&info);
        unmap_memory(data, size);
        return 0;
    }
    if (valid_package_id(path) && print_installed_info(path) == 0) return 0;
    print_error("package format or installed package ID was not recognized");
    return -1;
}

static void usage(void) {
    write_text("Uni - AOS local application installer\n");
    write_text("usage:\n");
    write_text("  uni install FILE.deb|FILE.ainstall|FILE.AppImage [...]\n");
    write_text("  uni check FILE.deb|FILE.ainstall|FILE.AppImage [...]\n");
    write_text("  uni inspect FILE | uni info FILE|PACKAGE\n");
    write_text("  uni list\n");
    write_text("  uni run PACKAGE [--experimental] [ARGS...]\n");
    write_text("  uni remove PACKAGE\n");
    write_text("\nDebian policy: .deb payloads install at normal root paths as a root-only transaction. Add dependency package files to the same install command. Maintainer scripts, devices, and links are not executed or installed yet.\n");
}

void aos_main(uint64_t argc, char** argv) {
    int result = -1;
    if (argc == 1 || (argc == 2 &&
        (streq(argv[1], "-h") || streq(argv[1], "--help")))) {
        usage();
        exit_code(0);
    }
    if (argc == 2 && streq(argv[1], "list")) {
        list_packages();
        exit_code(0);
    }
    if (argc >= 3 && streq(argv[1], "run")) {
        result = run_package(argc, argv);
    } else if (argc >= 3 && streq(argv[1], "install")) {
        result = install_packages(argc - 2, argv + 2);
    } else if (argc >= 3 && streq(argv[1], "check")) {
        result = prepare_install_plan(argc - 2, argv + 2);
        if (result == 0) {
            write_text("Package transaction is ready\n");
        }
    } else if (argc == 3 &&
               (streq(argv[1], "inspect") || streq(argv[1], "info"))) {
        result = inspect_package(argv[2]);
    } else if (argc == 3 && streq(argv[1], "remove")) {
        result = remove_package(argv[2]);
    } else {
        usage();
    }
    exit_code(result == 0 ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
