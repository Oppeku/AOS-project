/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_ARP_CACHE_INFO 539
#define AOS_SYS_DNS_CACHE_INFO 540
#define AOS_SYS_NET_CACHE_FLUSH 541

#define AOS_NET_CACHE_ARP 1
#define AOS_NET_CACHE_DNS 2
#define AOS_NET_CACHE_ALL_DEVICES 255

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

struct aos_dns_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv4[4];
    uint8_t reserved2[4];
    char dev_name[16];
    char name[128];
} __attribute__((packed));

static char num_buf[21];
static struct aos_arp_cache_info_user arp_entry;
static struct aos_dns_cache_info_user dns_entry;

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
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
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

static void write_hex8(uint8_t v) {
    static const char h[] = "0123456789abcdef";
    char buf[3];

    buf[0] = h[(v >> 4) & 0xf];
    buf[1] = h[v & 0xf];
    buf[2] = 0;
    write_cstr(buf);
}

static void write_mac(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        if (i) write_cstr(":");
        write_hex8(mac[i]);
    }
}

static void write_dev_name(const char* name, uint8_t index) {
    if (name && name[0]) {
        write_cstr(name);
    } else {
        write_cstr("net");
        write_u64(index);
    }
}

static int show_dns_cache(void) {
    int shown = 0;

    write_cstr("DNS cache:\n");
    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_DNS_CACHE_INFO, (long)i, (long)&dns_entry, 0) == 0) {
            write_cstr("  ");
            write_cstr(dns_entry.name);
            write_cstr(" dev ");
            write_dev_name(dns_entry.dev_name, dns_entry.dev_index);
            write_cstr(" -> ");
            write_ipv4(dns_entry.ipv4);
            write_cstr(" hits=");
            write_u64(dns_entry.hits);
            write_cstr(" ttl_ticks=");
            write_u64(dns_entry.ttl_ticks);
            write_cstr("\n");
            shown = 1;
        }
    }
    if (!shown) write_cstr("  empty\n");
    return shown;
}

static int show_arp_cache(void) {
    int shown = 0;

    write_cstr("ARP cache:\n");
    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_ARP_CACHE_INFO, (long)i, (long)&arp_entry, 0) == 0) {
            write_cstr("  ");
            write_ipv4(arp_entry.ipv4);
            write_cstr(" dev ");
            write_dev_name(arp_entry.dev_name, arp_entry.dev_index);
            write_cstr(" lladdr ");
            write_mac(arp_entry.mac);
            write_cstr(" hits=");
            write_u64(arp_entry.hits);
            write_cstr(" ttl_ticks=");
            write_u64(arp_entry.ttl_ticks);
            write_cstr("\n");
            shown = 1;
        }
    }
    if (!shown) write_cstr("  empty\n");
    return shown;
}

static void show_cache(void) {
    show_dns_cache();
    show_arp_cache();
}

static int flush_cache(uint64_t flags) {
    long rc = syscall3(AOS_SYS_NET_CACHE_FLUSH, (long)flags, AOS_NET_CACHE_ALL_DEVICES, 0);

    if (rc < 0) {
        write_cstr("netcache: flush failed\n");
        return 1;
    }

    write_cstr("netcache: flushed ");
    write_u64((uint64_t)rc);
    write_cstr(" cache entries\n");
    return 0;
}

static void usage(void) {
    write_cstr("usage: netcache [show] | netcache flush [all|dns|arp]\n");
}

void aos_main(uint64_t argc, char** argv) {
    if (argc == 1 || (argc >= 2 && cstr_eq(argv[1], "show"))) {
        show_cache();
        exit_code(0);
    }

    if (argc >= 2 && cstr_eq(argv[1], "flush")) {
        uint64_t flags = AOS_NET_CACHE_ARP | AOS_NET_CACHE_DNS;

        if (argc >= 3) {
            if (cstr_eq(argv[2], "dns")) {
                flags = AOS_NET_CACHE_DNS;
            } else if (cstr_eq(argv[2], "arp")) {
                flags = AOS_NET_CACHE_ARP;
            } else if (!cstr_eq(argv[2], "all")) {
                usage();
                exit_code(1);
            }
        }

        exit_code(flush_cache(flags));
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
