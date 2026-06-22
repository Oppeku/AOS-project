/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_NETDEV_IPV6_CONFIG 531
#define AOS_SYS_ARP_CACHE_INFO 539
#define AOS_SYS_NDP_CACHE_INFO 542

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

struct aos_ndp_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv6[16];
    uint8_t mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
} __attribute__((packed));

static struct aos_netdev_info_user netdev;
static struct aos_arp_cache_info_user arp_entry;
static struct aos_ndp_cache_info_user ndp_entry;

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall5(long n, long a, long b, long c, long d, long e) {
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8)
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

static void memzero(void* dst, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) *d++ = 0;
}

static void memcopy(void* dst, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
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

static int parse_u64_limited(const char* s, uint64_t max, uint64_t* out) {
    uint64_t v = 0;

    if (!s || !*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint64_t)(*s - '0');
        if (v > max) return 0;
        s++;
    }
    *out = v;
    return 1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_ipv6(const char* s, uint8_t out[16]) {
    uint16_t groups[8];
    int count = 0;
    int compress = -1;
    const char* p = s;

    for (int i = 0; i < 8; i++) groups[i] = 0;
    if (!s || !s[0]) return 0;

    if (p[0] == ':' && p[1] == ':') {
        compress = 0;
        p += 2;
    }

    while (*p) {
        uint32_t value = 0;
        int digits = 0;

        if (count >= 8) return 0;
        while (*p && *p != ':') {
            int hv = hex_value(*p);
            if (hv < 0 || digits >= 4) return 0;
            value = (value << 4) | (uint32_t)hv;
            digits++;
            p++;
        }
        if (digits == 0) return 0;
        groups[count++] = (uint16_t)value;

        if (*p == ':') {
            p++;
            if (*p == ':') {
                if (compress >= 0) return 0;
                compress = count;
                p++;
                if (!*p) break;
            } else if (!*p) {
                return 0;
            }
        }
    }

    if (compress >= 0) {
        int missing = 8 - count;
        if (missing < 0) return 0;
        for (int i = count - 1; i >= compress; i--) {
            groups[i + missing] = groups[i];
        }
        for (int i = 0; i < missing; i++) {
            groups[compress + i] = 0;
        }
    } else if (count != 8) {
        return 0;
    }

    for (int i = 0; i < 8; i++) {
        out[i * 2] = (uint8_t)(groups[i] >> 8);
        out[i * 2 + 1] = (uint8_t)groups[i];
    }
    return 1;
}

static int parse_ipv6_cidr(const char* s, uint8_t out[16], uint64_t* prefix) {
    char addr[64];
    uint64_t n = 0;

    if (!s) return 0;
    while (s[n] && s[n] != '/' && n + 1 < sizeof(addr)) {
        addr[n] = s[n];
        n++;
    }
    if (s[n] != '/') return 0;
    addr[n] = 0;
    if (!parse_ipv6(addr, out)) return 0;
    return parse_u64_limited(s + n + 1, 128, prefix);
}

static int find_iface(const char* s, uint64_t* out) {
    if (parse_u64_limited(s, 7, out)) return 1;
    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) continue;
        if (cstr_eq(s, netdev.name)) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

static void usage(void) {
    write_cstr("usage: ip [-4|-6] addr | ip [-4|-6] route | ip [-4|-6] neigh\n");
    write_cstr("       ip -6 addr add ADDR/PREFIX dev IFACE [via GATEWAY] [dns DNS]\n");
    write_cstr("       ip -6 route add default via GATEWAY dev IFACE [dns DNS]\n");
}

static int parse_ipv6_config_options(uint64_t argc,
                                     char** argv,
                                     uint64_t start,
                                     uint64_t* iface,
                                     uint8_t gateway[16],
                                     uint8_t dns[16],
                                     int* have_gateway,
                                     int* have_dns) {
    int have_dev = 0;

    *have_gateway = 0;
    *have_dns = 0;
    for (uint64_t i = start; i < argc;) {
        if (cstr_eq(argv[i], "dev") && i + 1 < argc) {
            if (!find_iface(argv[i + 1], iface)) return 0;
            have_dev = 1;
            i += 2;
        } else if (cstr_eq(argv[i], "via") && i + 1 < argc) {
            if (!parse_ipv6(argv[i + 1], gateway)) return 0;
            *have_gateway = 1;
            i += 2;
        } else if (cstr_eq(argv[i], "dns") && i + 1 < argc) {
            if (!parse_ipv6(argv[i + 1], dns)) return 0;
            *have_dns = 1;
            i += 2;
        } else {
            return 0;
        }
    }
    return have_dev;
}

static int apply_ipv6_config(uint64_t iface,
                             const uint8_t addr[16],
                             uint64_t prefix,
                             const uint8_t gateway[16],
                             const uint8_t dns[16]) {
    long rc = syscall5(AOS_SYS_NETDEV_IPV6_CONFIG,
                       (long)iface,
                       (long)addr,
                       (long)prefix,
                       (long)gateway,
                       (long)dns);
    return rc >= 0;
}

static void show_configured_ipv6(uint64_t iface, const char* label) {
    write_cstr(label);
    write_cstr(" dev ");
    if (syscall3(AOS_SYS_NETDEV_INFO, (long)iface, (long)&netdev, 0) == 0 && netdev.name[0]) {
        write_cstr(netdev.name);
    } else {
        write_u64(iface);
    }
    write_cstr("\n");
}

static int add_ipv6_addr(uint64_t argc, char** argv, uint64_t base) {
    uint64_t iface = 0;
    uint64_t prefix = 0;
    uint8_t addr[16];
    uint8_t gateway[16];
    uint8_t dns[16];
    int have_gateway;
    int have_dns;

    memzero(gateway, sizeof(gateway));
    memzero(dns, sizeof(dns));
    if (base + 4 > argc || !parse_ipv6_cidr(argv[base + 2], addr, &prefix)) return 0;
    if (!parse_ipv6_config_options(argc, argv, base + 3, &iface, gateway, dns, &have_gateway, &have_dns)) {
        return 0;
    }
    if (syscall3(AOS_SYS_NETDEV_INFO, (long)iface, (long)&netdev, 0) == 0 && netdev.ipv6_configured) {
        if (!have_gateway) memcopy(gateway, netdev.ipv6_gateway, 16);
        if (!have_dns) memcopy(dns, netdev.ipv6_dns, 16);
    }
    if (!apply_ipv6_config(iface, addr, prefix, gateway, dns)) {
        write_cstr("ip: IPv6 address configure failed\n");
        exit_code(1);
    }
    show_configured_ipv6(iface, "ip: IPv6 address configured");
    return 1;
}

static int add_ipv6_default_route(uint64_t argc, char** argv, uint64_t base) {
    uint64_t iface = 0;
    uint8_t gateway[16];
    uint8_t dns[16];
    uint8_t addr[16];
    uint64_t prefix;
    int have_gateway;
    int have_dns;

    memzero(gateway, sizeof(gateway));
    memzero(dns, sizeof(dns));
    if (base + 5 > argc || !cstr_eq(argv[base + 2], "default")) return 0;
    if (!parse_ipv6_config_options(argc, argv, base + 3, &iface, gateway, dns, &have_gateway, &have_dns)) {
        return 0;
    }
    if (!have_gateway) return 0;
    if (syscall3(AOS_SYS_NETDEV_INFO, (long)iface, (long)&netdev, 0) < 0 || !netdev.ipv6_configured) {
        write_cstr("ip: interface has no IPv6 address\n");
        exit_code(1);
    }
    memcopy(addr, netdev.ipv6_addr, 16);
    prefix = netdev.ipv6_prefix;
    if (!have_dns) memcopy(dns, netdev.ipv6_dns, 16);
    if (!apply_ipv6_config(iface, addr, prefix, gateway, dns)) {
        write_cstr("ip: IPv6 route configure failed\n");
        exit_code(1);
    }
    show_configured_ipv6(iface, "ip: IPv6 default route configured");
    return 1;
}

static void show_addr(int family) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) continue;
        if ((family == 4 && !netdev.ipv4_configured) ||
            (family == 6 && !netdev.ipv6_configured)) {
            continue;
        }
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
        if (family != 6 && netdev.ipv4_configured) {
            write_cstr("    inet ");
            write_ipv4(netdev.ipv4_addr);
            write_cstr("/");
            write_u64(netdev.ipv4_prefix);
            write_cstr("\n");
        }
        if (family != 4 && netdev.ipv6_configured) {
            write_cstr("    inet6 ");
            write_ipv6(netdev.ipv6_addr);
            write_cstr("/");
            write_u64(netdev.ipv6_prefix);
            write_cstr("\n");
        }
    }
    if (!shown) {
        if (family == 4) {
            write_cstr("ip: no IPv4 interfaces\n");
        } else if (family == 6) {
            write_cstr("ip: no IPv6 interfaces\n");
        } else {
            write_cstr("ip: no network interfaces\n");
        }
    }
}

