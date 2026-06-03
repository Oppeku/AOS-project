/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_NETDEV_INFO 526
#define AOS_SYS_NETDEV_SEND 527
#define AOS_SYS_NETDEV_RECV 528
#define AOS_SYS_DNS_LOOKUP 534

#define TX_SIZE 1518
#define RX_SIZE 1518
#define TCP_SRC_PORT 40000
#define DNS_SRC_PORT 40001
#define TCP_SEQ 0xa0502026U
#define DNS_TXID 0xa056

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
static uint8_t target_ip[4];
static uint8_t arp_ip[4];
static uint8_t target_mac[6];
static char target_name[128];
static char dns_cname[128];
static char http_req[384];
static char num_buf[21];
static uint16_t target_port;
static uint64_t iface_index;
static const char* host_arg;
static const char* path_arg;
static const char* port_arg;

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

static void copy_name(const char* s) {
    uint64_t i = 0;
    while (s[i] && i < sizeof(target_name) - 1) {
        target_name[i] = s[i];
        i++;
    }
    target_name[i] = 0;
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

static uint16_t get_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t get_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t checksum_finish(uint32_t sum) {
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (uint16_t)~sum;
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

static uint16_t ipv4_checksum(const uint8_t* data, uint32_t len) {
    return checksum_finish(checksum_add(0, data, len));
}

static uint16_t tcp_checksum(const uint8_t* ip, const uint8_t* tcp, uint16_t tcp_len) {
    uint32_t sum = 0;
    uint8_t pseudo[12];
    memcopy(pseudo, ip + 12, 8);
    pseudo[8] = 0;
    pseudo[9] = 6;
    put_be16(pseudo + 10, tcp_len);
    sum = checksum_add(sum, pseudo, sizeof(pseudo));
    sum = checksum_add(sum, tcp, tcp_len);
    return checksum_finish(sum);
}

static int parse_u16(const char* s, uint16_t* out) {
    uint32_t v = 0;
    if (!s || !*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint32_t)(*s - '0');
        if (v > 65535) return 0;
        s++;
    }
    *out = (uint16_t)v;
    return 1;
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int is_dash_i(const char* s) {
    return s && s[0] == '-' && s[1] == 'i' && s[2] == 0;
}

static int parse_iface_index(const char* s, uint64_t* out) {
    if (!s || s[0] < '0' || s[0] > '7' || s[1] != 0) return 0;
    *out = (uint64_t)(s[0] - '0');
    return 1;
}

static int is_httpget_name(const char* s) {
    uint64_t i = 0;
    const char* base = s;
    while (s[i]) {
        if (s[i] == '/') base = s + i + 1;
        i++;
    }
    return streq(base, "httpget") || streq(base, "httpget.elf");
}

static int parse_ipv4(const char* s, uint8_t out[4]) {
    uint32_t v = 0;
    uint32_t part = 0;
    uint32_t digits = 0;
    while (*s) {
        if (*s == '.') {
            if (digits == 0 || part >= 3) return 0;
            out[part++] = (uint8_t)v;
            v = 0;
            digits = 0;
            s++;
            continue;
        }
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint32_t)(*s - '0');
        if (v > 255) return 0;
        digits++;
        s++;
    }
    if (digits == 0 || part != 3) return 0;
    out[part] = (uint8_t)v;
    return 1;
}

static void choose_arp_ip(void) {
    if (target_ip[0] == netdev.ipv4_addr[0] &&
        target_ip[1] == netdev.ipv4_addr[1] &&
        target_ip[2] == netdev.ipv4_addr[2]) {
        memcopy(arp_ip, target_ip, 4);
    } else {
        memcopy(arp_ip, netdev.ipv4_gateway, 4);
    }
}

static void set_arp_ip(const uint8_t ip[4]) {
    memcopy(arp_ip, ip, 4);
}

static void build_arp_request(void) {
    uint8_t* eth = tx_frame;
    memzero(tx_frame, 60);
    for (int i = 0; i < 6; i++) eth[i] = 0xff;
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x08;
    eth[13] = 0x06;
    put_be16(eth + 14, 1);
    put_be16(eth + 16, 0x0800);
    eth[18] = 6;
    eth[19] = 4;
    put_be16(eth + 20, 1);
    memcopy(eth + 22, netdev.mac, 6);
    memcopy(eth + 28, netdev.ipv4_addr, 4);
    memcopy(eth + 38, arp_ip, 4);
}

static int resolve_arp_for_current_ip(void);

static int is_arp_reply(long n) {
    if (n < 42) return 0;
    if (rx_frame[12] != 0x08 || rx_frame[13] != 0x06) return 0;
    if (rx_frame[20] != 0x00 || rx_frame[21] != 0x02) return 0;
    if (!memeq(rx_frame + 28, arp_ip, 4)) return 0;
    if (!memeq(rx_frame + 38, netdev.ipv4_addr, 4)) return 0;
    memcopy(target_mac, rx_frame + 22, 6);
    return 1;
}

static int resolve_arp(void) {
    choose_arp_ip();
    return resolve_arp_for_current_ip();
}

static int resolve_arp_for_current_ip(void) {
    build_arp_request();
    if (syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame, 60) < 0) return 0;
    for (int i = 0; i < 350000; i++) {
        long n = syscall3(AOS_SYS_NETDEV_RECV, (long)iface_index, (long)rx_frame, RX_SIZE);
        if (n < 0) return 0;
        if (is_arp_reply(n)) return 1;
    }
    return 0;
}

