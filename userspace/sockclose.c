/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

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
            part = part * 10U + (uint32_t)(*s - '0');
            if (part > 255U) return 0;
            digits++;
        } else if (*s == '.') {
            if (digits == 0 || index >= 3U) return 0;
            out[index++] = (uint8_t)part;
            part = 0;
            digits = 0;
        } else {
            return 0;
        }
        s++;
    }
    if (digits == 0 || index != 3U) return 0;
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

static int parse_u16(const char* s, uint16_t* out) {
    uint32_t v = 0;

    if (!s || !*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10U + (uint32_t)(*s - '0');
        if (v > 65535U) return 0;
        s++;
    }
    *out = (uint16_t)v;
    return 1;
}

void aos_main(uint64_t argc, char** argv) {
    struct sockaddr_in addr;
    const char* host;
    uint16_t port = 80;
    uint64_t iface_index = 0;
    uint64_t arg = 1;
    uint8_t parsed_ip[4];
    int fd;
    long rc;

    if (argc >= 4 && is_dash_i(argv[arg])) {
        if (!parse_iface_index(argv[arg + 1], &iface_index)) {
            write_cstr("usage: sockclose [-i IFACE] HOST\n");
            exit_code(1);
        }
        arg += 2;
    }

    if (argc - arg < 1 || argc - arg > 2) {
        write_cstr("usage: sockclose [-i IFACE] HOST [PORT]\n");
        write_cstr("example: sockclose -i 1 oppeku.org 80\n");
        exit_code(1);
    }
    host = argv[arg];
    if (argc - arg == 2 && !parse_u16(argv[arg + 1], &port)) {
        write_cstr("sockclose: bad port\n");
        exit_code(1);
    }

    fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        write_cstr("sockclose: socket failed\n");
        exit_code(1);
    }
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, fd, (long)iface_index, 0);
    if (rc < 0) {
        write_cstr("sockclose: interface bind failed\n");
        syscall3(SYS_CLOSE, fd, 0, 0);
        exit_code(1);
    }

    for (uint64_t i = 0; i < sizeof(addr); i++) {
        ((uint8_t*)&addr)[i] = 0;
    }
    addr.sin_family = AF_INET;
    put_be16((uint8_t*)&addr.sin_port, port);
    if (parse_ipv4(host, parsed_ip)) {
        for (uint64_t i = 0; i < sizeof(parsed_ip); i++) {
            addr.sin_addr[i] = parsed_ip[i];
        }
    } else {
        write_cstr("dns: query ");
        write_cstr(host);
        write_cstr("\n");
        rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)host, (long)addr.sin_addr, (long)iface_index);
        if (rc < 0) {
            write_cstr("sockclose: DNS lookup failed\n");
            syscall3(SYS_CLOSE, fd, 0, 0);
            exit_code(1);
        }
    }

    rc = syscall3(SYS_CONNECT, fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        write_cstr("sockclose: connect failed\n");
        syscall3(SYS_CLOSE, fd, 0, 0);
        exit_code(1);
    }

    write_cstr("sockclose: connected, closing\n");
    rc = syscall3(SYS_CLOSE, fd, 0, 0);
    if (rc < 0) {
        write_cstr("sockclose: close failed\n");
        exit_code(1);
    }
    write_cstr("sockclose: FIN close complete\n");
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
