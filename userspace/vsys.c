/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_FORK 57
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258
#define SYS_UNLINKAT 263
#define SYS_GETDENTS64 217

#define AOS_SYS_MEM_INFO 509
#define AOS_SYS_UPTIME_INFO 510
#define AOS_SYS_DRIVER_INFO 519
#define AOS_SYS_NETDEV_INFO 526

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512
#define O_DIRECTORY 0x10000

#define SERVICE_DIR "/Vsys/Services"
#define SERVICE_DIR_ALT "/Vsys/Serivices"
#define VSYS_ROOT "/Vsys"
#define SERVICE_MAX 16
#define SERVICE_NAME_MAX 64
#define SERVICE_EXEC_MAX 128
#define SERVICE_FILE_MAX 2048
#define DIRENT_BUF_SIZE 1024
#define EXEC_ARG_MAX 8
#define EXEC_WORK_MAX 256
#define STATE_FILE "/Vsys/state"
#define STATE_FILE_MAX 4096
#define DEFAULT_MIN_BYTES (1ULL * 1024ULL * 1024ULL)
#define DEFAULT_MAX_BYTES (8ULL * 1024ULL * 1024ULL)

struct aos_mem_info {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

struct aos_uptime_info {
    uint64_t ticks;
    uint64_t seconds;
    uint32_t frequency;
    uint32_t reserved;
};

struct aos_driver_info {
    uint8_t type;
    uint8_t claimed;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t irq_line;
    uint8_t reserved[7];
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar[6];
    char driver[32];
    char status[64];
} __attribute__((packed));

struct aos_netdev_info {
    uint8_t type;
    uint8_t link_up;
    uint8_t mac[6];
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t reserved[5];
    char name[16];
    char driver[32];
    char status[64];
    uint8_t ipv4_addr[4];
    uint8_t ipv4_gateway[4];
    uint8_t ipv4_dns[4];
    uint8_t ipv4_prefix;
    uint8_t ipv4_configured;
    uint8_t ipv6_configured;
    uint8_t ipv6_prefix;
    uint8_t reserved2[2];
    uint8_t ipv6_addr[16];
    uint8_t ipv6_gateway[16];
    uint8_t ipv6_dns[16];
    uint8_t reserved3[12];
} __attribute__((packed));

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

struct service_entry {
    char name[SERVICE_NAME_MAX];
    char exec[SERVICE_EXEC_MAX];
    uint64_t min_bytes;
    uint64_t max_bytes;
    uint64_t assigned_bytes;
    int64_t pid;
    int32_t exit_status;
    uint8_t launched;
    uint8_t exited;
};

static char num_buf[21];
static char path_buf[192];
static char dst_path[192];
static char exec_work[EXEC_WORK_MAX];
static char exec_path_buf[EXEC_WORK_MAX];
static const char* exec_argv[EXEC_ARG_MAX + 1];
static char service_text[SERVICE_FILE_MAX];
static char dirent_buf[DIRENT_BUF_SIZE];
static char state_file[STATE_FILE_MAX];
static struct service_entry services[SERVICE_MAX];
static struct aos_mem_info mem_info;
static struct aos_uptime_info uptime_info;

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall4(long n, long a, long b, long c, long d) {
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
                     : "rcx", "r11", "memory");
    return ret;
}

static void exit_code(int code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a && b && a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a && b && a[i] == 0 && b[i] == 0;
}

