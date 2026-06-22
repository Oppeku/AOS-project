/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_DNS_LOOKUP 534
#define AOS_SYS_DNS_CACHE_INFO 540
#define AOS_SYS_NET_CACHE_FLUSH 541
#define AOS_SYS_DNS_LOOKUP6 543

#define AOS_NET_CACHE_DNS 2
#define AOS_NET_CACHE_ALL_DEVICES 255

struct aos_dns_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t family;
    uint8_t reserved;
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv4[4];
    uint8_t reserved2[4];
    uint8_t ipv6[16];
    char dev_name[16];
    char name[128];
} __attribute__((packed));

static char num_buf[21];
static struct aos_dns_cache_info_user cache_entry;

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

static void write_ipv4(const uint8_t ip[4]) {
    write_u64(ip[0]);
    write_cstr(".");
    write_u64(ip[1]);
    write_cstr(".");
    write_u64(ip[2]);
    write_cstr(".");
    write_u64(ip[3]);
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

static void write_ipv6(const uint8_t ip[16]) {
    for (uint64_t i = 0; i < 8; i++) {
        uint16_t group = ((uint16_t)ip[i * 2] << 8) | ip[i * 2 + 1];
        if (i) write_cstr(":");
        write_hex16(group);
    }
}

static int is_dash_i(const char* s) {
    return s && s[0] == '-' && s[1] == 'i' && s[2] == 0;
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

static int parse_iface_index(const char* s, uint64_t* out) {
    if (!s || s[0] < '0' || s[0] > '7' || s[1] != 0) return 0;
    *out = (uint64_t)(s[0] - '0');
    return 1;
}

static int print_cache(void) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_DNS_CACHE_INFO, (long)i, (long)&cache_entry, 0) == 0) {
            write_cstr(cache_entry.name);
            write_cstr(" dev ");
            if (cache_entry.dev_name[0]) {
                write_cstr(cache_entry.dev_name);
            } else {
                write_cstr("net");
                write_u64(cache_entry.dev_index);
            }
            write_cstr(" -> ");
            if (cache_entry.family == 6) {
                write_ipv6(cache_entry.ipv6);
            } else {
                write_ipv4(cache_entry.ipv4);
            }
            write_cstr(" hits=");
            write_u64(cache_entry.hits);
            write_cstr(" ttl_ticks=");
            write_u64(cache_entry.ttl_ticks);
            write_cstr(" cache\n");
            shown = 1;
        }
    }

    if (!shown) {
        write_cstr("gethost: DNS cache empty\n");
    }
    return shown;
}

void aos_main(uint64_t argc, char** argv) {
    uint8_t ip[4];
    uint8_t ip6[16];
    uint64_t iface_index = 0;
    uint64_t arg = 1;
    int family = 4;
    long rc;

    if (argc >= 2 && cstr_eq(argv[1], "cache")) {
        if (argc >= 3 && cstr_eq(argv[2], "flush")) {
            rc = syscall3(AOS_SYS_NET_CACHE_FLUSH,
                          AOS_NET_CACHE_DNS,
                          AOS_NET_CACHE_ALL_DEVICES,
                          0);
            if (rc < 0) {
                write_cstr("gethost: DNS cache flush failed\n");
                exit_code(1);
            }
            write_cstr("gethost: flushed ");
            write_u64((uint64_t)rc);
            write_cstr(" DNS cache entries\n");
            exit_code(0);
        }
        print_cache();
        exit_code(0);
    }

    while (arg < argc) {
        if (cstr_eq(argv[arg], "-4")) {
            family = 4;
            arg++;
            continue;
        }
        if (cstr_eq(argv[arg], "-6")) {
            family = 6;
            arg++;
            continue;
        }
        if (is_dash_i(argv[arg])) {
            if (arg + 1 >= argc || !parse_iface_index(argv[arg + 1], &iface_index)) {
                write_cstr("usage: gethost [-4|-6] [-i IFACE] HOST | gethost cache\n");
                exit_code(1);
            }
            arg += 2;
            continue;
        }
        break;
    }

    if (argc - arg < 1) {
        write_cstr("usage: gethost [-4|-6] [-i IFACE] HOST | gethost cache\nexample: gethost -6 oppeku.org\n");
        exit_code(1);
    }

    if (family == 6) {
        rc = syscall3(AOS_SYS_DNS_LOOKUP6, (long)argv[arg], (long)ip6, (long)iface_index);
        if (rc < 0) {
            write_cstr("gethost: AAAA lookup failed\n");
            exit_code(1);
        }

        write_cstr(argv[arg]);
        write_cstr(" -> ");
        write_ipv6(ip6);
        write_cstr("\n");
        exit_code(0);
    }

    rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)argv[arg], (long)ip, (long)iface_index);
    if (rc < 0) {
        write_cstr("gethost: lookup failed\n");
        exit_code(1);
    }

    write_cstr(argv[arg]);
    write_cstr(" -> ");
    write_ipv4(ip);
    write_cstr("\n");
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
