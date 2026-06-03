/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_NETDEV_SEND 527
#define AOS_SYS_NETDEV_RECV 528
#define AOS_SYS_NETDEV_IPV4_CONFIG 532

#define TX_SIZE 1518
#define RX_SIZE 1518
#define DHCP_XID 0xa0502026U
#define DHCP_OPT_MSG_TYPE 53
#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_ROUTER 3
#define DHCP_OPT_DNS 6
#define DHCP_OPT_SERVER_ID 54
#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_ACK 5

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
static uint8_t offered_ip[4];
static uint8_t server_ip[4];
static uint8_t router_ip[4];
static uint8_t dns_ip[4];
static uint8_t mask_ip[4];
static uint8_t prefix_len;
static uint64_t iface_index;
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

static void write_cstr(const char* s) {
    syscall5(SYS_WRITE, 1, (long)s, (long)cstrlen(s), 0, 0);
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

static int memeq(const uint8_t* a, const uint8_t* b, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
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

static int streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == 0 && *b == 0;
}

static int parse_iface_index(const char* s, uint64_t* out) {
    if (!s || !out) return -1;
    if (s[0] < '0' || s[0] > '7' || s[1] != 0) return -1;
    *out = (uint64_t)(s[0] - '0');
    return 0;
}

static void usage(void) {
    write_cstr("usage: dhcp [-i IFACE]\n");
    write_cstr("examples: dhcp | dhcp -i 1\n");
    exit_code(1);
}

static uint16_t checksum_finish(uint32_t sum) {
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

static uint16_t ipv4_checksum(const uint8_t* data, uint32_t len) {
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    if (len) sum += ((uint16_t)data[0] << 8);
    return checksum_finish(sum);
}

static uint8_t prefix_from_mask(const uint8_t mask[4]) {
    uint8_t prefix = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t v = mask[i];
        for (int bit = 7; bit >= 0; bit--) {
            if (v & (uint8_t)(1U << bit)) {
                prefix++;
            } else {
                return prefix;
            }
        }
    }
    return prefix;
}

static void put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t get_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint16_t build_dhcp_packet(uint8_t msg_type) {
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* udp = tx_frame + 34;
    uint8_t* bootp = tx_frame + 42;
    uint8_t* opt;
    uint16_t bootp_len;
    uint16_t udp_len;
    uint16_t ip_len;

    memzero(tx_frame, TX_SIZE);
    for (int i = 0; i < 6; i++) eth[i] = 0xff;
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x08;
    eth[13] = 0x00;

    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = 17;
    if (msg_type == DHCP_REQUEST) {
        memcopy(ip + 12, netdev.ipv4_addr, 4);
    }
    ip[16] = 255;
    ip[17] = 255;
    ip[18] = 255;
    ip[19] = 255;

    put_be16(udp + 0, 68);
    put_be16(udp + 2, 67);

    bootp[0] = 1;
    bootp[1] = 1;
    bootp[2] = 6;
    put_be32(bootp + 4, DHCP_XID);
    put_be16(bootp + 10, 0x8000);
    memcopy(bootp + 28, netdev.mac, 6);
    bootp[236] = 99;
    bootp[237] = 130;
    bootp[238] = 83;
    bootp[239] = 99;

    opt = bootp + 240;
    *opt++ = DHCP_OPT_MSG_TYPE;
    *opt++ = 1;
    *opt++ = msg_type;
    if (msg_type == DHCP_REQUEST) {
        *opt++ = 50;
        *opt++ = 4;
        memcopy(opt, offered_ip, 4);
        opt += 4;
        *opt++ = DHCP_OPT_SERVER_ID;
        *opt++ = 4;
        memcopy(opt, server_ip, 4);
        opt += 4;
    }
    *opt++ = 55;
    *opt++ = 3;
    *opt++ = DHCP_OPT_SUBNET_MASK;
    *opt++ = DHCP_OPT_ROUTER;
    *opt++ = DHCP_OPT_DNS;
    *opt++ = 255;

    bootp_len = (uint16_t)(opt - bootp);
    udp_len = (uint16_t)(8 + bootp_len);
    ip_len = (uint16_t)(20 + udp_len);
    put_be16(ip + 2, ip_len);
    put_be16(udp + 4, udp_len);
    put_be16(ip + 10, ipv4_checksum(ip, 20));
    return (uint16_t)(14 + ip_len);
}

static int parse_dhcp_options(const uint8_t* opt, uint16_t len, uint8_t wanted_type) {
    uint8_t seen_type = 0;
    uint64_t i = 0;

    while (i < len) {
        uint8_t code = opt[i++];
        uint8_t olen;
        if (code == 0) continue;
        if (code == 255) break;
        if (i >= len) break;
        olen = opt[i++];
        if (i + olen > len) break;

        if (code == DHCP_OPT_MSG_TYPE && olen >= 1) {
            seen_type = opt[i];
        } else if (code == DHCP_OPT_SERVER_ID && olen >= 4) {
            memcopy(server_ip, opt + i, 4);
        } else if (code == DHCP_OPT_ROUTER && olen >= 4) {
            memcopy(router_ip, opt + i, 4);
        } else if (code == DHCP_OPT_DNS && olen >= 4) {
            memcopy(dns_ip, opt + i, 4);
        } else if (code == DHCP_OPT_SUBNET_MASK && olen >= 4) {
            memcopy(mask_ip, opt + i, 4);
        }
        i += olen;
    }

    return seen_type == wanted_type ? 0 : -1;
}

static int parse_dhcp_reply(long len, uint8_t wanted_type) {
    uint8_t* eth = rx_frame;
    uint8_t* ip = rx_frame + 14;
    uint8_t* udp;
    uint8_t* bootp;
    uint16_t ip_header_len;
    uint16_t udp_len;
    uint16_t bootp_len;

    if (len < 14 + 20 + 8 + 240) return -1;
    if (eth[12] != 0x08 || eth[13] != 0x00) return -1;
    if ((ip[0] >> 4) != 4 || ip[9] != 17) return -1;
    ip_header_len = (uint16_t)((ip[0] & 0x0f) * 4);
    if (ip_header_len < 20 || len < 14 + ip_header_len + 8 + 240) return -1;
    udp = ip + ip_header_len;
    if (get_be16(udp + 0) != 67 || get_be16(udp + 2) != 68) return -1;
    udp_len = get_be16(udp + 4);
    if (udp_len < 248) return -1;
    bootp = udp + 8;
    if (bootp[0] != 2 || get_be32(bootp + 4) != DHCP_XID) return -1;
    if (!memeq(bootp + 28, netdev.mac, 6)) return -1;
    if (bootp[236] != 99 || bootp[237] != 130 || bootp[238] != 83 || bootp[239] != 99) return -1;

    memcopy(offered_ip, bootp + 16, 4);
    bootp_len = (uint16_t)(udp_len - 8);
    return parse_dhcp_options(bootp + 240, (uint16_t)(bootp_len - 240), wanted_type);
}

static int wait_for_dhcp(uint8_t msg_type) {
    for (int i = 0; i < 1000000; i++) {
        long n = syscall5(AOS_SYS_NETDEV_RECV, (long)iface_index, (long)rx_frame, RX_SIZE, 0, 0);
        if (n < 0) return -1;
        if (parse_dhcp_reply(n, msg_type) == 0) return 0;
    }
    return -1;
}

static int send_dhcp(uint8_t msg_type) {
    uint16_t len = build_dhcp_packet(msg_type);
    return syscall5(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame, len, 0, 0) < 0 ? -1 : 0;
}

void aos_main(uint64_t argc, char** argv) {
    iface_index = 0;
    if (argc == 3) {
        if (!streq(argv[1], "-i") || parse_iface_index(argv[2], &iface_index) != 0) {
            usage();
        }
    } else if (argc != 1) {
        usage();
    }

    if (syscall5(AOS_SYS_NETDEV_INFO, (long)iface_index, (long)&netdev, 0, 0, 0) < 0 || !netdev.link_up) {
        write_cstr("dhcp: no active network interface\n");
        exit_code(1);
    }

    write_cstr("dhcp: discover on ");
    write_cstr(netdev.name);
    write_cstr("\n");
    if (send_dhcp(DHCP_DISCOVER) != 0 || wait_for_dhcp(DHCP_OFFER) != 0) {
        write_cstr("dhcp: no offer received\n");
        exit_code(1);
    }

    write_cstr("dhcp: offer ");
    write_ipv4(offered_ip);
    write_cstr(" from ");
    write_ipv4(server_ip);
    write_cstr("\n");

    write_cstr("dhcp: request\n");
    if (send_dhcp(DHCP_REQUEST) != 0 || wait_for_dhcp(DHCP_ACK) != 0) {
        write_cstr("dhcp: no ack received\n");
        exit_code(1);
    }

    prefix_len = prefix_from_mask(mask_ip);
    if (syscall5(AOS_SYS_NETDEV_IPV4_CONFIG,
                 (long)iface_index,
                 (long)offered_ip,
                 prefix_len,
                 (long)router_ip,
                 (long)dns_ip) < 0) {
        write_cstr("dhcp: configure failed\n");
        exit_code(1);
    }

    write_cstr("dhcp: bound ");
    write_ipv4(offered_ip);
    write_cstr("/");
    write_u64(prefix_len);
    write_cstr(" gateway ");
    write_ipv4(router_ip);
    write_cstr(" dns ");
    write_ipv4(dns_ip);
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