static int starts_with(const char* s, const char* prefix) {
    uint64_t i = 0;
    while (prefix && prefix[i]) {
        if (!s || s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static void memzero(void* dst, uint64_t n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = 0;
}

static void copy_bytes(char* dst, uint64_t cap, const char* src, uint64_t len) {
    uint64_t i = 0;
    if (!dst || cap == 0) return;
    while (i + 1 < cap && i < len && src && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void copy_cstr(char* dst, uint64_t cap, const char* src) {
    copy_bytes(dst, cap, src, cstrlen(src));
}

static void write_cstr(const char* s) {
    syscall3(SYS_WRITE, 1, (long)s, (long)cstrlen(s));
}

static void write_u64(uint64_t v) {
    char* p = &num_buf[20];
    *p = 0;
    if (v == 0) {
        *--p = '0';
    } else {
        while (v) {
            *--p = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    write_cstr(p);
}

static void write_i64(int64_t v) {
    if (v < 0) {
        write_cstr("-");
        write_u64((uint64_t)(-v));
        return;
    }
    write_u64((uint64_t)v);
}

static void write_mib(uint64_t bytes) {
    write_u64((bytes + 1048575ULL) / 1048576ULL);
    write_cstr(" MiB");
}

static void append_char(char* dst, uint64_t cap, uint64_t* at, char c) {
    if (!dst || !at || cap == 0 || *at + 1 >= cap) return;
    dst[*at] = c;
    *at += 1;
    dst[*at] = 0;
}

static void append_cstr(char* dst, uint64_t cap, uint64_t* at, const char* s) {
    uint64_t i = 0;
    while (s && s[i]) {
        append_char(dst, cap, at, s[i]);
        i++;
    }
}

static void append_u64(char* dst, uint64_t cap, uint64_t* at, uint64_t v) {
    char tmp[21];
    char* p = &tmp[20];
    *p = 0;
    if (v == 0) {
        *--p = '0';
    } else {
        while (v) {
            *--p = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    append_cstr(dst, cap, at, p);
}

static void write_fixed_name(const char* s, uint64_t width) {
    uint64_t len = cstrlen(s);
    write_cstr(s);
    while (len < width) {
        write_cstr(" ");
        len++;
    }
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char* trim_left(const char* s) {
    while (s && is_space(*s)) s++;
    return s;
}

static uint64_t parse_uint(const char* s, uint64_t* consumed) {
    uint64_t v = 0;
    uint64_t i = 0;
    while (s && s[i] >= '0' && s[i] <= '9') {
        v = v * 10ULL + (uint64_t)(s[i] - '0');
        i++;
    }
    if (consumed) *consumed = i;
    return v;
}

static uint64_t parse_memory_value(const char* s) {
    uint64_t used = 0;
    uint64_t value = parse_uint(trim_left(s), &used);
    const char* unit = trim_left(s) + used;

    if (value == 0) return 0;
    if (unit[0] == 'g' || unit[0] == 'G') return value * 1024ULL * 1024ULL * 1024ULL;
    if (unit[0] == 'k' || unit[0] == 'K') return value * 1024ULL;
    if (unit[0] == 'b' || unit[0] == 'B') return value;
    return value * 1024ULL * 1024ULL;
}

static const char* basename_of(const char* path) {
    const char* base = path;
    uint64_t i = 0;
    while (path && path[i]) {
        if (path[i] == '/') base = path + i + 1;
        i++;
    }
    return base;
}

static void make_service_path(const char* name_or_path, char* out, uint64_t out_size) {
    const char* base = basename_of(name_or_path);
    uint64_t at = 0;
    const char* prefix = SERVICE_DIR "/";

    while (prefix[at] && at + 1 < out_size) {
        out[at] = prefix[at];
        at++;
    }
    for (uint64_t i = 0; base && base[i] && at + 1 < out_size; i++) {
        out[at++] = base[i];
    }
    out[at] = 0;
}

static int mkdir_ignore_exists(const char* path) {
    long rc = syscall4(SYS_MKDIRAT, AT_FDCWD, (long)path, 0755, 0);
    return rc == 0 || rc == -17;
}

static int ensure_service_dirs(void) {
    if (!mkdir_ignore_exists(VSYS_ROOT)) return 0;
    if (!mkdir_ignore_exists(SERVICE_DIR)) return 0;
    return 1;
}

static int open_read_all(const char* path, char* dst, uint64_t cap, uint64_t* out_len) {
    long fd;
    uint64_t total = 0;

    if (out_len) *out_len = 0;
    if (!dst || cap == 0) return 0;
    dst[0] = 0;

    fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
    if (fd < 0) return 0;

    while (total + 1 < cap) {
        long rc = syscall3(SYS_READ, fd, (long)(dst + total), (long)(cap - total - 1));
        if (rc <= 0) break;
        total += (uint64_t)rc;
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    dst[total] = 0;
    if (out_len) *out_len = total;
    return 1;
}

static int write_all_file(const char* path, const char* src, uint64_t len) {
    long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    uint64_t off = 0;
    if (fd < 0) return 0;
    while (off < len) {
        long rc = syscall3(SYS_WRITE, fd, (long)(src + off), (long)(len - off));
        if (rc <= 0) {
            syscall3(SYS_CLOSE, fd, 0, 0);
            return 0;
        }
        off += (uint64_t)rc;
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    return 1;
}

static int key_matches(const char* line, uint64_t line_len, const char* key) {
    uint64_t i = 0;
    while (key[i]) {
        if (i >= line_len || line[i] != key[i]) return 0;
        i++;
    }
    while (i < line_len && is_space(line[i])) i++;
    return i < line_len && line[i] == '=';
}

static const char* value_after_equals(const char* line, uint64_t line_len) {
    uint64_t i = 0;
    while (i < line_len && line[i] != '=') i++;
    if (i < line_len) i++;
    while (i < line_len && is_space(line[i])) i++;
    return line + i;
}

static uint64_t value_len(const char* value, const char* line, uint64_t line_len) {
    const char* end = line + line_len;
    while (end > value && is_space(*(end - 1))) end--;
    return (uint64_t)(end - value);
}

static void parse_service_text(struct service_entry* svc, const char* file_name, const char* text) {
    uint64_t pos = 0;

    memzero(svc, sizeof(*svc));
    copy_cstr(svc->name, sizeof(svc->name), file_name);
    svc->min_bytes = DEFAULT_MIN_BYTES;
    svc->max_bytes = DEFAULT_MAX_BYTES;

    while (text && text[pos]) {
        uint64_t start = pos;
        uint64_t len;
        const char* value;
        while (text[pos] && text[pos] != '\n') pos++;
        len = pos - start;
        if (text[pos] == '\n') pos++;

        if (len == 0 || text[start] == '#') continue;

        if (key_matches(text + start, len, "Name")) {
            value = value_after_equals(text + start, len);
            copy_bytes(svc->name, sizeof(svc->name), value, value_len(value, text + start, len));
        } else if (key_matches(text + start, len, "Exec")) {
            value = value_after_equals(text + start, len);
            copy_bytes(svc->exec, sizeof(svc->exec), value, value_len(value, text + start, len));
        } else if (key_matches(text + start, len, "MinMemory") ||
                   key_matches(text + start, len, "MemoryMin") ||
                   key_matches(text + start, len, "X-AOS-MinMemory")) {
            value = value_after_equals(text + start, len);
            svc->min_bytes = parse_memory_value(value);
        } else if (key_matches(text + start, len, "MaxMemory") ||
                   key_matches(text + start, len, "MemoryMax") ||
                   key_matches(text + start, len, "X-AOS-MaxMemory")) {
            value = value_after_equals(text + start, len);
            svc->max_bytes = parse_memory_value(value);
        }
    }

    if (svc->min_bytes == 0) svc->min_bytes = DEFAULT_MIN_BYTES;
    if (svc->max_bytes < svc->min_bytes) svc->max_bytes = svc->min_bytes;
}

static uint64_t load_services(void) {
    long fd;
    uint64_t count = 0;

    fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)SERVICE_DIR, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)SERVICE_DIR_ALT, O_RDONLY | O_DIRECTORY, 0);
    }
    if (fd < 0) return 0;

    for (;;) {
        long got = syscall3(SYS_GETDENTS64, fd, (long)dirent_buf, sizeof(dirent_buf));
        long off = 0;
        if (got <= 0) break;
        while (off < got && count < SERVICE_MAX) {
            struct linux_dirent64* ent = (struct linux_dirent64*)(dirent_buf + off);
            const char* name = ent->d_name;
            uint64_t len = 0;
            if (ent->d_reclen == 0) break;
            while (name[len]) len++;
            if (!(len == 1 && name[0] == '.') && !(len == 2 && name[0] == '.' && name[1] == '.')) {
                uint64_t text_len = 0;
                make_service_path(name, path_buf, sizeof(path_buf));
                if (open_read_all(path_buf, service_text, sizeof(service_text), &text_len) && text_len > 0) {
                    parse_service_text(&services[count], name, service_text);
                    count++;
                }
            }
            off += ent->d_reclen;
        }
    }

    syscall3(SYS_CLOSE, fd, 0, 0);
    return count;
}

static void plan_service_memory(uint64_t count, uint64_t free_bytes) {
    uint64_t reserve = free_bytes / 10ULL;
    uint64_t pool = free_bytes > reserve ? free_bytes - reserve : free_bytes;
    uint64_t min_total = 0;
    uint64_t extra_total = 0;
    uint64_t remaining;

    for (uint64_t i = 0; i < count; i++) {
        min_total += services[i].min_bytes;
        extra_total += services[i].max_bytes - services[i].min_bytes;
        services[i].assigned_bytes = 0;
    }

    if (count == 0) return;

    if (min_total > pool) {
        for (uint64_t i = 0; i < count; i++) {
            services[i].assigned_bytes = (services[i].min_bytes * pool) / min_total;
        }
        return;
    }

    remaining = pool - min_total;
    for (uint64_t i = 0; i < count; i++) {
        uint64_t extra = 0;
        services[i].assigned_bytes = services[i].min_bytes;
        if (extra_total > 0 && remaining > 0) {
            extra = ((services[i].max_bytes - services[i].min_bytes) * remaining) / extra_total;
            if (extra > services[i].max_bytes - services[i].min_bytes) {
                extra = services[i].max_bytes - services[i].min_bytes;
            }
        }
        services[i].assigned_bytes += extra;
    }
}

static uint64_t count_drivers(void) {
    struct aos_driver_info info;
    uint64_t count = 0;
    while (syscall3(AOS_SYS_DRIVER_INFO, (long)count, (long)&info, 0) >= 0) {
        count++;
    }
    return count;
}

static uint64_t count_netdevs(void) {
    struct aos_netdev_info info;
    uint64_t count = 0;
    while (syscall3(AOS_SYS_NETDEV_INFO, (long)count, (long)&info, 0) >= 0) {
        count++;
    }
    return count;
}

static void print_status(void) {
    uint64_t service_count;
    uint64_t assigned = 0;

    if (syscall3(AOS_SYS_MEM_INFO, (long)&mem_info, 0, 0) < 0) {
        memzero(&mem_info, sizeof(mem_info));
    }
    if (syscall3(AOS_SYS_UPTIME_INFO, (long)&uptime_info, 0, 0) < 0) {
        memzero(&uptime_info, sizeof(uptime_info));
    }

    service_count = load_services();
    plan_service_memory(service_count, mem_info.free);

    write_cstr("Vsys - Virtual System Manager\n");
    write_cstr("services: ");
    write_u64(service_count);
    write_cstr(" registered, drivers: ");
    write_u64(count_drivers());
    write_cstr(", netdevs: ");
    write_u64(count_netdevs());
    write_cstr("\n");

    write_cstr("memory: total ");
    write_mib(mem_info.total);
    write_cstr(", used ");
    write_mib(mem_info.used);
    write_cstr(", free ");
    write_mib(mem_info.free);
    write_cstr("\n");

    write_cstr("uptime: ");
    write_u64(uptime_info.seconds);
    write_cstr(" seconds\n");

    if (service_count == 0) {
        write_cstr("No services registered. Use: Vsys add SERVICE.desktop\n");
        return;
    }

    write_cstr("\nservice                         min       max       assigned\n");
    for (uint64_t i = 0; i < service_count; i++) {
        assigned += services[i].assigned_bytes;
        write_fixed_name(services[i].name, 32);
        write_mib(services[i].min_bytes);
        write_cstr("  ");
        write_mib(services[i].max_bytes);
        write_cstr("  ");
        write_mib(services[i].assigned_bytes);
        if (services[i].pid > 0) {
            write_cstr("  pid=");
            write_i64(services[i].pid);
        }
        if (services[i].exited) {
            write_cstr("  exit=");
            write_i64((int64_t)services[i].exit_status);
        }
        if (services[i].exec[0]) {
            write_cstr("  ");
            write_cstr(services[i].exec);
        }
        write_cstr("\n");
    }

    write_cstr("free block after Vsys plan: ");
    if (mem_info.free > assigned) {
        write_mib(mem_info.free - assigned);
    } else {
        write_cstr("0 MiB");
    }
    write_cstr("\n");
}

static void usage(void) {
    write_cstr("usage:\n");
    write_cstr("  Vsys\n");
    write_cstr("  Vsys status\n");
    write_cstr("  Vsys services\n");
    write_cstr("  Vsys init\n");
    write_cstr("  Vsys start\n");
    write_cstr("  Vsys run\n");
    write_cstr("  Vsys add SERVICE.desktop\n");
    write_cstr("  Vsys SERVICE.desktop add\n");
    write_cstr("  Vsys remove SERVICE.desktop\n");
    write_cstr("  Vsys SERVICE.desktop remove\n");
    write_cstr("\nservice keys: Name=, Exec=, MinMemory=16M, MaxMemory=64M\n");
}

static int split_exec_line(const char* line, const char** argv, uint64_t max_args) {
    uint64_t argc = 0;
    uint64_t in = 0;
    uint64_t out = 0;

    copy_cstr(exec_work, sizeof(exec_work), line);
    while (exec_work[in] && argc < max_args) {
        while (is_space(exec_work[in])) in++;
        if (!exec_work[in]) break;
        argv[argc++] = exec_work + out;
        while (exec_work[in] && !is_space(exec_work[in]) && out + 1 < sizeof(exec_work)) {
            exec_work[out++] = exec_work[in++];
        }
        exec_work[out++] = 0;
        while (is_space(exec_work[in])) in++;
    }
    argv[argc] = 0;
    return (int)argc;
}

static void build_prefixed_path(const char* prefix, const char* name, char* out, uint64_t cap) {
    uint64_t at = 0;
    for (uint64_t i = 0; prefix && prefix[i] && at + 1 < cap; i++) out[at++] = prefix[i];
    for (uint64_t i = 0; name && name[i] && at + 1 < cap; i++) out[at++] = name[i];
    out[at] = 0;
}

static void exec_service_current(const struct service_entry* svc) {
    const char* program;
    const char* base;

    if (!svc || !svc->exec[0] || split_exec_line(svc->exec, exec_argv, EXEC_ARG_MAX) <= 0) {
        exit_code(127);
    }

    program = exec_argv[0];
    base = basename_of(program);

    syscall3(SYS_EXECVE, (long)program, (long)exec_argv, 0);

    build_prefixed_path("/", base, exec_path_buf, sizeof(exec_path_buf));
    syscall3(SYS_EXECVE, (long)exec_path_buf, (long)exec_argv, 0);

    build_prefixed_path("/commands/", base, exec_path_buf, sizeof(exec_path_buf));
    syscall3(SYS_EXECVE, (long)exec_path_buf, (long)exec_argv, 0);

    build_prefixed_path("/commands/", program, exec_path_buf, sizeof(exec_path_buf));
    syscall3(SYS_EXECVE, (long)exec_path_buf, (long)exec_argv, 0);

    exit_code(127);
}

static int write_runtime_state(uint64_t count, const char* mode) {
    uint64_t at = 0;

    append_cstr(state_file, sizeof(state_file), &at, "mode=");
    append_cstr(state_file, sizeof(state_file), &at, mode);
    append_char(state_file, sizeof(state_file), &at, '\n');
    append_cstr(state_file, sizeof(state_file), &at, "services=");
    append_u64(state_file, sizeof(state_file), &at, count);
    append_char(state_file, sizeof(state_file), &at, '\n');
    append_cstr(state_file, sizeof(state_file), &at, "free=");
    append_u64(state_file, sizeof(state_file), &at, mem_info.free);
    append_char(state_file, sizeof(state_file), &at, '\n');

    for (uint64_t i = 0; i < count; i++) {
        append_cstr(state_file, sizeof(state_file), &at, "service=");
        append_cstr(state_file, sizeof(state_file), &at, services[i].name);
        append_cstr(state_file, sizeof(state_file), &at, " pid=");
        append_u64(state_file, sizeof(state_file), &at, services[i].pid > 0 ? (uint64_t)services[i].pid : 0);
        append_cstr(state_file, sizeof(state_file), &at, " assigned=");
        append_u64(state_file, sizeof(state_file), &at, services[i].assigned_bytes);
        append_cstr(state_file, sizeof(state_file), &at, " state=");
        if (services[i].exited) {
            append_cstr(state_file, sizeof(state_file), &at, "exited exit=");
            append_u64(state_file, sizeof(state_file), &at, (uint64_t)services[i].exit_status);
        } else if (services[i].launched) {
            append_cstr(state_file, sizeof(state_file), &at, "running");
        } else {
            append_cstr(state_file, sizeof(state_file), &at, "planned");
        }
        if (services[i].exec[0]) {
            append_cstr(state_file, sizeof(state_file), &at, " exec=");
            append_cstr(state_file, sizeof(state_file), &at, services[i].exec);
        }
        append_char(state_file, sizeof(state_file), &at, '\n');
    }

    return write_all_file(STATE_FILE, state_file, at);
}

static uint64_t prepare_services(void) {
    uint64_t service_count;

    if (!ensure_service_dirs()) {
        write_cstr("Vsys: failed to create /Vsys/Services\n");
        exit_code(1);
    }
    if (syscall3(AOS_SYS_MEM_INFO, (long)&mem_info, 0, 0) < 0) {
        memzero(&mem_info, sizeof(mem_info));
    }
    service_count = load_services();
    plan_service_memory(service_count, mem_info.free);
    return service_count;
}

static void launch_services(int supervise) {
    uint64_t service_count = prepare_services();
    uint64_t launched = 0;

    if (service_count == 0) {
        write_cstr("Vsys: no services registered\n");
        exit_code(1);
    }

    for (uint64_t i = 0; i < service_count; i++) {
        long pid;
        services[i].pid = -1;
        if (!services[i].exec[0]) {
            continue;
        }

        pid = syscall3(SYS_FORK, 0, 0, 0);
        if (pid < 0) {
            write_cstr("Vsys: fork failed for ");
            write_cstr(services[i].name);
            write_cstr("\n");
            continue;
        }
        if (pid == 0) {
            exec_service_current(&services[i]);
        }

        services[i].pid = pid;
        services[i].launched = 1;
        launched++;
        write_cstr("Vsys: launched ");
        write_cstr(services[i].name);
        write_cstr(" pid=");
        write_i64(pid);
        write_cstr(" assigned=");
        write_mib(services[i].assigned_bytes);
        write_cstr("\n");
    }

    if (!write_runtime_state(service_count, supervise ? "supervising" : "started")) {
        write_cstr("Vsys: warning: failed to write /Vsys/state\n");
    }

    if (!supervise) {
        write_cstr("Vsys: state written to /Vsys/state\n");
        write_cstr("Vsys: use 'Vsys run' to supervise services on this scheduler\n");
        exit_code(launched > 0 ? 0 : 1);
    }

    for (uint64_t i = 0; i < service_count; i++) {
        int32_t status = 0;
        long waited;
        if (!services[i].launched || services[i].pid <= 0) continue;
        waited = syscall3(SYS_WAIT4, services[i].pid, (long)&status, 0);
        if (waited >= 0) {
            services[i].exited = 1;
            services[i].exit_status = status;
            write_cstr("Vsys: service exited ");
            write_cstr(services[i].name);
            write_cstr(" pid=");
            write_i64(waited);
            write_cstr(" status=");
            write_i64(status);
            write_cstr("\n");
            (void)write_runtime_state(service_count, "supervising");
        }
    }

    (void)write_runtime_state(service_count, "stopped");
    write_cstr("Vsys: all launched services finished\n");
    exit_code(0);
}

static void add_service(const char* src) {
    uint64_t len = 0;
    if (!ensure_service_dirs()) {
        write_cstr("Vsys: failed to create /Vsys/Services\n");
        exit_code(1);
    }
    if (!open_read_all(src, service_text, sizeof(service_text), &len)) {
        write_cstr("Vsys: failed to read service entry\n");
        exit_code(1);
    }
    make_service_path(src, dst_path, sizeof(dst_path));
    if (!write_all_file(dst_path, service_text, len)) {
        write_cstr("Vsys: failed to register service\n");
        exit_code(1);
    }
    write_cstr("Vsys: registered ");
    write_cstr(dst_path);
    write_cstr("\n");
}

static void remove_service(const char* name_or_path) {
    const char* target = name_or_path;
    if (!starts_with(name_or_path, SERVICE_DIR "/") && !starts_with(name_or_path, SERVICE_DIR_ALT "/")) {
        make_service_path(name_or_path, dst_path, sizeof(dst_path));
        target = dst_path;
    }
    if (syscall4(SYS_UNLINKAT, AT_FDCWD, (long)target, 0, 0) < 0) {
        write_cstr("Vsys: failed to remove service\n");
        exit_code(1);
    }
    write_cstr("Vsys: removed ");
    write_cstr(target);
    write_cstr("\n");
}

static void init_registry(void) {
    if (!ensure_service_dirs()) {
        write_cstr("Vsys: init failed\n");
        exit_code(1);
    }
    write_cstr("Vsys: registry ready at /Vsys/Services\n");
}

void aos_main(uint64_t argc, char** argv) {
    if (argc <= 1) {
        init_registry();
        print_status();
        exit_code(0);
    }

    if (argc == 2) {
        if (streq(argv[1], "status") || streq(argv[1], "services")) {
            init_registry();
            print_status();
            exit_code(0);
        }
        if (streq(argv[1], "start")) {
            launch_services(0);
        }
        if (streq(argv[1], "run")) {
            launch_services(1);
        }
        if (streq(argv[1], "init")) {
            init_registry();
            exit_code(0);
        }
        if (streq(argv[1], "-h") || streq(argv[1], "--help")) {
            usage();
            exit_code(0);
        }
    }

    if (argc == 3) {
        if (streq(argv[1], "add")) {
            add_service(argv[2]);
            exit_code(0);
        }
        if (streq(argv[1], "remove")) {
            remove_service(argv[2]);
            exit_code(0);
        }
        if (streq(argv[2], "add")) {
            add_service(argv[1]);
            exit_code(0);
        }
        if (streq(argv[2], "remove")) {
            remove_service(argv[1]);
            exit_code(0);
        }
    }

    usage();
    exit_code(1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
