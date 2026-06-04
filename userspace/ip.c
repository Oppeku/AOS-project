/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_ARP_CACHE_INFO 539

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

struct aos_arp_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv4[4];
    uint8_t mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
} __attribute__((packed));

static struct aos_netdev_info_user netdev;
static struct aos_arp_cache_info_user arp_entry;

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

static int cstr_eq(const char* a, const char* b) {
    uint64_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void write_u64(uint64_t value) {
    char buf[21];
    int pos = 20;
    buf[pos] = 0;
    if (value == 0) {
        write_cstr("0");
        return;
    }
    while (value && pos > 0) {
        buf[--pos] = (char)('0' + (value % 10));
        value /= 10;
    }
    write_cstr(&buf[pos]);
}

static void write_hex8(uint8_t value) {
    static const char digits[] = "0123456789abcdef";
    char out[3];
    out[0] = digits[(value >> 4) & 0xf];
    out[1] = digits[value & 0xf];
    out[2] = 0;
    write_cstr(out);
}

static void write_hex16(uint16_t value) {
    write_hex8((uint8_t)(value >> 8));
    write_hex8((uint8_t)value);
}

static void write_mac(const uint8_t mac[6]) {
    for (uint64_t i = 0; i < 6; i++) {
        if (i) write_cstr(":");
        write_hex8(mac[i]);
    }
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

static void write_ipv6(const uint8_t ip[16]) {
    for (uint64_t i = 0; i < 8; i++) {
        uint16_t group = ((uint16_t)ip[i * 2] << 8) | ip[i * 2 + 1];
        if (i) write_cstr(":");
        write_hex16(group);
    }
}

static int ipv4_is_zero(const uint8_t ip[4]) {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static int ipv6_is_zero(const uint8_t ip[16]) {
    for (uint64_t i = 0; i < 16; i++) {
        if (ip[i] != 0) return 0;
    }
    return 1;
}

static void usage(void) {
    write_cstr("usage: ip addr | ip route | ip neigh\n");
}

static void show_addr(void) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) continue;
        shown = 1;
        write_u64(i + 1);
        write_cstr(": ");
        write_cstr(netdev.name[0] ? netdev.name : "?");
        write_cstr(": <");
        write_cstr(netdev.link_up ? "UP" : "DOWN");
        write_cstr("> driver ");
        write_cstr(netdev.driver[0] ? netdev.driver : "?");
        write_cstr("\n    link/ether ");
        write_mac(netdev.mac);
        write_cstr("\n");
        if (netdev.ipv4_configured) {
            write_cstr("    inet ");
            write_ipv4(netdev.ipv4_addr);
            write_cstr("/");
            write_u64(netdev.ipv4_prefix);
            write_cstr("\n");
        }
        if (netdev.ipv6_configured) {
            write_cstr("    inet6 ");
            write_ipv6(netdev.ipv6_addr);
            write_cstr("/");
            write_u64(netdev.ipv6_prefix);
            write_cstr("\n");
        }
    }
    if (!shown) write_cstr("ip: no network interfaces\n");
}

static void show_route(void) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) continue;
        if (!netdev.link_up) continue;
        if (netdev.ipv4_configured) {
            shown = 1;
            write_cstr("inet ");
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
        if (netdev.ipv6_configured) {
            shown = 1;
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
    }
    if (!shown) write_cstr("ip: no active routes\n");
}

static void show_neigh(void) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_ARP_CACHE_INFO, (long)i, (long)&arp_entry, 0) < 0) continue;
        shown = 1;
        write_ipv4(arp_entry.ipv4);
        write_cstr(" dev ");
        if (arp_entry.dev_name[0]) {
            write_cstr(arp_entry.dev_name);
        } else {
            write_cstr("net");
            write_u64(arp_entry.dev_index);
        }
        write_cstr(" lladdr ");
        write_mac(arp_entry.mac);
        write_cstr(" REACHABLE hits ");
        write_u64(arp_entry.hits);
        write_cstr(" ttl_ticks ");
        write_u64(arp_entry.ttl_ticks);
        write_cstr("\n");
    }
    if (!shown) write_cstr("ip: no cached neighbors\n");
}

void aos_main(uint64_t argc, char** argv) {
    if (argc < 2 || cstr_eq(argv[1], "-h") || cstr_eq(argv[1], "--help")) {
        usage();
        exit_code(argc < 2 ? 1 : 0);
    }

    if (cstr_eq(argv[1], "addr") || cstr_eq(argv[1], "address") || cstr_eq(argv[1], "a")) {
        show_addr();
        exit_code(0);
    }
    if (cstr_eq(argv[1], "route") || cstr_eq(argv[1], "r")) {
        show_route();
        exit_code(0);
    }
    if (cstr_eq(argv[1], "neigh") || cstr_eq(argv[1], "neighbor") || cstr_eq(argv[1], "n")) {
        show_neigh();
        exit_code(0);
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
