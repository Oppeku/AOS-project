/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_SOCKET_INFO 533
#define AOS_SYS_NETDEV_STATS 535

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
    uint8_t remote_ip[4];
    uint8_t next_hop_ip[4];
    uint8_t remote_mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
} __attribute__((packed));

static struct aos_netdev_info_user netdev;
static struct aos_netdev_stats_user stats;
static struct aos_socket_info_user socket_info;

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
        }
    }
    if (!shown) {
        write_cstr("  none\n");
    }
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
        if (ipv4_is_zero(socket_info.remote_ip)) {
            write_cstr("0.0.0.0");
        } else {
            write_ipv4(socket_info.remote_ip);
        }
        write_cstr(":");
        write_u64(socket_info.remote_port);
        write_cstr(" rx=");
        write_u64(socket_info.rx_len - socket_info.rx_off);
        write_cstr(" refs=");
        write_u64(socket_info.refcount);
        write_cstr("\n");
    }
    if (!shown) {
        write_cstr("  none\n");
    }
}

static void usage(void) {
    write_cstr("usage: netstat [-i|-a]\n");
    write_cstr("  no args  show interface counters and sockets\n");
    write_cstr("  -i       show interface counters only\n");
    write_cstr("  -a       show active sockets only\n");
}

void aos_main(uint64_t argc, char** argv) {
    int show_ifaces = 1;
    int show_socket_list = 1;

    if (argc > 2) {
        usage();
        exit_code(1);
    }
    if (argc == 2) {
        if (cstr_eq(argv[1], "-i") || cstr_eq(argv[1], "--interfaces")) {
            show_socket_list = 0;
        } else if (cstr_eq(argv[1], "-a") || cstr_eq(argv[1], "--sockets")) {
            show_ifaces = 0;
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
