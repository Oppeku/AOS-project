/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_SOCKET_INFO 533
#define AOS_SYS_NETDEV_STATS 535
#define AOS_SYS_ARP_CACHE_INFO 539
#define AOS_SYS_DNS_CACHE_INFO 540
#define AOS_SYS_NDP_CACHE_INFO 542

#define AF_INET6 10

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

struct aos_netdev_stats_user {
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t tx_dropped;
    uint64_t rx_dropped;
    uint64_t tx_arp;
    uint64_t rx_arp;
    uint64_t tx_ipv4;
    uint64_t rx_ipv4;
    uint64_t tx_ipv6;
    uint64_t rx_ipv6;
    uint64_t tx_icmp;
    uint64_t rx_icmp;
    uint64_t tx_udp;
    uint64_t rx_udp;
    uint64_t tx_tcp;
    uint64_t rx_tcp;
} __attribute__((packed));

struct aos_socket_info_user {
    uint8_t valid;
    uint8_t index;
    uint8_t state;
    uint8_t family;
    uint8_t type;
    uint8_t protocol;
    uint8_t dev_index;
    uint8_t reserved;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t refcount;
    uint32_t rx_len;
    uint32_t rx_off;
    uint32_t seq;
    uint32_t ack;
    uint16_t peer_window;
    uint16_t peer_mss;
    uint32_t peer_window_bytes;
    uint32_t retransmits;
    uint32_t cwnd_bytes;
    uint32_t ssthresh_bytes;
    uint8_t local_window_scale;
    uint8_t peer_window_scale;
    uint16_t reserved4;
    uint8_t remote_ip[4];
    uint8_t next_hop_ip[4];
    uint8_t remote_ip6[16];
    uint8_t next_hop_ip6[16];
    uint8_t remote_mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
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
static struct aos_netdev_stats_user stats;
static struct aos_socket_info_user socket_info;
static struct aos_arp_cache_info_user arp_entry;
static struct aos_dns_cache_info_user dns_entry;
static struct aos_ndp_cache_info_user ndp_entry;

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

static void write_ipv4(const uint8_t ip[4]) {
    write_u64(ip[0]);
    write_cstr(".");
    write_u64(ip[1]);
    write_cstr(".");
    write_u64(ip[2]);
    write_cstr(".");
    write_u64(ip[3]);
}

static void write_hex16(uint16_t value) {
    static const char digits[] = "0123456789abcdef";
    char out[5];

    out[0] = digits[(value >> 12) & 0xf];
    out[1] = digits[(value >> 8) & 0xf];
    out[2] = digits[(value >> 4) & 0xf];
    out[3] = digits[value & 0xf];
    out[4] = 0;
    write_cstr(out);
}

static void write_ipv6(const uint8_t ip[16]) {
    for (uint64_t i = 0; i < 8; i++) {
        uint16_t group = ((uint16_t)ip[i * 2] << 8) | ip[i * 2 + 1];
        if (i) write_cstr(":");
        write_hex16(group);
    }
}

static const char* socket_state(uint8_t state) {
    if (state == 1) return "created";
    if (state == 2) return "connected";
    if (state == 3) return "closed";
    return "free";
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

static int ipv4_is_zero(const uint8_t ip[4]) {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static int ipv6_is_zero(const uint8_t ip[16]) {
    for (uint64_t i = 0; i < 16; i++) {
        if (ip[i] != 0) return 0;
    }
    return 1;
}

static void show_interfaces(void) {
    int shown = 0;

    write_cstr("Interface counters:\n");
    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) {
            continue;
        }
        shown = 1;
        write_cstr("  ");
        write_cstr(netdev.name[0] ? netdev.name : "?");
        write_cstr(" link=");
        write_cstr(netdev.link_up ? "up" : "down");
        write_cstr(" driver=");
        write_cstr(netdev.driver[0] ? netdev.driver : "?");
        if (netdev.ipv4_configured) {
            write_cstr(" inet=");
            write_ipv4(netdev.ipv4_addr);
            write_cstr("/");
            write_u64(netdev.ipv4_prefix);
        }
        write_cstr("\n");

        if (syscall3(AOS_SYS_NETDEV_STATS, (long)i, (long)&stats, 0) >= 0) {
            write_cstr("    rx packets=");
            write_u64(stats.rx_packets);
            write_cstr(" bytes=");
            write_u64(stats.rx_bytes);
            write_cstr(" errors=");
            write_u64(stats.rx_errors);
            write_cstr(" dropped=");
            write_u64(stats.rx_dropped);
            write_cstr("\n");
            write_cstr("    tx packets=");
            write_u64(stats.tx_packets);
            write_cstr(" bytes=");
            write_u64(stats.tx_bytes);
            write_cstr(" errors=");
            write_u64(stats.tx_errors);
            write_cstr(" dropped=");
            write_u64(stats.tx_dropped);
            write_cstr("\n");
            write_cstr("    protocols rx: arp=");
            write_u64(stats.rx_arp);
            write_cstr(" ipv4=");
            write_u64(stats.rx_ipv4);
            write_cstr(" ipv6=");
            write_u64(stats.rx_ipv6);
            write_cstr(" icmp=");
            write_u64(stats.rx_icmp);
            write_cstr(" udp=");
            write_u64(stats.rx_udp);
            write_cstr(" tcp=");
            write_u64(stats.rx_tcp);
            write_cstr("\n");
            write_cstr("    protocols tx: arp=");
            write_u64(stats.tx_arp);
            write_cstr(" ipv4=");
            write_u64(stats.tx_ipv4);
            write_cstr(" ipv6=");
            write_u64(stats.tx_ipv6);
            write_cstr(" icmp=");
            write_u64(stats.tx_icmp);
            write_cstr(" udp=");
            write_u64(stats.tx_udp);
            write_cstr(" tcp=");
            write_u64(stats.tx_tcp);
            write_cstr("\n");
        }
    }
    if (!shown) {
        write_cstr("  none\n");
    }
}

static void show_routes(void) {
    int shown = 0;

    write_cstr("Routing table:\n");
    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) {
            continue;
        }
        if (!netdev.link_up) {
            continue;
        }
        if (netdev.ipv4_configured) {
            shown = 1;
            write_cstr("  inet  ");
            write_ipv4(netdev.ipv4_addr);
            write_cstr("/");
            write_u64(netdev.ipv4_prefix);
            write_cstr(" dev ");
            write_cstr(netdev.name[0] ? netdev.name : "?");
            write_cstr(" scope link\n");

            if (!ipv4_is_zero(netdev.ipv4_gateway)) {
                write_cstr("  default via ");
                write_ipv4(netdev.ipv4_gateway);
                write_cstr(" dev ");
                write_cstr(netdev.name[0] ? netdev.name : "?");
                write_cstr(" proto ipv4\n");
            }
        }
        if (netdev.ipv6_configured) {
            shown = 1;
            write_cstr("  inet6 ");
            write_ipv6(netdev.ipv6_addr);
            write_cstr("/");
            write_u64(netdev.ipv6_prefix);
            write_cstr(" dev ");
            write_cstr(netdev.name[0] ? netdev.name : "?");
            write_cstr(" scope link\n");

            if (!ipv6_is_zero(netdev.ipv6_gateway)) {
                write_cstr("  default via ");
                write_ipv6(netdev.ipv6_gateway);
                write_cstr(" dev ");
                write_cstr(netdev.name[0] ? netdev.name : "?");
                write_cstr(" proto ipv6\n");
            }
        }
    }
    if (!shown) {
        write_cstr("  none\n");
    }
}

