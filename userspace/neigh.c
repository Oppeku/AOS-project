/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_NETDEV_SEND 527
#define AOS_SYS_NETDEV_RECV 528
#define AOS_SYS_ARP_CACHE_INFO 539
#define AOS_SYS_NET_CACHE_FLUSH 541

#define AOS_NET_CACHE_ARP 1
#define AOS_NET_CACHE_ALL_DEVICES 255

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
static struct aos_arp_cache_info_user cache_entry;
static uint8_t tx_frame[TX_SIZE];
static uint8_t rx_frame[RX_SIZE];
static uint8_t resolved_mac[6];
static char num_buf[21];
static uint64_t active_netdev_index;

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

static void write_hex16(uint16_t v) {
    write_hex8((uint8_t)(v >> 8));
    write_hex8((uint8_t)v);
}

static void write_mac(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        if (i) write_cstr(":");
        write_hex8(mac[i]);
    }
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
    uint8_t pseudo[8];
    uint32_t sum = 0;

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

static void build_arp_request(const uint8_t target_ip[4]) {
    uint8_t* eth = tx_frame;
    uint8_t* arp = tx_frame + 14;

    memzero(tx_frame, 60);
    for (int i = 0; i < 6; i++) eth[i] = 0xff;
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x08;
    eth[13] = 0x06;
    arp[0] = 0x00;
    arp[1] = 0x01;
    arp[2] = 0x08;
    arp[3] = 0x00;
    arp[4] = 0x06;
    arp[5] = 0x04;
    arp[6] = 0x00;
    arp[7] = 0x01;
    memcopy(arp + 8, netdev.mac, 6);
    memcopy(arp + 14, netdev.ipv4_addr, 4);
    memcopy(arp + 24, target_ip, 4);
}

static int parse_arp_reply(long len, const uint8_t target_ip[4]) {
    uint8_t* eth = rx_frame;
    uint8_t* arp = rx_frame + 14;

    if (len < 42) return 0;
    if (eth[12] != 0x08 || eth[13] != 0x06) return 0;
    if (arp[6] != 0x00 || arp[7] != 0x02) return 0;
    if (!memeq(arp + 14, target_ip, 4)) return 0;
    if (!memeq(arp + 24, netdev.ipv4_addr, 4)) return 0;
    memcopy(resolved_mac, arp + 8, 6);
    return 1;
}