static uint16_t encode_qname(uint8_t* out, const char* name) {
    uint8_t* start = out;
    while (*name) {
        uint8_t* len = out++;
        uint8_t count = 0;
        while (*name && *name != '.') {
            if (count >= 63) return 0;
            *out++ = (uint8_t)*name++;
            count++;
        }
        *len = count;
        if (*name == '.') name++;
    }
    *out++ = 0;
    return (uint16_t)(out - start);
}

static uint16_t build_dns_query(const char* name) {
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* udp = tx_frame + 34;
    uint8_t* dns = tx_frame + 42;
    uint8_t* qname = dns + 12;
    uint16_t qlen;
    uint16_t dns_len;
    uint16_t udp_len;
    uint16_t ip_len;

    memzero(tx_frame, TX_SIZE);
    memcopy(eth, target_mac, 6);
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x08;
    eth[13] = 0x00;
    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = 17;
    memcopy(ip + 12, netdev.ipv4_addr, 4);
    memcopy(ip + 16, netdev.ipv4_dns, 4);
    put_be16(udp + 0, DNS_SRC_PORT);
    put_be16(udp + 2, 53);
    put_be16(dns + 0, DNS_TXID);
    put_be16(dns + 2, 0x0100);
    put_be16(dns + 4, 1);
    qlen = encode_qname(qname, name);
    put_be16(qname + qlen, 1);
    put_be16(qname + qlen + 2, 1);
    dns_len = (uint16_t)(12 + qlen + 4);
    udp_len = (uint16_t)(8 + dns_len);
    ip_len = (uint16_t)(20 + udp_len);
    put_be16(udp + 4, udp_len);
    put_be16(ip + 2, ip_len);
    put_be16(ip + 10, ipv4_checksum(ip, 20));
    return (uint16_t)(14 + ip_len);
}

static const uint8_t* skip_dns_name(const uint8_t* p, const uint8_t* end) {
    while (p < end) {
        uint8_t len = *p++;
        if (len == 0) return p;
        if ((len & 0xc0) == 0xc0) {
            if (p >= end) return end;
            return p + 1;
        }
        p += len;
    }
    return end;
}

static void decode_dns_name(const uint8_t* msg, const uint8_t* p, const uint8_t* end, char* out) {
    uint32_t out_i = 0;
    uint32_t jumps = 0;
    while (p < end && jumps < 24 && out_i < 127) {
        uint8_t len = *p++;
        if (len == 0) break;
        if ((len & 0xc0) == 0xc0) {
            uint16_t off;
            if (p >= end) break;
            off = (uint16_t)(((len & 0x3f) << 8) | *p);
            p = msg + off;
            jumps++;
            continue;
        }
        if (out_i) out[out_i++] = '.';
        while (len-- && p < end && out_i < 127) {
            out[out_i++] = (char)*p++;
        }
    }
    out[out_i] = 0;
}

