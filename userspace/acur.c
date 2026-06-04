/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_SOCKET 41
#define SYS_CONNECT 42
#define SYS_EXIT 60
#define SYS_OPENAT 257
#define AOS_SYS_DNS_LOOKUP 534
#define AOS_SYS_SOCKET_BIND_NETDEV 538

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512

#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint8_t sin_addr[4];
    uint8_t sin_zero[8];
} __attribute__((packed));

struct package_entry {
    const char* name;
    const char* host;
    const char* path;
    const char* outfile;
    const char* description;
};

static char db[4096];
static char req[512];
static char rx[512];
static char header[1024];
static char num_buf[21];

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
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static void write_cstr(const char* s) {
    syscall3(SYS_WRITE, 1, (long)s, (long)cstrlen(s));
}

static void write_buf_fd(int fd, const char* s, uint64_t n) {
    uint64_t off = 0;
    while (off < n) {
        long rc = syscall3(SYS_WRITE, fd, (long)(s + off), (long)(n - off));
        if (rc <= 0) break;
        off += (uint64_t)rc;
    }
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

static void put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static int parse_ipv4(const char* s, uint8_t out[4]) {
    uint32_t part = 0;
    uint32_t digits = 0;
    uint32_t index = 0;

    while (*s) {
        if (*s >= '0' && *s <= '9') {
            part = part * 10 + (uint32_t)(*s - '0');
            if (part > 255) return 0;
            digits++;
        } else if (*s == '.') {
            if (digits == 0 || index >= 3) return 0;
            out[index++] = (uint8_t)part;
            part = 0;
            digits = 0;
        } else {
            return 0;
        }
        s++;
    }
    if (digits == 0 || index != 3) return 0;
    out[index] = (uint8_t)part;
    return 1;
}

static int parse_iface_index(const char* s, uint64_t* out) {
    if (!s || s[0] < '0' || s[0] > '7' || s[1] != 0) return 0;
    *out = (uint64_t)(s[0] - '0');
    return 1;
}

static uint64_t append(char* dst, uint64_t at, uint64_t cap, const char* src) {
    while (*src && at + 1 < cap) dst[at++] = *src++;
    dst[at] = 0;
    return at;
}

static uint64_t build_request(const char* host, const char* path) {
    uint64_t n = 0;
    if (!path || !path[0]) path = "/";
    n = append(req, n, sizeof(req), "GET ");
    n = append(req, n, sizeof(req), path);
    n = append(req, n, sizeof(req), " HTTP/1.0\r\nHost: ");
    n = append(req, n, sizeof(req), host);
    n = append(req, n, sizeof(req), "\r\nUser-Agent: AOS-acur/0.1\r\nConnection: close\r\n\r\n");
    return n;
}

static int find_header_end(const char* buf, uint64_t len, uint64_t* body_at) {
    for (uint64_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            *body_at = i + 4;
            return 1;
        }
    }
    return 0;
}

static int load_db(void) {
    int fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"/pakages/pakages.txt", O_RDONLY, 0);
    long total = 0;
    long rc;

    if (fd < 0) {
        fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"pakages/pakages.txt", O_RDONLY, 0);
    }
    if (fd < 0) {
        fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"/pakages.txt", O_RDONLY, 0);
    }
    if (fd < 0) {
        fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"pakages.txt", O_RDONLY, 0);
    }
    if (fd < 0) return -1;

    while (total + 1 < (long)sizeof(db)) {
        rc = syscall3(SYS_READ, fd, (long)(db + total), (long)(sizeof(db) - 1 - (uint64_t)total));
        if (rc < 0) {
            syscall3(SYS_CLOSE, fd, 0, 0);
            return -1;
        }
        if (rc == 0) break;
        total += rc;
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    db[total] = 0;
    return total > 0 ? 0 : -1;
}

