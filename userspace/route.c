/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526

struct aos_netdev_info_user {
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

static struct aos_netdev_info_user netdev;
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

static void write_buf(const char* s, uint64_t len) {
    syscall3(SYS_WRITE, 1, (long)s, (long)len);
}

static void write_cstr(const char* s) {
    write_buf(s, cstrlen(s));
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

static void write_ipv4(const uint8_t ip[4]) {
    write_u64(ip[0]);
    write_cstr(".");
    write_u64(ip[1]);
    write_cstr(".");
    write_u64(ip[2]);
    write_cstr(".");
    write_u64(ip[3]);
}

static void write_hex16(uint16_t v) {
    static const char h[] = "0123456789abcdef";
    char buf[5];
    buf[0] = h[(v >> 12) & 0xf];
    buf[1] = h[(v >> 8) & 0xf];
    buf[2] = h[(v >> 4) & 0xf];
    buf[3] = h[v & 0xf];
    buf[4] = 0;
    write_cstr(buf);
}

static void write_ipv6(const uint8_t ip[16]) {
    for (int i = 0; i < 8; i++) {
        uint16_t group = ((uint16_t)ip[i * 2] << 8) | ip[i * 2 + 1];
        if (i) write_cstr(":");
        write_hex16(group);
    }
}

static int ipv4_is_zero(const uint8_t ip[4]) {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static int ipv6_is_zero(const uint8_t ip[16]) {
    for (int i = 0; i < 16; i++) {
        if (ip[i] != 0) return 0;
    }
    return 1;
}

static void print_ipv4_routes(void) {
    if (!netdev.ipv4_configured) return;

    write_cstr("inet  ");
    write_ipv4(netdev.ipv4_addr);
    write_cstr("/");
    write_u64(netdev.ipv4_prefix);
    write_cstr(" dev ");
    write_cstr(netdev.name);
    write_cstr(" scope link\n");

    if (!ipv4_is_zero(netdev.ipv4_gateway)) {
        write_cstr("default via ");
        write_ipv4(netdev.ipv4_gateway);
        write_cstr(" dev ");
        write_cstr(netdev.name);
        write_cstr(" proto ipv4\n");
    }
}

static void print_ipv6_routes(void) {
    if (!netdev.ipv6_configured) return;

    write_cstr("inet6 ");
    write_ipv6(netdev.ipv6_addr);
    write_cstr("/");
    write_u64(netdev.ipv6_prefix);
    write_cstr(" dev ");
    write_cstr(netdev.name);
    write_cstr(" scope link\n");

    if (!ipv6_is_zero(netdev.ipv6_gateway)) {
        write_cstr("default via ");
        write_ipv6(netdev.ipv6_gateway);
        write_cstr(" dev ");
        write_cstr(netdev.name);
        write_cstr(" proto ipv6\n");
    }
}

void aos_main(void) {
    uint64_t index = 0;
    uint64_t printed = 0;

    while (syscall3(AOS_SYS_NETDEV_INFO, (long)index, (long)&netdev, 0) >= 0) {
        if (netdev.link_up) {
            print_ipv4_routes();
            print_ipv6_routes();
            printed = 1;
        }
        index++;
    }

    if (!printed) {
        write_cstr("route: no active network routes\n");
    }
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile("call aos_main\n");
}