static int parse_dns_response(long n, const char* wanted_name) {
    const uint8_t* msg;
    const uint8_t* p;
    const uint8_t* end = rx_frame + n;
    uint16_t qd;
    uint16_t an;
    if (n < 60) return 0;
    if (rx_frame[12] != 0x08 || rx_frame[13] != 0x00) return 0;
    if (rx_frame[23] != 17) return 0;
    if (!memeq(rx_frame + 26, netdev.ipv4_dns, 4)) return 0;
    if (!memeq(rx_frame + 30, netdev.ipv4_addr, 4)) return 0;
    if (get_be16(rx_frame + 34) != 53) return 0;
    if (get_be16(rx_frame + 36) != DNS_SRC_PORT) return 0;
    msg = rx_frame + 42;
    if (get_be16(msg) != DNS_TXID) return 0;
    if ((msg[3] & 0x0f) != 0) return 0;
    qd = get_be16(msg + 4);
    an = get_be16(msg + 6);
    p = msg + 12;
    for (uint16_t i = 0; i < qd; i++) {
        p = skip_dns_name(p, end);
        if (p + 4 > end) return 0;
        p += 4;
    }
    for (uint16_t i = 0; i < an; i++) {
        uint16_t type;
        uint16_t klass;
        uint16_t rdlen;
        p = skip_dns_name(p, end);
        if (p + 10 > end) return 0;
        type = get_be16(p);
        klass = get_be16(p + 2);
        rdlen = get_be16(p + 8);
        p += 10;
        if (p + rdlen > end) return 0;
        if (klass == 1 && type == 1 && rdlen == 4) {
            memcopy(target_ip, p, 4);
            return 1;
        }
        if (klass == 1 && type == 5) {
            decode_dns_name(msg, p, end, dns_cname);
        }
        p += rdlen;
    }
    (void)wanted_name;
    return 0;
}

static int resolve_dns(const char* name) {
    const char* current = name;
    for (int depth = 0; depth < 3; depth++) {
        for (int tries = 0; tries < 3; tries++) {
            set_arp_ip(netdev.ipv4_dns);
            if (!resolve_arp_for_current_ip()) return 0;
            if (syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame, build_dns_query(current)) < 0) return 0;
            write_cstr("dns: query ");
            write_cstr(current);
            write_cstr("\n");
            for (int i = 0; i < 900000; i++) {
                long n = syscall3(AOS_SYS_NETDEV_RECV, (long)iface_index, (long)rx_frame, RX_SIZE);
                if (n < 0) return 0;
                if (parse_dns_response(n, current)) {
                    write_cstr("dns: ");
                    write_cstr(name);
                    write_cstr(" -> ");
                    write_ipv4(target_ip);
                    write_cstr("\n");
                    return 1;
                }
            }
        }
        if (!dns_cname[0]) break;
        current = dns_cname;
        dns_cname[0] = 0;
    }
    return 0;
}

static int resolve_name(const char* name) {
    long rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)name, (long)target_ip, 0);
    if (rc >= 0) {
        write_cstr("dns: ");
        write_cstr(name);
        write_cstr(" -> ");
        write_ipv4(target_ip);
        write_cstr("\n");
        return 1;
    }
    return resolve_dns(name);
}

static uint16_t build_tcp_payload(uint8_t flags,
                                  uint32_t seq,
                                  uint32_t ack,
                                  const uint8_t* payload,
                                  uint16_t payload_len) {
    uint8_t* eth = tx_frame;
    uint8_t* ip = tx_frame + 14;
    uint8_t* tcp = tx_frame + 34;
    uint16_t tcp_len = (uint16_t)(20 + payload_len);
    uint16_t ip_len = (uint16_t)(20 + tcp_len);
    memzero(tx_frame, TX_SIZE);
    memcopy(eth, target_mac, 6);
    memcopy(eth + 6, netdev.mac, 6);
    eth[12] = 0x08;
    eth[13] = 0x00;
    ip[0] = 0x45;
    put_be16(ip + 2, ip_len);
    put_be16(ip + 4, 0xa055);
    ip[8] = 64;
    ip[9] = 6;
    memcopy(ip + 12, netdev.ipv4_addr, 4);
    memcopy(ip + 16, target_ip, 4);
    put_be16(ip + 10, ipv4_checksum(ip, 20));
    put_be16(tcp + 0, TCP_SRC_PORT);
    put_be16(tcp + 2, target_port);
    put_be32(tcp + 4, seq);
    put_be32(tcp + 8, ack);
    tcp[12] = 0x50;
    tcp[13] = flags;
    put_be16(tcp + 14, 64240);
    if (payload_len) memcopy(tcp + 20, payload, payload_len);
    put_be16(tcp + 16, tcp_checksum(ip, tcp, tcp_len));
    return (uint16_t)(14 + ip_len);
}

static uint16_t build_tcp(uint8_t flags, uint32_t seq, uint32_t ack) {
    return build_tcp_payload(flags, seq, ack, 0, 0);
}