static int split_entry(char* line, struct package_entry* out) {
    char* fields[5];
    uint64_t field = 0;

    if (!line || line[0] == 0 || line[0] == '#') return 0;
    fields[field++] = line;
    for (uint64_t i = 0; line[i]; i++) {
        if (line[i] == '|') {
            line[i] = 0;
            if (field >= 5) return 0;
            fields[field++] = &line[i + 1];
        }
    }
    if (field != 5) return 0;
    out->name = fields[0];
    out->host = fields[1];
    out->path = fields[2];
    out->outfile = fields[3];
    out->description = fields[4];
    return 1;
}

static int next_line(uint64_t* pos, char** line) {
    uint64_t start;
    if (!db[*pos]) return 0;
    while (db[*pos] == '\n' || db[*pos] == '\r') (*pos)++;
    if (!db[*pos]) return 0;
    start = *pos;
    while (db[*pos] && db[*pos] != '\n' && db[*pos] != '\r') (*pos)++;
    if (db[*pos]) db[(*pos)++] = 0;
    *line = &db[start];
    return 1;
}

static void print_entry(const struct package_entry* entry) {
    write_cstr(entry->name);
    write_cstr(" -> ");
    write_cstr(entry->outfile);
    write_cstr("\n    ");
    write_cstr(entry->description);
    write_cstr("\n");
}

static int find_entry(const char* name, struct package_entry* out) {
    uint64_t pos = 0;
    char* line;
    struct package_entry entry;

    while (next_line(&pos, &line)) {
        if (split_entry(line, &entry) && streq(entry.name, name)) {
            *out = entry;
            return 0;
        }
    }
    return -1;
}

static void list_entries(void) {
    uint64_t pos = 0;
    char* line;
    struct package_entry entry;
    int count = 0;

    write_cstr("Acur package list\n");
    while (next_line(&pos, &line)) {
        if (split_entry(line, &entry)) {
            print_entry(&entry);
            count++;
        }
    }
    if (!count) write_cstr("acur: package list is empty\n");
}

static void info_entry(const struct package_entry* entry) {
    write_cstr("name: ");
    write_cstr(entry->name);
    write_cstr("\nhost: ");
    write_cstr(entry->host);
    write_cstr("\npath: ");
    write_cstr(entry->path);
    write_cstr("\nout:  ");
    write_cstr(entry->outfile);
    write_cstr("\ndesc: ");
    write_cstr(entry->description);
    write_cstr("\n");
}