static int resolve_arp(const uint8_t target_ip[4]) {
    build_arp_request(target_ip);
    if (syscall3(AOS_SYS_NETDEV_SEND, (long)active_netdev_index, (long)tx_frame, 60) < 0) return -1;

    for (int i = 0; i < 800000; i++) {
        long n = syscall3(AOS_SYS_NETDEV_RECV, (long)active_netdev_index, (long)rx_frame, RX_SIZE);
        if (n < 0) return -1;
        if (parse_arp_reply(n, target_ip)) return 0;
    }
    return -1;
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

static void build_neighbor_solicit(const uint8_t target_ip[16]) {
    uint8_t multicast_ip[16];
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* icmp = tx_frame + 54;

    memzero(tx_frame, TX_SIZE);
    solicited_node_multicast(target_ip, multicast_ip);
    eth[0] = 0x33;
    eth[1] = 0x33;
    eth[2] = 0xff;
    eth[3] = target_ip[13];
    eth[4] = target_ip[14];
    eth[5] = target_ip[15];
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x86;
    eth[13] = 0xdd;

    build_ipv6_header(ip, multicast_ip, 32, 255);
    icmp[0] = 135;
    memcopy(icmp + 8, target_ip, 16);
    icmp[24] = 1;
    icmp[25] = 1;
    memcopy(icmp + 26, netdev.mac, 6);
    uint16_t sum = icmp6_checksum(netdev.ipv6_addr, multicast_ip, icmp, 32);
    icmp[2] = (uint8_t)(sum >> 8);
    icmp[3] = (uint8_t)sum;
}

static int parse_neighbor_advert(long len, const uint8_t target_ip[16]) {
    uint8_t* eth = rx_frame;
    uint8_t* ip = rx_frame + 14;
    uint8_t* icmp = rx_frame + 54;

    if (len < 86) return 0;
    if (eth[12] != 0x86 || eth[13] != 0xdd) return 0;
    if (ip[6] != 58 || icmp[0] != 136) return 0;
    if (!memeq(ip + 8, target_ip, 16)) return 0;
    if (!memeq(icmp + 8, target_ip, 16)) return 0;
    memcopy(resolved_mac, eth + 6, 6);
    if (icmp[24] == 2 && icmp[25] == 1) {
        memcopy(resolved_mac, icmp + 26, 6);
    }
    return 1;
}

static int resolve_ndp(const uint8_t target_ip[16]) {
    build_neighbor_solicit(target_ip);
    if (syscall3(AOS_SYS_NETDEV_SEND, (long)active_netdev_index, (long)tx_frame, 86) < 0) return -1;

    for (int i = 0; i < 350000; i++) {
        long n = syscall3(AOS_SYS_NETDEV_RECV, (long)active_netdev_index, (long)rx_frame, RX_SIZE);
        if (n < 0) return -1;
        if (parse_neighbor_advert(n, target_ip)) return 0;
    }
    return -1;
}

static void print_incomplete_prefix(void) {
    write_cstr(" dev ");
    write_cstr(netdev.name);
    write_cstr(" lladdr <incomplete> FAILED\n");
}

static void print_reachable_suffix(void) {
    write_cstr(" dev ");
    write_cstr(netdev.name);
    write_cstr(" lladdr ");
    write_mac(resolved_mac);
    write_cstr(" REACHABLE\n");
}

static void inspect_netdev(uint64_t index) {
    if (syscall3(AOS_SYS_NETDEV_INFO, (long)index, (long)&netdev, 0) < 0) return;
    if (!netdev.link_up) return;

    active_netdev_index = index;

    if (netdev.ipv4_configured && !ipv4_is_zero(netdev.ipv4_gateway)) {
        write_ipv4(netdev.ipv4_gateway);
        if (resolve_arp(netdev.ipv4_gateway) == 0) {
            print_reachable_suffix();
        } else {
            print_incomplete_prefix();
        }
    }

    if (netdev.ipv6_configured && !ipv6_is_zero(netdev.ipv6_gateway)) {
        write_ipv6(netdev.ipv6_gateway);
        if (resolve_ndp(netdev.ipv6_gateway) == 0) {
            print_reachable_suffix();
        } else {
            print_incomplete_prefix();
        }
    }
}

static void print_cache_entry(void) {
    write_ipv4(cache_entry.ipv4);
    write_cstr(" dev ");
    if (cache_entry.dev_name[0]) {
        write_cstr(cache_entry.dev_name);
    } else {
        write_cstr("net");
        write_u64(cache_entry.dev_index);
    }
    write_cstr(" lladdr ");
    write_mac(cache_entry.mac);
    write_cstr(" REACHABLE hits=");
    write_u64(cache_entry.hits);
    write_cstr(" ttl_ticks=");
    write_u64(cache_entry.ttl_ticks);
    write_cstr(" cache\n");
}

static int print_kernel_cache(void) {
    int shown = 0;

    for (uint64_t i = 0; i < 8; i++) {
        if (syscall3(AOS_SYS_ARP_CACHE_INFO, (long)i, (long)&cache_entry, 0) == 0) {
            print_cache_entry();
            shown = 1;
        }
    }

    return shown;
}

void aos_main(uint64_t argc, char** argv) {
    if (argc >= 2 && cstr_eq(argv[1], "flush")) {
        long rc = syscall3(AOS_SYS_NET_CACHE_FLUSH,
                           AOS_NET_CACHE_ARP,
                           AOS_NET_CACHE_ALL_DEVICES,
                           0);
        if (rc < 0) {
            write_cstr("neigh: ARP cache flush failed\n");
            exit_code(1);
        }
        write_cstr("neigh: flushed ");
        write_u64((uint64_t)rc);
        write_cstr(" ARP cache entries\n");
        exit_code(0);
    }

    (void)print_kernel_cache();
    for (uint64_t i = 0; i < 8; i++) {
        inspect_netdev(i);
    }
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