static void show_protocol_stats(void) {
    uint64_t tx_arp = 0;
    uint64_t rx_arp = 0;
    uint64_t tx_ipv4 = 0;
    uint64_t rx_ipv4 = 0;
    uint64_t tx_ipv6 = 0;
    uint64_t rx_ipv6 = 0;
    uint64_t tx_icmp = 0;
    uint64_t rx_icmp = 0;
    uint64_t tx_udp = 0;
    uint64_t rx_udp = 0;
    uint64_t tx_tcp = 0;
    uint64_t rx_tcp = 0;
    uint64_t devices = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_NETDEV_INFO, (long)i, (long)&netdev, 0) < 0) {
            continue;
        }
        if (syscall3(AOS_SYS_NETDEV_STATS, (long)i, (long)&stats, 0) < 0) {
            continue;
        }
        devices++;
        tx_arp += stats.tx_arp;
        rx_arp += stats.rx_arp;
        tx_ipv4 += stats.tx_ipv4;
        rx_ipv4 += stats.rx_ipv4;
        tx_ipv6 += stats.tx_ipv6;
        rx_ipv6 += stats.rx_ipv6;
        tx_icmp += stats.tx_icmp;
        rx_icmp += stats.rx_icmp;
        tx_udp += stats.tx_udp;
        rx_udp += stats.rx_udp;
        tx_tcp += stats.tx_tcp;
        rx_tcp += stats.rx_tcp;
    }

    write_cstr("Protocol statistics:\n");
    write_cstr("  devices=");
    write_u64(devices);
    write_cstr("\n");
    write_cstr("  arp   rx=");
    write_u64(rx_arp);
    write_cstr(" tx=");
    write_u64(tx_arp);
    write_cstr("\n");
    write_cstr("  ipv4  rx=");
    write_u64(rx_ipv4);
    write_cstr(" tx=");
    write_u64(tx_ipv4);
    write_cstr("\n");
    write_cstr("  ipv6  rx=");
    write_u64(rx_ipv6);
    write_cstr(" tx=");
    write_u64(tx_ipv6);
    write_cstr("\n");
    write_cstr("  icmp  rx=");
    write_u64(rx_icmp);
    write_cstr(" tx=");
    write_u64(tx_icmp);
    write_cstr("\n");
    write_cstr("  udp   rx=");
    write_u64(rx_udp);
    write_cstr(" tx=");
    write_u64(tx_udp);
    write_cstr("\n");
    write_cstr("  tcp   rx=");
    write_u64(rx_tcp);
    write_cstr(" tx=");
    write_u64(tx_tcp);
    write_cstr("\n");
}