static void show_route(int family) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) continue;
        if (!netdev.link_up) continue;
        if (family != 6 && netdev.ipv4_configured) {
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
        if (family != 4 && netdev.ipv6_configured) {
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
    if (!shown) {
        if (family == 4) {
            write_cstr("ip: no active IPv4 routes\n");
        } else if (family == 6) {
            write_cstr("ip: no active IPv6 routes\n");
        } else {
            write_cstr("ip: no active routes\n");
        }
    }
}

static void show_neigh(int family) {
    int shown = 0;

    if (family != 6) {
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
    }
    if (family != 4) {
        for (uint64_t i = 0; i < 8; i++) {
            if (syscall3(AOS_SYS_NDP_CACHE_INFO, (long)i, (long)&ndp_entry, 0) < 0) continue;
            shown = 1;
            write_ipv6(ndp_entry.ipv6);
            write_cstr(" dev ");
            if (ndp_entry.dev_name[0]) {
                write_cstr(ndp_entry.dev_name);
            } else {
                write_cstr("net");
                write_u64(ndp_entry.dev_index);
            }
            write_cstr(" lladdr ");
            write_mac(ndp_entry.mac);
            write_cstr(" REACHABLE hits ");
            write_u64(ndp_entry.hits);
            write_cstr(" ttl_ticks ");
            write_u64(ndp_entry.ttl_ticks);
            write_cstr("\n");
        }
    }
    if (!shown) {
        if (family == 4) {
            write_cstr("ip: no cached IPv4 neighbors\n");
        } else if (family == 6) {
            write_cstr("ip: no cached IPv6 neighbors\n");
        } else {
            write_cstr("ip: no cached neighbors\n");
        }
    }
}

void aos_main(uint64_t argc, char** argv) {
    int family = 0;
    const char* command = 0;
    uint64_t command_index = 1;

    if (argc < 2 || cstr_eq(argv[1], "-h") || cstr_eq(argv[1], "--help")) {
        usage();
        exit_code(argc < 2 ? 1 : 0);
    }

    if (cstr_eq(argv[1], "-4") || cstr_eq(argv[1], "-6")) {
        family = cstr_eq(argv[1], "-4") ? 4 : 6;
        if (argc < 3) {
            usage();
            exit_code(1);
        }
        command = argv[2];
        command_index = 2;
    } else {
        command = argv[1];
        command_index = 1;
        if (argc == 3) {
            if (cstr_eq(argv[2], "-4")) {
                family = 4;
            } else if (cstr_eq(argv[2], "-6")) {
                family = 6;
            } else {
                usage();
                exit_code(1);
            }
        }
    }

    if (family == 6 && cstr_eq(command, "addr") && argc > command_index + 1) {
        if (cstr_eq(argv[command_index + 1], "add") &&
            add_ipv6_addr(argc, argv, command_index)) {
            exit_code(0);
        }
        usage();
        exit_code(1);
    }
    if (family == 6 && cstr_eq(command, "route") && argc > command_index + 1) {
        if (cstr_eq(argv[command_index + 1], "add") &&
            add_ipv6_default_route(argc, argv, command_index)) {
            exit_code(0);
        }
        usage();
        exit_code(1);
    }
    if (argc > 3) {
        usage();
        exit_code(1);
    }

    if (cstr_eq(command, "addr") || cstr_eq(command, "address") || cstr_eq(command, "a")) {
        show_addr(family);
        exit_code(0);
    }
    if (cstr_eq(command, "route") || cstr_eq(command, "r")) {
        show_route(family);
        exit_code(0);
    }
    if (cstr_eq(command, "neigh") || cstr_eq(command, "neighbor") || cstr_eq(command, "n")) {
        show_neigh(family);
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