static int parse_tcp_reply(long n, uint32_t* reply_seq) {
    uint8_t flags;
    if (n < 54) return 0;
    if (rx_frame[12] != 0x08 || rx_frame[13] != 0x00) return 0;
    if (rx_frame[23] != 6) return 0;
    if (!memeq(rx_frame + 26, target_ip, 4)) return 0;
    if (!memeq(rx_frame + 30, netdev.ipv4_addr, 4)) return 0;
    if (get_be16(rx_frame + 34) != target_port) return 0;
    if (get_be16(rx_frame + 36) != TCP_SRC_PORT) return 0;
    flags = rx_frame[47];
    if (flags & 0x04) return 2;
    if ((flags & 0x12) == 0x12 && get_be32(rx_frame + 42) == TCP_SEQ + 1) {
        *reply_seq = get_be32(rx_frame + 38);
        return 1;
    }
    return 0;
}

static uint16_t append_cstr(char* dst, uint16_t at, uint16_t cap, const char* src) {
    while (*src && at + 1 < cap) dst[at++] = *src++;
    dst[at] = 0;
    return at;
}

static uint16_t build_http_request(const char* host, const char* path) {
    uint16_t n = 0;
    if (!path || !path[0]) path = "/";
    n = append_cstr(http_req, n, sizeof(http_req), "GET ");
    n = append_cstr(http_req, n, sizeof(http_req), path);
    n = append_cstr(http_req, n, sizeof(http_req), " HTTP/1.0\r\nHost: ");
    n = append_cstr(http_req, n, sizeof(http_req), host);
    n = append_cstr(http_req, n, sizeof(http_req), "\r\nUser-Agent: AOS-httpget/0.1\r\nConnection: close\r\n\r\n");
    return n;
}

static int print_http_payload(long n, uint32_t* next_ack, uint32_t* got_payload) {
    uint16_t ip_len;
    uint16_t ip_hlen;
    uint16_t tcp_hlen;
    uint16_t payload_len;
    uint8_t flags;
    uint32_t seq;
    if (n < 54) return 0;
    if (rx_frame[12] != 0x08 || rx_frame[13] != 0x00) return 0;
    if (rx_frame[23] != 6) return 0;
    if (!memeq(rx_frame + 26, target_ip, 4)) return 0;
    if (!memeq(rx_frame + 30, netdev.ipv4_addr, 4)) return 0;
    if (get_be16(rx_frame + 34) != target_port) return 0;
    if (get_be16(rx_frame + 36) != TCP_SRC_PORT) return 0;
    ip_hlen = (uint16_t)((rx_frame[14] & 0x0f) * 4);
    tcp_hlen = (uint16_t)((rx_frame[46] >> 4) * 4);
    if (ip_hlen < 20 || tcp_hlen < 20) return 0;
    ip_len = get_be16(rx_frame + 16);
    if (ip_len < ip_hlen + tcp_hlen) return 0;
    payload_len = (uint16_t)(ip_len - ip_hlen - tcp_hlen);
    flags = rx_frame[47];
    seq = get_be32(rx_frame + 38);
    if (payload_len) {
        syscall3(SYS_WRITE, 1, (long)(rx_frame + 14 + ip_hlen + tcp_hlen), payload_len);
        *next_ack = seq + payload_len + ((flags & 0x01) ? 1U : 0U);
        *got_payload = 1;
        return (flags & 0x01) ? 2 : 1;
    }
    if (flags & 0x01) {
        *next_ack = seq + 1;
        return 2;
    }
    return 0;
}

