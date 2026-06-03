/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_NETDEV_SEND 527
#define AOS_SYS_NETDEV_RECV 528
#define AOS_SYS_NETDEV_IPV6_CONFIG 531

#define TX_SIZE 1518
#define RX_SIZE 1518

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
static uint8_t tx_frame[TX_SIZE];
static uint8_t rx_frame[RX_SIZE];
static uint8_t router_ip[16];
static uint8_t prefix_ip[16];
static uint8_t global_ip[16];
static char num_buf[21];

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
    syscall5(SYS_EXIT, code, 0, 0, 0, 0);
    for (;;) {}
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void write_buf(const char* s, uint64_t len) {
    syscall5(SYS_WRITE, 1, (long)s, (long)len, 0, 0);
}

static void write_cstr(const char* s) {
    write_buf(s, cstrlen(s));
}

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
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

static uint32_t checksum_add(uint32_t sum, const uint8_t* data, uint32_t len) {
    while (len > 1) {
        sum += ((uint16_t)data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    if (len) sum += ((uint16_t)data[0] << 8);
    return sum;
}

static uint16_t checksum_finish(uint32_t sum) {
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static uint16_t icmp6_checksum(const uint8_t src[16],
                               const uint8_t dst[16],
                               const uint8_t* icmp,
                               uint32_t len) {
    uint32_t sum = 0;
    uint8_t pseudo[8];

    sum = checksum_add(sum, src, 16);
    sum = checksum_add(sum, dst, 16);
    pseudo[0] = (uint8_t)(len >> 24);
    pseudo[1] = (uint8_t)(len >> 16);
    pseudo[2] = (uint8_t)(len >> 8);
    pseudo[3] = (uint8_t)len;
    pseudo[4] = 0;
    pseudo[5] = 0;
    pseudo[6] = 0;
    pseudo[7] = 58;
    sum = checksum_add(sum, pseudo, 8);
    sum = checksum_add(sum, icmp, len);
    return checksum_finish(sum);
}

static void build_router_solicit(void) {
    static const uint8_t all_routers[16] = {
        0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02
    };
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* icmp = tx_frame + 54;

    eth[0] = 0x33;
    eth[1] = 0x33;
    eth[2] = 0x00;
    eth[3] = 0x00;
    eth[4] = 0x00;
    eth[5] = 0x02;
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x86;
    eth[13] = 0xdd;

    memzero(ip, 40);
    ip[0] = 0x60;
    ip[5] = 16;
    ip[6] = 58;
    ip[7] = 255;
    memcopy(ip + 8, netdev.ipv6_addr, 16);
    memcopy(ip + 24, all_routers, 16);

    memzero(icmp, 16);
    icmp[0] = 133;
    icmp[8] = 1;
    icmp[9] = 1;
    memcopy(icmp + 10, netdev.mac, 6);
    uint16_t sum = icmp6_checksum(netdev.ipv6_addr, all_routers, icmp, 16);
    icmp[2] = (uint8_t)(sum >> 8);
    icmp[3] = (uint8_t)sum;
}

static void make_slaac_address(uint8_t prefix_len) {
    memzero(global_ip, 16);
    memcopy(global_ip, prefix_ip, 16);
    if (prefix_len <= 64) {
        for (int i = 8; i < 16; i++) {
            global_ip[i] = netdev.ipv6_addr[i];
        }
    }
}

static int parse_router_advert(long len, uint8_t* prefix_len_out) {
    uint8_t* eth = rx_frame;
    uint8_t* ip = rx_frame + 14;
    uint8_t* icmp = rx_frame + 54;
    uint32_t pos = 70;

    if (len < 70) return 0;
    if (eth[12] != 0x86 || eth[13] != 0xdd) return 0;
    if (ip[6] != 58 || ip[7] != 255) return 0;
    if (icmp[0] != 134 || icmp[1] != 0) return 0;

    memcopy(router_ip, ip + 8, 16);
    while (pos + 2 <= (uint32_t)len) {
        uint8_t type = rx_frame[pos];
        uint8_t opt_len = rx_frame[pos + 1];
        uint32_t bytes = (uint32_t)opt_len * 8U;

        if (opt_len == 0 || pos + bytes > (uint32_t)len) {
            break;
        }
        if (type == 3 && bytes >= 32) {
            uint8_t prefix_len = rx_frame[pos + 2];
            uint8_t flags = rx_frame[pos + 3];
            if ((flags & 0x40U) != 0 && prefix_len <= 64) {
                memcopy(prefix_ip, &rx_frame[pos + 16], 16);
                *prefix_len_out = prefix_len;
                make_slaac_address(prefix_len);
                return 1;
            }
        }
        pos += bytes;
    }
    return 0;
}

static long build_test_router_advert(void) {
    static const uint8_t router[16] = {
        0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02
    };
    static const uint8_t prefix[16] = {
        0x20, 0x01, 0x0d, 0xb8, 0x0a, 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint8_t* eth = rx_frame;
    uint8_t* ip = rx_frame + 14;
    uint8_t* icmp = rx_frame + 54;
    uint8_t* opt = rx_frame + 70;

    memzero(rx_frame, RX_SIZE);
    eth[0] = 0x33;
    eth[1] = 0x33;
    eth[2] = 0x00;
    eth[3] = 0x00;
    eth[4] = 0x00;
    eth[5] = 0x01;
    eth[6] = 0x52;
    eth[7] = 0x55;
    eth[8] = 0x0a;
    eth[9] = 0x00;
    eth[10] = 0x00;
    eth[11] = 0x02;
    eth[12] = 0x86;
    eth[13] = 0xdd;
    ip[0] = 0x60;
    ip[4] = 0x00;
    ip[5] = 0x30;
    ip[6] = 58;
    ip[7] = 255;
    memcopy(ip + 8, router, 16);
    memcopy(ip + 24, netdev.ipv6_addr, 16);
    icmp[0] = 134;
    icmp[1] = 0;
    icmp[4] = 64;
    icmp[5] = 0x08;
    opt[0] = 3;
    opt[1] = 4;
    opt[2] = 64;
    opt[3] = 0xc0;
    opt[4] = 0x00;
    opt[5] = 0x00;
    opt[6] = 0x0e;
    opt[7] = 0x10;
    opt[8] = 0x00;
    opt[9] = 0x00;
    opt[10] = 0x07;
    opt[11] = 0x08;
    memcopy(opt + 16, prefix, 16);
    return 102;
}

static void configure_from_parsed_ra(uint8_t prefix_len) {
    if (syscall5(AOS_SYS_NETDEV_IPV6_CONFIG,
                 0,
                 (long)global_ip,
                 prefix_len,
                 (long)router_ip,
                 0) < 0) {
        write_cstr("rdisc6: configure failed\n");
        exit_code(1);
    }
    write_cstr("router ");
    write_ipv6(router_ip);
    write_cstr("\n");
    write_cstr("prefix ");
    write_ipv6(prefix_ip);
    write_cstr("/");
    write_u64(prefix_len);
    write_cstr("\n");
    write_cstr("configured ");
    write_ipv6(global_ip);
    write_cstr("\n");
}

void aos_main(uint64_t argc, char** argv) {
    uint8_t prefix_len = 0;

    if (syscall5(AOS_SYS_NETDEV_INFO, 0, (long)&netdev, 0, 0, 0) < 0) {
        write_cstr("rdisc6: no network interface is registered\n");
        exit_code(1);
    }
    if (!netdev.link_up) {
        write_cstr("rdisc6: network link is down\n");
        exit_code(1);
    }
    if (!netdev.ipv6_configured) {
        write_cstr("rdisc6: interface has no IPv6 link-local address\n");
        exit_code(1);
    }

    if (argc >= 2 && streq(argv[1], "test")) {
        write_cstr("rdisc6: test router advertisement\n");
        if (!parse_router_advert(build_test_router_advert(), &prefix_len)) {
            write_cstr("rdisc6: test parse failed\n");
            exit_code(1);
        }
        configure_from_parsed_ra(prefix_len);
        exit_code(0);
    }

    build_router_solicit();
    write_cstr("rdisc6: sending router solicitation on ");
    write_cstr(netdev.name);
    write_cstr("\n");
    if (syscall5(AOS_SYS_NETDEV_SEND, 0, (long)tx_frame, 70, 0, 0) < 0) {
        write_cstr("rdisc6: send failed\n");
        exit_code(1);
    }

    for (int i = 0; i < 800000; i++) {
        long n = syscall5(AOS_SYS_NETDEV_RECV, 0, (long)rx_frame, RX_SIZE, 0, 0);
        if (n < 0) {
            write_cstr("rdisc6: receive failed\n");
            exit_code(1);
        }
        if (parse_router_advert(n, &prefix_len)) {
            configure_from_parsed_ra(prefix_len);
            exit_code(0);
        }
    }

    write_cstr("rdisc6: no router advertisement received\n");
    exit_code(1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