static int fetch_entry(const struct package_entry* entry, uint64_t iface_index) {
    struct sockaddr_in addr;
    uint8_t parsed_ip[4];
    uint64_t req_len;
    uint64_t header_len = 0;
    uint64_t saved = 0;
    int body_started = 0;
    int first_arg_is_ip;
    int sock_fd;
    int out_fd;
    long rc;

    out_fd = (int)syscall4(SYS_OPENAT,
                           AT_FDCWD,
                           (long)entry->outfile,
                           O_WRONLY | O_CREAT | O_TRUNC,
                           0);
    if (out_fd < 0) {
        write_cstr("acur: output open failed\n");
        return -1;
    }

    sock_fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd < 0) {
        write_cstr("acur: socket failed\n");
        syscall3(SYS_CLOSE, out_fd, 0, 0);
        return -1;
    }
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, sock_fd, (long)iface_index, 0);
    if (rc < 0) {
        write_cstr("acur: interface bind failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        syscall3(SYS_CLOSE, out_fd, 0, 0);
        return -1;
    }

    for (uint64_t i = 0; i < sizeof(addr); i++) ((uint8_t*)&addr)[i] = 0;
    addr.sin_family = AF_INET;
    put_be16((uint8_t*)&addr.sin_port, 80);

    first_arg_is_ip = parse_ipv4(entry->host, parsed_ip);
    if (first_arg_is_ip) {
        for (uint64_t i = 0; i < sizeof(parsed_ip); i++) addr.sin_addr[i] = parsed_ip[i];
    } else {
        write_cstr("dns: query ");
        write_cstr(entry->host);
        write_cstr("\n");
        rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)entry->host, (long)addr.sin_addr, (long)iface_index);
        if (rc < 0) {
            write_cstr("acur: DNS lookup failed\n");
            syscall3(SYS_CLOSE, sock_fd, 0, 0);
            syscall3(SYS_CLOSE, out_fd, 0, 0);
            return -1;
        }
    }

    rc = syscall3(SYS_CONNECT, sock_fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        write_cstr("acur: connect failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        syscall3(SYS_CLOSE, out_fd, 0, 0);
        return -1;
    }

    req_len = build_request(entry->host, entry->path);
    rc = syscall3(SYS_WRITE, sock_fd, (long)req, req_len);
    if (rc < 0) {
        write_cstr("acur: request write failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        syscall3(SYS_CLOSE, out_fd, 0, 0);
        return -1;
    }

    for (;;) {
        rc = syscall3(SYS_READ, sock_fd, (long)rx, sizeof(rx));
        if (rc < 0) {
            write_cstr("acur: network read failed\n");
            syscall3(SYS_CLOSE, sock_fd, 0, 0);
            syscall3(SYS_CLOSE, out_fd, 0, 0);
            return -1;
        }
        if (rc == 0) break;
        if (!body_started) {
            uint64_t body_at = 0;
            if (header_len + (uint64_t)rc > sizeof(header)) {
                write_cstr("acur: HTTP header too large\n");
                syscall3(SYS_CLOSE, sock_fd, 0, 0);
                syscall3(SYS_CLOSE, out_fd, 0, 0);
                return -1;
            }
            for (uint64_t i = 0; i < (uint64_t)rc; i++) header[header_len + i] = rx[i];
            header_len += (uint64_t)rc;
            if (!find_header_end(header, header_len, &body_at)) continue;
            body_started = 1;
            if (body_at < header_len) {
                uint64_t body_len = header_len - body_at;
                write_buf_fd(out_fd, header + body_at, body_len);
                saved += body_len;
            }
        } else {
            write_buf_fd(out_fd, rx, (uint64_t)rc);
            saved += (uint64_t)rc;
        }
    }

    syscall3(SYS_CLOSE, sock_fd, 0, 0);
    syscall3(SYS_CLOSE, out_fd, 0, 0);
    if (!body_started) {
        write_cstr("acur: no HTTP body received\n");
        return -1;
    }

    write_cstr("acur: fetched ");
    write_cstr(entry->name);
    write_cstr(" -> ");
    write_cstr(entry->outfile);
    write_cstr(" (");
    write_u64(saved);
    write_cstr(" bytes)\n");
    return 0;
}

static void usage(void) {
    write_cstr("usage: acur list | acur info NAME | acur fetch NAME [--iface N]\n");
}

void aos_main(uint64_t argc, char** argv) {
    struct package_entry entry;
    uint64_t iface_index = 0;

    if (argc >= 5 && streq(argv[argc - 2], "--iface")) {
        if (!parse_iface_index(argv[argc - 1], &iface_index)) {
            usage();
            exit_code(1);
        }
        argc -= 2;
    }

    if (argc == 1 || (argc == 2 && streq(argv[1], "list"))) {
        if (load_db() != 0) {
            write_cstr("acur: could not read package list\n");
            exit_code(1);
        }
        list_entries();
        exit_code(0);
    }

    if (argc == 2 && (streq(argv[1], "-h") || streq(argv[1], "--help"))) {
        usage();
        exit_code(0);
    }

    if (argc != 3 || (!streq(argv[1], "info") && !streq(argv[1], "fetch"))) {
        usage();
        exit_code(1);
    }

    if (load_db() != 0) {
        write_cstr("acur: could not read package list\n");
        exit_code(1);
    }
    if (find_entry(argv[2], &entry) != 0) {
        write_cstr("acur: package not found: ");
        write_cstr(argv[2]);
        write_cstr("\n");
        exit_code(1);
    }

    if (streq(argv[1], "info")) {
        info_entry(&entry);
        exit_code(0);
    }

    exit_code(fetch_entry(&entry, iface_index) == 0 ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
