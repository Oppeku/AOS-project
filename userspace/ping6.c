/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_NETDEV_SEND 527
#define AOS_SYS_NETDEV_RECV 528

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
static uint8_t target_ip[16];
static uint8_t neighbor_ip[16];
static uint8_t target_mac[6];
static char target_text[96];

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

static void write_buf(const char* s, uint64_t len) {
    syscall3(SYS_WRITE, 1, (long)s, (long)len);
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
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

static int memeq(const uint8_t* a, const uint8_t* b, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static void copy_target_text(const char* src) {
    uint64_t i = 0;
    while (src && src[i] && i + 1 < sizeof(target_text)) {
        target_text[i] = src[i];
        i++;
    }
    target_text[i] = 0;
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
    if (!s || !s[0]) return -1;

    if (p[0] == ':' && p[1] == ':') {
        compress = 0;
        p += 2;
    }

    while (*p) {
        uint32_t value = 0;
        int digits = 0;

        if (count >= 8) return -1;
        while (*p && *p != ':') {
            int hv = hex_value(*p);
            if (hv < 0 || digits >= 4) return -1;
            value = (value << 4) | (uint32_t)hv;
            digits++;
            p++;
        }
        if (digits == 0) return -1;
        groups[count++] = (uint16_t)value;

        if (*p == ':') {
            p++;
            if (*p == ':') {
                if (compress >= 0) return -1;
                compress = count;
                p++;
                if (!*p) break;
            } else if (!*p) {
                return -1;
            }
        }
    }

    if (compress >= 0) {
        int missing = 8 - count;
        if (missing < 0) return -1;
        for (int i = count - 1; i >= compress; i--) {
            groups[i + missing] = groups[i];
        }
        for (int i = 0; i < missing; i++) {
            groups[compress + i] = 0;
        }
    } else if (count != 8) {
        return -1;
    }

    for (int i = 0; i < 8; i++) {
        out[i * 2] = (uint8_t)(groups[i] >> 8);
        out[i * 2 + 1] = (uint8_t)groups[i];
    }
    return 0;
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

static int ipv6_is_zero(const uint8_t ip[16]) {
    for (int i = 0; i < 16; i++) {
        if (ip[i] != 0) return 0;
    }
    return 1;
}

static int ipv6_is_link_local(const uint8_t ip[16]) {
    return ip[0] == 0xfe && (ip[1] & 0xc0U) == 0x80U;
}

static int ipv6_same_prefix(const uint8_t a[16], const uint8_t b[16], uint8_t prefix) {
    uint8_t full = prefix / 8U;
    uint8_t rem = prefix % 8U;

    for (uint8_t i = 0; i < full; i++) {
        if (a[i] != b[i]) return 0;
    }
    if (rem != 0) {
        uint8_t mask = (uint8_t)(0xffU << (8U - rem));
        if ((a[full] & mask) != (b[full] & mask)) return 0;
    }
    return 1;
}

static void write_mac(const uint8_t mac[6]) {
    static const char h[] = "0123456789abcdef";
    char b[3];
    b[2] = 0;
    for (int i = 0; i < 6; i++) {
        if (i) write_cstr(":");
        b[0] = h[(mac[i] >> 4) & 0xf];
        b[1] = h[mac[i] & 0xf];
        write_cstr(b);
    }
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

static void build_ipv6_header(uint8_t* ip,
                              const uint8_t dst[16],
                              uint16_t payload_len,
                              uint8_t hop_limit) {
    memzero(ip, 40);
    ip[0] = 0x60;
    ip[4] = (uint8_t)(payload_len >> 8);
    ip[5] = (uint8_t)payload_len;
    ip[6] = 58;
    ip[7] = hop_limit;
    memcopy(ip + 8, netdev.ipv6_addr, 16);
    memcopy(ip + 24, dst, 16);
}

static void solicited_node_multicast(const uint8_t target[16], uint8_t out[16]) {
    memzero(out, 16);
    out[0] = 0xff;
    out[1] = 0x02;
    out[11] = 0x01;
    out[12] = 0xff;
    out[13] = target[13];
    out[14] = target[14];
    out[15] = target[15];
}

static void build_neighbor_solicit(void) {
    uint8_t multicast_ip[16];
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* icmp = tx_frame + 54;

    solicited_node_multicast(neighbor_ip, multicast_ip);
    eth[0] = 0x33;
    eth[1] = 0x33;
    eth[2] = 0xff;
    eth[3] = neighbor_ip[13];
    eth[4] = neighbor_ip[14];
    eth[5] = neighbor_ip[15];
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x86;
    eth[13] = 0xdd;

    build_ipv6_header(ip, multicast_ip, 32, 255);
    memzero(icmp, 32);
    icmp[0] = 135;
    memcopy(icmp + 8, neighbor_ip, 16);
    icmp[24] = 1;
    icmp[25] = 1;
    memcopy(icmp + 26, netdev.mac, 6);
    uint16_t sum = icmp6_checksum(netdev.ipv6_addr, multicast_ip, icmp, 32);
    icmp[2] = (uint8_t)(sum >> 8);
    icmp[3] = (uint8_t)sum;
}

static int parse_neighbor_advert(long len) {
    uint8_t* eth = rx_frame;
    uint8_t* ip = rx_frame + 14;
    uint8_t* icmp = rx_frame + 54;

    if (len < 86) return 0;
    if (eth[12] != 0x86 || eth[13] != 0xdd) return 0;
    if (ip[6] != 58 || icmp[0] != 136) return 0;
    if (!memeq(ip + 8, neighbor_ip, 16)) return 0;
    if (!memeq(icmp + 8, neighbor_ip, 16)) return 0;
    memcopy(target_mac, eth + 6, 6);
    if (icmp[24] == 2 && icmp[25] == 1) {
        memcopy(target_mac, icmp + 26, 6);
    }
    return 1;
}

static int resolve_neighbor(void) {
    build_neighbor_solicit();
    if (syscall3(AOS_SYS_NETDEV_SEND, 0, (long)tx_frame, 86) < 0) return -1;

    write_cstr("ndp: who-has ");
    write_ipv6(neighbor_ip);
    write_cstr("\n");

    for (int i = 0; i < 350000; i++) {
        long n = syscall3(AOS_SYS_NETDEV_RECV, 0, (long)rx_frame, RX_SIZE);
        if (n < 0) return -1;
        if (parse_neighbor_advert(n)) {
            write_cstr("ndp: reply at ");
            write_mac(target_mac);
            write_cstr("\n");
            return 0;
        }
    }
    return -1;
}

static void build_echo_request(void) {
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* icmp = tx_frame + 54;
    const char payload[] = "AOSPING6";
    uint16_t icmp_len = 16;

    memcopy(eth, target_mac, 6);
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x86;
    eth[13] = 0xdd;
    build_ipv6_header(ip, target_ip, icmp_len, 64);
    memzero(icmp, icmp_len);
    icmp[0] = 128;
    icmp[4] = 0xa0;
    icmp[5] = 0x06;
    icmp[7] = 1;
    memcopy(icmp + 8, payload, 8);
    uint16_t sum = icmp6_checksum(netdev.ipv6_addr, target_ip, icmp, icmp_len);
    icmp[2] = (uint8_t)(sum >> 8);
    icmp[3] = (uint8_t)sum;
}

static int is_echo_reply(long len) {
    uint8_t* eth = rx_frame;
    uint8_t* ip = rx_frame + 14;
    uint8_t* icmp = rx_frame + 54;

    if (len < 70) return 0;
    if (eth[12] != 0x86 || eth[13] != 0xdd) return 0;
    if (ip[6] != 58) return 0;
    if (!memeq(ip + 8, target_ip, 16)) return 0;
    if (!memeq(ip + 24, netdev.ipv6_addr, 16)) return 0;
    if (icmp[0] != 129 || icmp[1] != 0) return 0;
    if (icmp[4] != 0xa0 || icmp[5] != 0x06 || icmp[6] != 0 || icmp[7] != 1) return 0;
    return 1;
}

static void print_header(void) {
    write_cstr("PING6 ");
    write_cstr(target_text);
    write_cstr(" via ");
    write_cstr(netdev.name);
    write_cstr(" (");
    write_cstr(netdev.driver);
    write_cstr(")\n");
}

static void print_reply(void) {
    write_cstr("64 bytes from ");
    write_ipv6(target_ip);
    write_cstr(": icmp_seq=1 hop_limit=64\n");
}

static int choose_neighbor(void) {
    if (ipv6_is_link_local(target_ip) ||
        ipv6_same_prefix(target_ip, netdev.ipv6_addr, netdev.ipv6_prefix)) {
        memcopy(neighbor_ip, target_ip, 16);
        return 0;
    }

    if (ipv6_is_zero(netdev.ipv6_gateway)) {
        write_cstr("ping6: destination is off-link and no gateway6 is configured\n");
        return -1;
    }

    memcopy(neighbor_ip, netdev.ipv6_gateway, 16);
    write_cstr("route: gateway6 ");
    write_ipv6(neighbor_ip);
    write_cstr("\n");
    return 0;
}

void aos_main(uint64_t argc, char** argv) {
    const char* target;

    if (argc < 2) {
        write_cstr("usage: ping6 ADDRESS\nexamples: ping6 self | ping6 fe80::2\n");
        exit_code(1);
    }

    target = argv[1];
    if (syscall3(AOS_SYS_NETDEV_INFO, 0, (long)&netdev, 0) < 0) {
        write_cstr("ping6: no network interface is registered\n");
        exit_code(1);
    }
    if (!netdev.link_up) {
        write_cstr("ping6: network link is down\n");
        exit_code(1);
    }
    if (!netdev.ipv6_configured) {
        write_cstr("ping6: interface has no IPv6 address\n");
        exit_code(1);
    }

    if (streq(target, "self")) {
        memcopy(target_ip, netdev.ipv6_addr, 16);
        copy_target_text("self");
    } else {
        if (parse_ipv6(target, target_ip) != 0) {
            write_cstr("ping6: bad IPv6 address\n");
            exit_code(1);
        }
        copy_target_text(target);
    }

    print_header();
    if (memeq(target_ip, netdev.ipv6_addr, 16)) {
        print_reply();
        exit_code(0);
    }

    if (choose_neighbor() != 0) {
        exit_code(1);
    }
    if (resolve_neighbor() != 0) {
        write_cstr("ping6: neighbor discovery failed\n");
        exit_code(1);
    }
    build_echo_request();
    if (syscall3(AOS_SYS_NETDEV_SEND, 0, (long)tx_frame, 70) < 0) {
        write_cstr("ping6: ICMPv6 send failed\n");
        exit_code(1);
    }
    for (int i = 0; i < 500000; i++) {
        long n = syscall3(AOS_SYS_NETDEV_RECV, 0, (long)rx_frame, RX_SIZE);
        if (n < 0) {
            write_cstr("ping6: network receive failed\n");
            exit_code(1);
        }
        if (is_echo_reply(n)) {
            print_reply();
            exit_code(0);
        }
    }
    write_cstr("ping6: timeout waiting for ICMPv6 echo reply\n");
    exit_code(1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
