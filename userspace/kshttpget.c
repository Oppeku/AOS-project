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

static void write_buf(const char* s, uint64_t n) {
    syscall3(SYS_WRITE, 1, (long)s, (long)n);
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
    n = append(req, n, sizeof(req), "\r\nUser-Agent: AOS-kshttpget/0.1\r\nConnection: close\r\n\r\n");
    return n;
}

void aos_main(uint64_t argc, char** argv) {
    struct sockaddr_in addr;
    const char* ip_text;
    const char* host;
    const char* path;
    int fd;
    long rc;
    uint64_t req_len;
    uint64_t iface_index = 0;
    uint64_t arg = 1;
    int got = 0;
    uint8_t parsed_ip[4];
    int first_arg_is_ip;

    if (argc >= 4 && is_dash_i(argv[arg])) {
        if (!parse_iface_index(argv[arg + 1], &iface_index)) {
            write_cstr("usage: kshttpget [-i IFACE] HOST [PATH]\n");
            exit_code(1);
        }
        arg += 2;
    }

    if (argc - arg < 1) {
        write_cstr("usage: kshttpget [-i IFACE] HOST [PATH]\nexample: kshttpget -i 1 oppeku.org /\n");
        write_cstr("also: kshttpget [-i IFACE] IPV4 HOST [PATH]\n");
        exit_code(1);
    }

    ip_text = argv[arg];
    first_arg_is_ip = parse_ipv4(ip_text, parsed_ip);
    host = argv[arg];
    path = (argc - arg >= 2) ? argv[arg + 1] : "/";
    if (first_arg_is_ip && argc - arg >= 2) {
        host = argv[arg + 1];
        path = (argc - arg >= 3) ? argv[arg + 2] : "/";
    }

    fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        write_cstr("kshttpget: socket failed\n");
        exit_code(1);
    }
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, fd, (long)iface_index, 0);
    if (rc < 0) {
        write_cstr("kshttpget: interface bind failed\n");
        syscall3(SYS_CLOSE, fd, 0, 0);
        exit_code(1);
    }

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
            write_cstr("kshttpget: DNS lookup failed\n");
            syscall3(SYS_CLOSE, fd, 0, 0);
            exit_code(1);
        }
    }
    if (!addr.sin_addr[0] && !addr.sin_addr[1] && !addr.sin_addr[2] && !addr.sin_addr[3]) {
        write_cstr("kshttpget: invalid IPv4 address\n");
        syscall3(SYS_CLOSE, fd, 0, 0);
        exit_code(1);
    }

    rc = syscall3(SYS_CONNECT, fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        write_cstr("kshttpget: connect failed\n");
        syscall3(SYS_CLOSE, fd, 0, 0);
        exit_code(1);
    }

    req_len = build_request(host, path);
    write_cstr("kshttpget: GET ");
    write_cstr(host);
    write_cstr(path);
    write_cstr("\n");
    rc = syscall3(SYS_WRITE, fd, (long)req, req_len);
    if (rc < 0) {
        write_cstr("kshttpget: write failed\n");
        syscall3(SYS_CLOSE, fd, 0, 0);
        exit_code(1);
    }

    for (;;) {
        rc = syscall3(SYS_READ, fd, (long)rx, sizeof(rx));
        if (rc < 0) {
            write_cstr("kshttpget: read failed\n");
            syscall3(SYS_CLOSE, fd, 0, 0);
            exit_code(1);
        }
        if (rc == 0) break;
        got = 1;
        write_buf(rx, (uint64_t)rc);
        if (rc < (long)sizeof(rx)) break;
    }
    if (got) write_cstr("\n");
    syscall3(SYS_CLOSE, fd, 0, 0);
    exit_code(got ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
