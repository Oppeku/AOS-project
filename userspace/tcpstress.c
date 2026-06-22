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
#define AOS_SYS_DNS_LOOKUP 534
#define AOS_SYS_SOCKET_BIND_NETDEV 538

#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint8_t sin_addr[4];
    uint8_t sin_zero[8];
} __attribute__((packed));

static char req[384];
static char rx[512];
static char num_buf[21];

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
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

static void put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static int parse_u64(const char* s, uint64_t* out) {
    uint64_t v = 0;
    if (!s || !s[0]) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint64_t)(*s - '0');
        if (v > 1000) return 0;
        s++;
    }
    *out = v;
    return 1;
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

static int is_dash_i(const char* s) {
    return s && s[0] == '-' && s[1] == 'i' && s[2] == 0;
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

static uint64_t build_request(const char* host, const char* path) {
    uint64_t n = 0;
    if (!path || !path[0]) path = "/";
    n = append(req, n, sizeof(req), "GET ");
    n = append(req, n, sizeof(req), path);
    n = append(req, n, sizeof(req), " HTTP/1.0\r\nHost: ");
    n = append(req, n, sizeof(req), host);
    n = append(req, n, sizeof(req), "\r\nUser-Agent: AOS-tcpstress/0.1\r\nConnection: close\r\n\r\n");
    return n;
}

static int run_one(const struct sockaddr_in* base_addr,
                   const char* host,
                   uint64_t iface_index,
                   uint64_t req_len,
                   uint64_t* bytes) {
    struct sockaddr_in addr = *base_addr;
    int fd;
    long rc;
    int got = 0;

    fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return 0;
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, fd, (long)iface_index, 0);
    if (rc < 0) {
        syscall3(SYS_CLOSE, fd, 0, 0);
        return 0;
    }
    rc = syscall3(SYS_CONNECT, fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        syscall3(SYS_CLOSE, fd, 0, 0);
        return 0;
    }
    rc = syscall3(SYS_WRITE, fd, (long)req, req_len);
    if (rc < 0) {
        syscall3(SYS_CLOSE, fd, 0, 0);
        return 0;
    }

    for (;;) {
        rc = syscall3(SYS_READ, fd, (long)rx, sizeof(rx));
        if (rc < 0) {
            syscall3(SYS_CLOSE, fd, 0, 0);
            return 0;
        }
        if (rc == 0) break;
        *bytes += (uint64_t)rc;
        got = 1;
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    (void)host;
    return got;
}

static void usage(void) {
    write_cstr("usage: tcpstress [-i IFACE] HOST [PATH] [COUNT]\n");
    write_cstr("example: tcpstress oppeku.org / 5\n");
}

void aos_main(uint64_t argc, char** argv) {
    struct sockaddr_in addr;
    const char* host;
    const char* path;
    uint64_t count = 5;
    uint64_t iface_index = 0;
    uint64_t arg = 1;
    uint64_t ok = 0;
    uint64_t bytes = 0;
    uint64_t req_len;
    uint8_t parsed_ip[4];
    int first_arg_is_ip;
    long rc;

    if (argc >= 4 && is_dash_i(argv[arg])) {
        if (!parse_iface_index(argv[arg + 1], &iface_index)) {
            usage();
            exit_code(1);
        }
        arg += 2;
    }
    if (argc - arg < 1 || argc - arg > 3) {
        usage();
        exit_code(1);
    }

    first_arg_is_ip = parse_ipv4(argv[arg], parsed_ip);
    host = argv[arg];
    path = (argc - arg >= 2) ? argv[arg + 1] : "/";
    if (argc - arg >= 3 && !parse_u64(argv[arg + 2], &count)) {
        usage();
        exit_code(1);
    }
    if (count == 0) count = 1;

    for (uint64_t i = 0; i < sizeof(addr); i++) {
        ((uint8_t*)&addr)[i] = 0;
    }
    addr.sin_family = AF_INET;
    put_be16((uint8_t*)&addr.sin_port, 80);
    if (first_arg_is_ip) {
        for (uint64_t i = 0; i < sizeof(parsed_ip); i++) {
            addr.sin_addr[i] = parsed_ip[i];
        }
    } else {
        write_cstr("dns: query ");
        write_cstr(host);
        write_cstr("\n");
        rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)host, (long)addr.sin_addr, (long)iface_index);
        if (rc < 0) {
            write_cstr("tcpstress: DNS lookup failed\n");
            exit_code(1);
        }
    }

    req_len = build_request(host, path);
    write_cstr("tcpstress: ");
    write_u64(count);
    write_cstr(" GETs to ");
    write_cstr(host);
    write_cstr(path);
    write_cstr("\n");

    for (uint64_t i = 0; i < count; i++) {
        if (run_one(&addr, host, iface_index, req_len, &bytes)) {
            ok++;
            write_cstr(".");
        } else {
            write_cstr("x");
        }
    }
    write_cstr("\n");
    write_cstr("tcpstress: ok=");
    write_u64(ok);
    write_cstr(" fail=");
    write_u64(count - ok);
    write_cstr(" bytes=");
    write_u64(bytes);
    write_cstr("\n");
    exit_code(ok == count ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
