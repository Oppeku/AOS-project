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

static char req[512];
static char rx[512];
static char header[1536];
static char host_buf[128];
static char path_buf[256];
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
    while (*src && at + 1 < cap) {
        dst[at++] = *src++;
    }
    dst[at] = 0;
    return at;
}

static uint64_t build_request(const char* method, const char* host, const char* path) {
    uint64_t n = 0;
    if (!path || !path[0]) path = "/";
    n = append(req, n, sizeof(req), method);
    n = append(req, n, sizeof(req), " ");
    n = append(req, n, sizeof(req), path);
    n = append(req, n, sizeof(req), " HTTP/1.0\r\nHost: ");
    n = append(req, n, sizeof(req), host);
    n = append(req, n, sizeof(req), "\r\nUser-Agent: AOS-curl/0.1\r\nConnection: close\r\n\r\n");
    return n;
}

static int starts_with_http(const char* s) {
    const char* p = "http://";
    uint64_t i = 0;
    while (p[i]) {
        if (s[i] != p[i]) return 0;
        i++;
    }
    return 1;
}

static int copy_url_part(char* dst, uint64_t cap, const char* src, uint64_t len) {
    if (len + 1 > cap) return 0;
    for (uint64_t i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = 0;
    return 1;
}

static int parse_target(const char* target, const char* optional_path, const char** host, const char** path) {
    const char* p = target;
    uint64_t host_len = 0;
    uint64_t path_len = 0;

    if (starts_with_http(target)) p = target + 7;
    while (p[host_len] && p[host_len] != '/') host_len++;
    if (host_len == 0) return 0;
    if (!copy_url_part(host_buf, sizeof(host_buf), p, host_len)) return 0;

    if (p[host_len] == '/') {
        while (p[host_len + path_len]) path_len++;
        if (!copy_url_part(path_buf, sizeof(path_buf), p + host_len, path_len)) return 0;
    } else if (optional_path && optional_path[0]) {
        uint64_t i = 0;
        while (optional_path[i]) i++;
        if (optional_path[0] == '/') {
            if (!copy_url_part(path_buf, sizeof(path_buf), optional_path, i)) return 0;
        } else {
            if (i + 2 > sizeof(path_buf)) return 0;
            path_buf[0] = '/';
            for (uint64_t j = 0; j < i; j++) path_buf[j + 1] = optional_path[j];
            path_buf[i + 1] = 0;
        }
    } else {
        path_buf[0] = '/';
        path_buf[1] = 0;
    }

    *host = host_buf;
    *path = path_buf;
    return 1;
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

static void usage(void) {
    write_cstr("usage: curl [-i] [-I] [-o FILE] [--iface N] URL|HOST [PATH]\n");
    write_cstr("examples: curl oppeku.org / | curl http://oppeku.org/ | curl -o /tmp/page.txt oppeku.org /\n");
}

void aos_main(uint64_t argc, char** argv) {
    struct sockaddr_in addr;
    const char* host = 0;
    const char* path = 0;
    const char* outfile = 0;
    const char* target = 0;
    const char* optional_path = 0;
    const char* method = "GET";
    uint8_t parsed_ip[4];
    uint64_t iface_index = 0;
    uint64_t req_len = 0;
    uint64_t header_len = 0;
    uint64_t saved = 0;
    int include_headers = 0;
    int headers_only = 0;
    int body_started = 0;
    int out_fd = 1;
    int sock_fd;
    int first_arg_is_ip;
    long rc;

    for (uint64_t i = 1; i < argc; i++) {
        if (streq(argv[i], "--help") || streq(argv[i], "-h")) {
            usage();
            exit_code(0);
        } else if (streq(argv[i], "-i")) {
            include_headers = 1;
        } else if (streq(argv[i], "-I")) {
            headers_only = 1;
            method = "HEAD";
        } else if (streq(argv[i], "-o")) {
            if (i + 1 >= argc) {
                usage();
                exit_code(1);
            }
            outfile = argv[++i];
        } else if (streq(argv[i], "--iface")) {
            if (i + 1 >= argc || !parse_iface_index(argv[i + 1], &iface_index)) {
                usage();
                exit_code(1);
            }
            i++;
        } else if (!target) {
            target = argv[i];
        } else if (!optional_path) {
            optional_path = argv[i];
        } else {
            usage();
            exit_code(1);
        }
    }

    if (!target || !parse_target(target, optional_path, &host, &path)) {
        usage();
        exit_code(1);
    }

    if (outfile) {
        out_fd = (int)syscall4(SYS_OPENAT,
                               AT_FDCWD,
                               (long)outfile,
                               O_WRONLY | O_CREAT | O_TRUNC,
                               0);
        if (out_fd < 0) {
            write_cstr("curl: output open failed\n");
            exit_code(1);
        }
    }

    sock_fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd < 0) {
        write_cstr("curl: socket failed\n");
        if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
        exit_code(1);
    }
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, sock_fd, (long)iface_index, 0);
    if (rc < 0) {
        write_cstr("curl: interface bind failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
        exit_code(1);
    }

    for (uint64_t i = 0; i < sizeof(addr); i++) ((uint8_t*)&addr)[i] = 0;
    addr.sin_family = AF_INET;
    put_be16((uint8_t*)&addr.sin_port, 80);

    first_arg_is_ip = parse_ipv4(host, parsed_ip);
    if (first_arg_is_ip) {
        for (uint64_t i = 0; i < sizeof(parsed_ip); i++) addr.sin_addr[i] = parsed_ip[i];
    } else {
        write_cstr("dns: query ");
        write_cstr(host);
        write_cstr("\n");
        rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)host, (long)addr.sin_addr, (long)iface_index);
        if (rc < 0) {
            write_cstr("curl: DNS lookup failed\n");
            syscall3(SYS_CLOSE, sock_fd, 0, 0);
            if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
            exit_code(1);
        }
    }

    rc = syscall3(SYS_CONNECT, sock_fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        write_cstr("curl: connect failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
        exit_code(1);
    }

    req_len = build_request(method, host, path);
    rc = syscall3(SYS_WRITE, sock_fd, (long)req, req_len);
    if (rc < 0) {
        write_cstr("curl: request write failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
        exit_code(1);
    }

    for (;;) {
        rc = syscall3(SYS_READ, sock_fd, (long)rx, sizeof(rx));
        if (rc < 0) {
            write_cstr("curl: network read failed\n");
            syscall3(SYS_CLOSE, sock_fd, 0, 0);
            if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
            exit_code(1);
        }
        if (rc == 0) break;

        if (!body_started) {
            uint64_t body_at = 0;
            if (header_len + (uint64_t)rc > sizeof(header)) {
                write_cstr("curl: HTTP header too large\n");
                syscall3(SYS_CLOSE, sock_fd, 0, 0);
                if (outfile) syscall3(SYS_CLOSE, out_fd, 0, 0);
                exit_code(1);
            }
            for (uint64_t i = 0; i < (uint64_t)rc; i++) header[header_len + i] = rx[i];
            header_len += (uint64_t)rc;
            if (!find_header_end(header, header_len, &body_at)) continue;
            body_started = 1;
            if (include_headers || headers_only) {
                write_buf_fd(out_fd, header, headers_only ? header_len : body_at);
            }
            if (!headers_only && header_len > body_at) {
                write_buf_fd(out_fd, header + body_at, header_len - body_at);
                saved += header_len - body_at;
            }
        } else if (!headers_only) {
            write_buf_fd(out_fd, rx, (uint64_t)rc);
            saved += (uint64_t)rc;
        }
    }

    syscall3(SYS_CLOSE, sock_fd, 0, 0);
    if (outfile) {
        syscall3(SYS_CLOSE, out_fd, 0, 0);
        write_cstr("curl: saved ");
        write_u64(saved);
        write_cstr(" bytes to ");
        write_cstr(outfile);
        write_cstr("\n");
    }
    exit_code(body_started ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