void aos_main(uint64_t argc, char** argv) {
    uint32_t reply_seq = 0;
    uint8_t http_mode = is_httpget_name(argv[0]);
    uint64_t arg = 1;

    iface_index = 0;
    if (argc >= 4 && is_dash_i(argv[arg])) {
        if (!parse_iface_index(argv[arg + 1], &iface_index)) {
            write_cstr(http_mode ?
                       "usage: httpget [-i IFACE] HOST [PATH]\n" :
                       "usage: tcp [-i IFACE] HOST PORT\n");
            exit_code(1);
        }
        arg += 2;
    }

    if (http_mode) {
        if (argc - arg < 1) {
            write_cstr("usage: httpget [-i IFACE] HOST [PATH]\nexample: httpget -i 1 oppeku.org /\n");
            exit_code(1);
        }
        host_arg = argv[arg];
        path_arg = (argc - arg >= 2) ? argv[arg + 1] : "/";
        target_port = 80;
    } else {
        if (argc - arg < 2) {
            write_cstr("usage: tcp [-i IFACE] HOST PORT\nexample: tcp -i 1 oppeku.org 80\n");
            exit_code(1);
        }
        host_arg = argv[arg];
        port_arg = argv[arg + 1];
        if (!parse_u16(port_arg, &target_port)) {
            write_cstr("usage: tcp [-i IFACE] HOST PORT\nexample: tcp -i 1 oppeku.org 80\n");
            exit_code(1);
        }
    }
    copy_name(host_arg);
    if (syscall3(AOS_SYS_NETDEV_INFO, (long)iface_index, (long)&netdev, 0) < 0) {
        write_cstr("tcp: no network interface is registered\n");
        exit_code(1);
    }
    if (!netdev.link_up || !netdev.ipv4_configured) {
        write_cstr("tcp: interface is not IPv4-ready\n");
        exit_code(1);
    }
    if (!parse_ipv4(host_arg, target_ip) && !resolve_name(host_arg)) {
        write_cstr("tcp: DNS lookup failed\n");
        exit_code(1);
    }
    write_cstr("tcp: connecting ");
    write_cstr(target_name);
    write_cstr(":");
    write_u64(target_port);
    write_cstr(" from ");
    write_cstr(netdev.name);
    write_cstr("\n");
    if (!resolve_arp()) {
        write_cstr("tcp: ARP failed\n");
        exit_code(1);
    }
    if (syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame, build_tcp(0x02, TCP_SEQ, 0)) < 0) {
        write_cstr("tcp: SYN send failed\n");
        exit_code(1);
    }
    for (int i = 0; i < 900000; i++) {
        long n = syscall3(AOS_SYS_NETDEV_RECV, (long)iface_index, (long)rx_frame, RX_SIZE);
        int state;
        if (n < 0) {
            write_cstr("tcp: receive failed\n");
            exit_code(1);
        }
        state = parse_tcp_reply(n, &reply_seq);
        if (state == 1) {
            if (http_mode) {
                uint16_t req_len = build_http_request(target_name, path_arg);
                uint32_t next_ack = reply_seq + 1;
                uint32_t got_payload = 0;
                syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame,
                         build_tcp_payload(0x18, TCP_SEQ + 1, next_ack, (const uint8_t*)http_req, req_len));
                write_cstr("httpget: GET ");
                write_cstr(target_name);
                write_cstr(path_arg);
                write_cstr("\n");
                for (int j = 0; j < 1600000; j++) {
                    long m = syscall3(AOS_SYS_NETDEV_RECV, (long)iface_index, (long)rx_frame, RX_SIZE);
                    int http_state;
                    if (m < 0) {
                        write_cstr("httpget: receive failed\n");
                        exit_code(1);
                    }
                    http_state = print_http_payload(m, &next_ack, &got_payload);
                    if (http_state) {
                        syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame,
                                 build_tcp(0x10, TCP_SEQ + 1 + req_len, next_ack));
                    }
                    if (http_state == 2) {
                        syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame,
                                 build_tcp(0x14, TCP_SEQ + 1 + req_len, next_ack));
                        if (got_payload) write_cstr("\n");
                        exit_code(got_payload ? 0 : 1);
                    }
                }
                if (got_payload) {
                    syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame,
                             build_tcp(0x14, TCP_SEQ + 1 + req_len, next_ack));
                    write_cstr("\n");
                }
                if (!got_payload) write_cstr("httpget: no response payload\n");
                exit_code(got_payload ? 0 : 1);
            }
            syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame,
                     build_tcp(0x10, TCP_SEQ + 1, reply_seq + 1));
            syscall3(AOS_SYS_NETDEV_SEND, (long)iface_index, (long)tx_frame,
                     build_tcp(0x14, TCP_SEQ + 1, reply_seq + 1));
            write_cstr("tcp: open ");
            write_ipv4(target_ip);
            write_cstr(":");
            write_u64(target_port);
            write_cstr("\n");
            exit_code(0);
        }
        if (state == 2) {
            write_cstr("tcp: closed ");
            write_ipv4(target_ip);
            write_cstr(":");
            write_u64(target_port);
            write_cstr("\n");
            exit_code(1);
        }
    }
    write_cstr("tcp: timeout waiting for SYN-ACK\n");
    exit_code(1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