static void show_sockets(void) {
    int shown = 0;

    write_cstr("Active sockets:\n");
    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_SOCKET_INFO, (long)i, (long)&socket_info, 0) < 0) {
            continue;
        }
        shown = 1;
        write_cstr("  #");
        write_u64(socket_info.index);
        write_cstr(" ");
        write_cstr(socket_state(socket_info.state));
        write_cstr(" dev=");
        write_cstr(socket_info.dev_name[0] ? socket_info.dev_name : "?");
        write_cstr(" local=:");
        write_u64(socket_info.local_port);
        write_cstr(" remote=");
        if (socket_info.family == AF_INET6) {
            write_cstr("[");
            if (ipv6_is_zero(socket_info.remote_ip6)) {
                write_cstr("::");
            } else {
                write_ipv6(socket_info.remote_ip6);
            }
            write_cstr("]");
        } else if (ipv4_is_zero(socket_info.remote_ip)) {
            write_cstr("0.0.0.0");
        } else {
            write_ipv4(socket_info.remote_ip);
        }
        write_cstr(":");
        write_u64(socket_info.remote_port);
        write_cstr(" rx=");
        write_u64(socket_info.rx_len - socket_info.rx_off);
        write_cstr(" win=");
        write_u64(socket_info.peer_window);
        write_cstr(" winbytes=");
        write_u64(socket_info.peer_window_bytes);
        write_cstr(" mss=");
        write_u64(socket_info.peer_mss);
        write_cstr(" rexmit=");
        write_u64(socket_info.retransmits);
        write_cstr(" cwnd=");
        write_u64(socket_info.cwnd_bytes);
        write_cstr(" ssthresh=");
        write_u64(socket_info.ssthresh_bytes);
        write_cstr(" wscale=");
        write_u64(socket_info.local_window_scale);
        write_cstr("/");
        write_u64(socket_info.peer_window_scale);
        write_cstr(" refs=");
        write_u64(socket_info.refcount);
        write_cstr("\n");
    }
    if (!shown) {
        write_cstr("  none\n");
    }
}

static void show_cache_stats(void) {
    uint64_t dns_count = 0;
    uint64_t dns_hits = 0;
    uint64_t arp_count = 0;
    uint64_t arp_hits = 0;
    uint64_t ndp_count = 0;
    uint64_t ndp_hits = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_DNS_CACHE_INFO, (long)i, (long)&dns_entry, 0) == 0) {
            dns_count++;
            dns_hits += dns_entry.hits;
        }
        if (syscall3(AOS_SYS_ARP_CACHE_INFO, (long)i, (long)&arp_entry, 0) == 0) {
            arp_count++;
            arp_hits += arp_entry.hits;
        }
        if (syscall3(AOS_SYS_NDP_CACHE_INFO, (long)i, (long)&ndp_entry, 0) == 0) {
            ndp_count++;
            ndp_hits += ndp_entry.hits;
        }
    }

    write_cstr("Neighbor and resolver cache:\n");
    write_cstr("  dns entries=");
    write_u64(dns_count);
    write_cstr(" hits=");
    write_u64(dns_hits);
    write_cstr("\n");
    write_cstr("  arp entries=");
    write_u64(arp_count);
    write_cstr(" hits=");
    write_u64(arp_hits);
    write_cstr("\n");
    write_cstr("  ndp entries=");
    write_u64(ndp_count);
    write_cstr(" hits=");
    write_u64(ndp_hits);
    write_cstr("\n");
}

static void usage(void) {
    write_cstr("usage: netstat [-i|-a|-c|-r|-s]\n");
    write_cstr("  no args  show interface counters, routes, cache stats, protocol stats, and sockets\n");
    write_cstr("  -i       show interface counters only\n");
    write_cstr("  -a       show active sockets only\n");
    write_cstr("  -c       show DNS/ARP/NDP cache stats only\n");
    write_cstr("  -r       show routes only\n");
    write_cstr("  -s       show protocol totals only\n");
}

void aos_main(uint64_t argc, char** argv) {
    int show_ifaces = 1;
    int show_route_list = 1;
    int show_caches = 1;
    int show_protocols = 1;
    int show_socket_list = 1;

    if (argc > 2) {
        usage();
        exit_code(1);
    }
    if (argc == 2) {
        if (cstr_eq(argv[1], "-i") || cstr_eq(argv[1], "--interfaces")) {
            show_route_list = 0;
            show_caches = 0;
            show_protocols = 0;
            show_socket_list = 0;
        } else if (cstr_eq(argv[1], "-a") || cstr_eq(argv[1], "--sockets")) {
            show_ifaces = 0;
            show_route_list = 0;
            show_caches = 0;
            show_protocols = 0;
        } else if (cstr_eq(argv[1], "-c") || cstr_eq(argv[1], "--caches")) {
            show_ifaces = 0;
            show_route_list = 0;
            show_protocols = 0;
            show_socket_list = 0;
        } else if (cstr_eq(argv[1], "-r") || cstr_eq(argv[1], "--routes")) {
            show_ifaces = 0;
            show_caches = 0;
            show_protocols = 0;
            show_socket_list = 0;
        } else if (cstr_eq(argv[1], "-s") || cstr_eq(argv[1], "--statistics")) {
            show_ifaces = 0;
            show_route_list = 0;
            show_caches = 0;
            show_socket_list = 0;
        } else if (cstr_eq(argv[1], "-h") || cstr_eq(argv[1], "--help")) {
            usage();
            exit_code(0);
        } else {
            usage();
            exit_code(1);
        }
    }

    write_cstr("AOS network statistics\n");
    if (show_ifaces) show_interfaces();
    if (show_route_list) show_routes();
    if (show_caches) show_cache_stats();
    if (show_protocols) show_protocol_stats();
    if (show_socket_list) show_sockets();
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
