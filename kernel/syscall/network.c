#include "syscall_internal.h"

struct linux_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint8_t sin_addr[4];
    uint8_t sin_zero[8];
} __attribute__((packed));

struct linux_sockaddr_in6 {
    uint16_t sin6_family;
    uint16_t sin6_port;
    uint32_t sin6_flowinfo;
    uint8_t sin6_addr[16];
    uint32_t sin6_scope_id;
} __attribute__((packed));

static uint16_t sock_get_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t sock_get_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void sock_put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void sock_put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t sock_checksum_add(uint32_t sum, const uint8_t* data, uint16_t len) {
    while (len > 1) {
        sum += (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)((uint16_t)data[0] << 8);
    }
    return sum;
}

static uint16_t sock_checksum_finish(uint32_t sum) {
    while (sum >> 16) {
        sum = (sum & 0xffffU) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static int sock_same_bytes(const uint8_t* a, const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static int sock_ipv4_same_subnet(const uint8_t a[4], const uint8_t b[4], uint8_t prefix) {
    uint32_t av;
    uint32_t bv;
    uint32_t mask;

    if (prefix == 0) return 0;
    if (prefix > 32) return 0;
    av = ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | a[3];
    bv = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    mask = prefix == 32 ? 0xffffffffU : (0xffffffffU << (32 - prefix));
    return (av & mask) == (bv & mask);
}

static int sock_ipv4_is_zero(const uint8_t ip[4]) {
    return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

static int sock_ipv6_is_zero(const uint8_t ip[16]) {
    for (size_t i = 0; i < 16; i++) {
        if (ip[i] != 0) return 0;
    }
    return 1;
}

static int sock_ipv6_is_link_local(const uint8_t ip[16]) {
    return ip[0] == 0xfe && (ip[1] & 0xc0U) == 0x80U;
}

static int sock_ipv6_same_prefix(const uint8_t a[16], const uint8_t b[16], uint8_t prefix) {
    uint8_t full = prefix / 8U;
    uint8_t rem = prefix % 8U;

    if (prefix > 128) return 0;
    for (uint8_t i = 0; i < full; i++) {
        if (a[i] != b[i]) return 0;
    }
    if (rem != 0) {
        uint8_t mask = (uint8_t)(0xffU << (8U - rem));
        if ((a[full] & mask) != (b[full] & mask)) return 0;
    }
    return 1;
}

static uint16_t sock_ipv4_checksum(uint8_t* ip) {
    ip[10] = 0;
    ip[11] = 0;
    return sock_checksum_finish(sock_checksum_add(0, ip, 20));
}

static uint16_t sock_tcp_checksum(const struct netdev* dev,
                                  const struct socket_object* sock,
                                  const uint8_t* tcp,
                                  uint16_t tcp_len) {
    uint32_t sum = 0;
    sum = sock_checksum_add(sum, dev->ipv4_addr, 4);
    sum = sock_checksum_add(sum, sock->remote_ip, 4);
    sum += LINUX_IPPROTO_TCP;
    sum += tcp_len;
    sum = sock_checksum_add(sum, tcp, tcp_len);
    return sock_checksum_finish(sum);
}

static uint16_t sock_tcp6_checksum(const struct netdev* dev,
                                   const struct socket_object* sock,
                                   const uint8_t* tcp,
                                   uint16_t tcp_len) {
    uint32_t sum = 0;
    uint8_t pseudo[8];

    sum = sock_checksum_add(sum, dev->ipv6_addr, 16);
    sum = sock_checksum_add(sum, sock->remote_ip6, 16);
    pseudo[0] = (uint8_t)(tcp_len >> 24);
    pseudo[1] = (uint8_t)(tcp_len >> 16);
    pseudo[2] = (uint8_t)(tcp_len >> 8);
    pseudo[3] = (uint8_t)tcp_len;
    pseudo[4] = 0;
    pseudo[5] = 0;
    pseudo[6] = 0;
    pseudo[7] = LINUX_IPPROTO_TCP;
    sum = sock_checksum_add(sum, pseudo, 8);
    sum = sock_checksum_add(sum, tcp, tcp_len);
    return sock_checksum_finish(sum);
}

static uint16_t sock_udp6_checksum(const struct netdev* dev,
                                   const uint8_t dst[16],
                                   const uint8_t* udp,
                                   uint16_t udp_len) {
    uint32_t sum = 0;
    uint8_t pseudo[8];
    uint16_t check;

    sum = sock_checksum_add(sum, dev->ipv6_addr, 16);
    sum = sock_checksum_add(sum, dst, 16);
    pseudo[0] = (uint8_t)(udp_len >> 24);
    pseudo[1] = (uint8_t)(udp_len >> 16);
    pseudo[2] = (uint8_t)(udp_len >> 8);
    pseudo[3] = (uint8_t)udp_len;
    pseudo[4] = 0;
    pseudo[5] = 0;
    pseudo[6] = 0;
    pseudo[7] = 17;
    sum = sock_checksum_add(sum, pseudo, 8);
    sum = sock_checksum_add(sum, udp, udp_len);
    check = sock_checksum_finish(sum);
    return check ? check : 0xffffU;
}

static int sock_copy_user_name(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0 || !src) return -1;
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    if (i == 0 || src[i] != '\0') return -1;
    return 0;
}

static int sock_name_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int dns_cache_lookup(uint8_t dev_index, const char* name, uint8_t family, uint8_t* out_ip) {
    uint64_t now = timer_get_ticks();

    for (size_t i = 0; i < SOCKET_DNS_CACHE_ENTRIES; i++) {
        if (!g_dns_cache[i].valid) continue;
        if (g_dns_cache[i].dev_index != dev_index) continue;
        if (g_dns_cache[i].family != family) continue;
        if (g_dns_cache[i].expires_at <= now) {
            g_dns_cache[i].valid = 0;
            continue;
        }
        if (sock_name_equal(g_dns_cache[i].name, name)) {
            local_memcpy(out_ip,
                         family == 6 ? g_dns_cache[i].ipv6 : g_dns_cache[i].ipv4,
                         family == 6 ? 16 : 4);
            g_dns_cache[i].hits++;
            return 1;
        }
    }
    return 0;
}

static void dns_cache_store(uint8_t dev_index, const char* name, uint8_t family, const uint8_t* ip) {
    size_t slot = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    for (size_t i = 0; i < SOCKET_DNS_CACHE_ENTRIES; i++) {
        if (!g_dns_cache[i].valid ||
            (g_dns_cache[i].dev_index == dev_index &&
             g_dns_cache[i].family == family &&
             sock_name_equal(g_dns_cache[i].name, name))) {
            slot = i;
            goto store;
        }
        if (g_dns_cache[i].expires_at < oldest) {
            oldest = g_dns_cache[i].expires_at;
            slot = i;
        }
    }

store:
    local_memset(&g_dns_cache[slot], 0, sizeof(g_dns_cache[slot]));
    g_dns_cache[slot].valid = 1;
    g_dns_cache[slot].dev_index = dev_index;
    g_dns_cache[slot].family = family;
    g_dns_cache[slot].expires_at = now + SOCKET_DNS_CACHE_TTL_TICKS;
    sock_copy_user_name(g_dns_cache[slot].name, sizeof(g_dns_cache[slot].name), name);
    local_memcpy(family == 6 ? g_dns_cache[slot].ipv6 : g_dns_cache[slot].ipv4,
                 ip,
                 family == 6 ? 16 : 4);
}

static int arp_cache_lookup(uint8_t dev_index, const uint8_t ip[4], uint8_t out_mac[6]) {
    uint64_t now = timer_get_ticks();

    if (!ip || !out_mac) return 0;
    for (size_t i = 0; i < SOCKET_ARP_CACHE_ENTRIES; i++) {
        if (!g_arp_cache[i].valid) continue;
        if (g_arp_cache[i].dev_index != dev_index) continue;
        if (g_arp_cache[i].expires_at <= now) {
            g_arp_cache[i].valid = 0;
            continue;
        }
        if (sock_same_bytes(g_arp_cache[i].ipv4, ip, 4)) {
            local_memcpy(out_mac, g_arp_cache[i].mac, 6);
            g_arp_cache[i].hits++;
            return 1;
        }
    }
    return 0;
}

static void arp_cache_store(uint8_t dev_index, const uint8_t ip[4], const uint8_t mac[6]) {
    size_t slot = 0;
    uint32_t hits = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    if (!ip || !mac) return;
    for (size_t i = 0; i < SOCKET_ARP_CACHE_ENTRIES; i++) {
        if (!g_arp_cache[i].valid) {
            slot = i;
            goto store;
        }
        if (g_arp_cache[i].dev_index == dev_index && sock_same_bytes(g_arp_cache[i].ipv4, ip, 4)) {
            slot = i;
            hits = g_arp_cache[i].hits;
            goto store;
        }
        if (g_arp_cache[i].expires_at < oldest) {
            oldest = g_arp_cache[i].expires_at;
            slot = i;
        }
    }

store:
    local_memset(&g_arp_cache[slot], 0, sizeof(g_arp_cache[slot]));
    g_arp_cache[slot].valid = 1;
    g_arp_cache[slot].dev_index = dev_index;
    g_arp_cache[slot].hits = hits;
    g_arp_cache[slot].expires_at = now + SOCKET_ARP_CACHE_TTL_TICKS;
    local_memcpy(g_arp_cache[slot].ipv4, ip, 4);
    local_memcpy(g_arp_cache[slot].mac, mac, 6);
}

static int ndp_cache_lookup(uint8_t dev_index, const uint8_t ip[16], uint8_t out_mac[6]) {
    uint64_t now = timer_get_ticks();

    if (!ip || !out_mac) return 0;
    for (size_t i = 0; i < SOCKET_NDP_CACHE_ENTRIES; i++) {
        if (!g_ndp_cache[i].valid) continue;
        if (g_ndp_cache[i].dev_index != dev_index) continue;
        if (g_ndp_cache[i].expires_at <= now) {
            g_ndp_cache[i].valid = 0;
            continue;
        }
        if (sock_same_bytes(g_ndp_cache[i].ipv6, ip, 16)) {
            local_memcpy(out_mac, g_ndp_cache[i].mac, 6);
            g_ndp_cache[i].hits++;
            return 1;
        }
    }
    return 0;
}

static void ndp_cache_store(uint8_t dev_index, const uint8_t ip[16], const uint8_t mac[6]) {
    size_t slot = 0;
    uint32_t hits = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    if (!ip || !mac) return;
    for (size_t i = 0; i < SOCKET_NDP_CACHE_ENTRIES; i++) {
        if (!g_ndp_cache[i].valid) {
            slot = i;
            goto store;
        }
        if (g_ndp_cache[i].dev_index == dev_index && sock_same_bytes(g_ndp_cache[i].ipv6, ip, 16)) {
            slot = i;
            hits = g_ndp_cache[i].hits;
            goto store;
        }
        if (g_ndp_cache[i].expires_at < oldest) {
            oldest = g_ndp_cache[i].expires_at;
            slot = i;
        }
    }

store:
    local_memset(&g_ndp_cache[slot], 0, sizeof(g_ndp_cache[slot]));
    g_ndp_cache[slot].valid = 1;
    g_ndp_cache[slot].dev_index = dev_index;
    g_ndp_cache[slot].hits = hits;
    g_ndp_cache[slot].expires_at = now + SOCKET_NDP_CACHE_TTL_TICKS;
    local_memcpy(g_ndp_cache[slot].ipv6, ip, 16);
    local_memcpy(g_ndp_cache[slot].mac, mac, 6);
}

static uint16_t sock_encode_dns_qname(uint8_t* out, const char* name) {
    uint8_t* start = out;

    while (*name) {
        uint8_t* len = out++;
        uint8_t count = 0;
        while (*name && *name != '.') {
            if (count >= 63) return 0;
            *out++ = (uint8_t)*name++;
            count++;
        }
        if (count == 0) return 0;
        *len = count;
        if (*name == '.') name++;
    }
    *out++ = 0;
    return (uint16_t)(out - start);
}

static uint16_t sock_build_dns_query(const struct netdev* dev,
                                     const uint8_t dns_mac[6],
                                     const char* name,
                                     uint16_t qtype,
                                     uint8_t* frame) {
    uint8_t* ip = frame + 14;
    uint8_t* udp = frame + 34;
    uint8_t* dns = frame + 42;
    uint8_t* qname = dns + 12;
    uint16_t qlen;
    uint16_t dns_len;
    uint16_t udp_len;
    uint16_t ip_len;
    uint16_t ip_sum;

    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    local_memcpy(frame, dns_mac, 6);
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;

    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = 17;
    local_memcpy(ip + 12, dev->ipv4_addr, 4);
    local_memcpy(ip + 16, dev->ipv4_dns, 4);

    sock_put_be16(udp + 0, SOCKET_DNS_PORT);
    sock_put_be16(udp + 2, 53);

    sock_put_be16(dns + 0, SOCKET_DNS_TXID);
    sock_put_be16(dns + 2, 0x0100);
    sock_put_be16(dns + 4, 1);
    qlen = sock_encode_dns_qname(qname, name);
    if (qlen == 0) return 0;
    sock_put_be16(qname + qlen, qtype);
    sock_put_be16(qname + qlen + 2, 1);

    dns_len = (uint16_t)(12 + qlen + 4);
    udp_len = (uint16_t)(8 + dns_len);
    ip_len = (uint16_t)(20 + udp_len);
    sock_put_be16(udp + 4, udp_len);
    sock_put_be16(ip + 2, ip_len);
    ip_sum = sock_ipv4_checksum(ip);
    sock_put_be16(ip + 10, ip_sum);
    return (uint16_t)(14 + ip_len);
}

static uint16_t sock_build_dns_query6(const struct netdev* dev,
                                      const uint8_t dns_mac[6],
                                      const char* name,
                                      uint16_t qtype,
                                      uint8_t* frame) {
    uint8_t* ip = frame + 14;
    uint8_t* udp = frame + 54;
    uint8_t* dns = frame + 62;
    uint8_t* qname = dns + 12;
    uint16_t qlen;
    uint16_t dns_len;
    uint16_t udp_len;
    uint16_t udp_sum;

    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    local_memcpy(frame, dns_mac, 6);
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x86;
    frame[13] = 0xdd;

    ip[0] = 0x60;
    ip[6] = 17;
    ip[7] = 64;
    local_memcpy(ip + 8, dev->ipv6_addr, 16);
    local_memcpy(ip + 24, dev->ipv6_dns, 16);

    sock_put_be16(udp + 0, SOCKET_DNS_PORT);
    sock_put_be16(udp + 2, 53);

    sock_put_be16(dns + 0, SOCKET_DNS_TXID);
    sock_put_be16(dns + 2, 0x0100);
    sock_put_be16(dns + 4, 1);
    qlen = sock_encode_dns_qname(qname, name);
    if (qlen == 0) return 0;
    sock_put_be16(qname + qlen, qtype);
    sock_put_be16(qname + qlen + 2, 1);

    dns_len = (uint16_t)(12 + qlen + 4);
    udp_len = (uint16_t)(8 + dns_len);
    sock_put_be16(udp + 4, udp_len);
    sock_put_be16(ip + 4, udp_len);
    udp_sum = sock_udp6_checksum(dev, dev->ipv6_dns, udp, udp_len);
    sock_put_be16(udp + 6, udp_sum);
    return (uint16_t)(14 + 40 + udp_len);
}

static const uint8_t* sock_skip_dns_name(const uint8_t* p, const uint8_t* end) {
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

static void sock_decode_dns_name(const uint8_t* msg,
                                 const uint8_t* p,
                                 const uint8_t* end,
                                 char* out,
                                 size_t out_size) {
    size_t out_i = 0;
    uint32_t jumps = 0;

    if (!out || out_size == 0) return;
    while (p < end && jumps < 24 && out_i + 1 < out_size) {
        uint8_t len = *p++;
        if (len == 0) break;
        if ((len & 0xc0) == 0xc0) {
            uint16_t off;
            if (p >= end) break;
            off = (uint16_t)(((uint16_t)(len & 0x3f) << 8) | *p);
            p = msg + off;
            jumps++;
            continue;
        }
        if (out_i && out_i + 1 < out_size) out[out_i++] = '.';
        while (len-- && p < end && out_i + 1 < out_size) {
            out[out_i++] = (char)*p++;
        }
    }
    out[out_i] = '\0';
}

static int sock_parse_dns_response(const struct netdev* dev,
                                   const uint8_t* frame,
                                   int len,
                                   uint16_t qtype,
                                   uint8_t* out_addr,
                                   char cname[SOCKET_DNS_NAME_MAX]) {
    const uint8_t* msg;
    const uint8_t* p;
    const uint8_t* end = frame + len;
    uint16_t qd;
    uint16_t an;

    if (len < 60) return 0;
    if (frame[12] != 0x08 || frame[13] != 0x00) return 0;
    if (frame[23] != 17) return 0;
    if (!sock_same_bytes(frame + 26, dev->ipv4_dns, 4)) return 0;
    if (!sock_same_bytes(frame + 30, dev->ipv4_addr, 4)) return 0;
    if (sock_get_be16(frame + 34) != 53) return 0;
    if (sock_get_be16(frame + 36) != SOCKET_DNS_PORT) return 0;

    msg = frame + 42;
    if (sock_get_be16(msg) != SOCKET_DNS_TXID) return 0;
    if ((msg[2] & 0x80) == 0) return 0;
    if ((msg[3] & 0x0f) != 0) return -1;

    qd = sock_get_be16(msg + 4);
    an = sock_get_be16(msg + 6);
    p = msg + 12;
    for (uint16_t i = 0; i < qd; i++) {
        p = sock_skip_dns_name(p, end);
        if (p + 4 > end) return 0;
        p += 4;
    }
    for (uint16_t i = 0; i < an; i++) {
        uint16_t type;
        uint16_t klass;
        uint16_t rdlen;
        p = sock_skip_dns_name(p, end);
        if (p + 10 > end) return 0;
        type = sock_get_be16(p);
        klass = sock_get_be16(p + 2);
        rdlen = sock_get_be16(p + 8);
        p += 10;
        if (p + rdlen > end) return 0;
        if (klass == 1 && type == qtype &&
            ((qtype == 1 && rdlen == 4) || (qtype == 28 && rdlen == 16))) {
            local_memcpy(out_addr, p, rdlen);
            return 1;
        }
        if (klass == 1 && type == 5 && cname) {
            sock_decode_dns_name(msg, p, end, cname, SOCKET_DNS_NAME_MAX);
        }
        p += rdlen;
    }
    if (cname && cname[0]) return 2;
    return -1;
}

static int sock_parse_dns_response6(const struct netdev* dev,
                                    const uint8_t* frame,
                                    int len,
                                    uint16_t qtype,
                                    uint8_t* out_addr,
                                    char cname[SOCKET_DNS_NAME_MAX]) {
    const uint8_t* msg;
    const uint8_t* p;
    const uint8_t* end = frame + len;
    const uint8_t* ip = frame + 14;
    const uint8_t* udp = frame + 54;
    uint16_t qd;
    uint16_t an;

    if (len < 74) return 0;
    if (frame[12] != 0x86 || frame[13] != 0xdd) return 0;
    if (ip[6] != 17) return 0;
    if (!sock_same_bytes(ip + 8, dev->ipv6_dns, 16)) return 0;
    if (!sock_same_bytes(ip + 24, dev->ipv6_addr, 16)) return 0;
    if (sock_get_be16(udp + 0) != 53) return 0;
    if (sock_get_be16(udp + 2) != SOCKET_DNS_PORT) return 0;

    msg = frame + 62;
    if (sock_get_be16(msg) != SOCKET_DNS_TXID) return 0;
    if ((msg[2] & 0x80) == 0) return 0;
    if ((msg[3] & 0x0f) != 0) return -1;

    qd = sock_get_be16(msg + 4);
    an = sock_get_be16(msg + 6);
    p = msg + 12;
    for (uint16_t i = 0; i < qd; i++) {
        p = sock_skip_dns_name(p, end);
        if (p + 4 > end) return 0;
        p += 4;
    }
    for (uint16_t i = 0; i < an; i++) {
        uint16_t type;
        uint16_t klass;
        uint16_t rdlen;
        p = sock_skip_dns_name(p, end);
        if (p + 10 > end) return 0;
        type = sock_get_be16(p);
        klass = sock_get_be16(p + 2);
        rdlen = sock_get_be16(p + 8);
        p += 10;
        if (p + rdlen > end) return 0;
        if (klass == 1 && type == qtype &&
            ((qtype == 1 && rdlen == 4) || (qtype == 28 && rdlen == 16))) {
            local_memcpy(out_addr, p, rdlen);
            return 1;
        }
        if (klass == 1 && type == 5 && cname) {
            sock_decode_dns_name(msg, p, end, cname, SOCKET_DNS_NAME_MAX);
        }
        p += rdlen;
    }
    if (cname && cname[0]) return 2;
    return -1;
}

static uint16_t sock_build_arp_request(const struct netdev* dev,
                                       const uint8_t target_ip[4],
                                       uint8_t* frame) {
    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    for (size_t i = 0; i < 6; i++) frame[i] = 0xff;
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x08;
    frame[13] = 0x06;
    sock_put_be16(frame + 14, 1);
    sock_put_be16(frame + 16, 0x0800);
    frame[18] = 6;
    frame[19] = 4;
    sock_put_be16(frame + 20, 1);
    local_memcpy(frame + 22, dev->mac, 6);
    local_memcpy(frame + 28, dev->ipv4_addr, 4);
    local_memcpy(frame + 38, target_ip, 4);
    return 42;
}

static int sock_parse_arp_reply(const uint8_t* frame,
                                int len,
                                const uint8_t target_ip[4],
                                const uint8_t local_ip[4],
                                uint8_t out_mac[6]) {
    if (len < 42) return 0;
    if (frame[12] != 0x08 || frame[13] != 0x06) return 0;
    if (sock_get_be16(frame + 20) != 2) return 0;
    if (!sock_same_bytes(frame + 28, target_ip, 4)) return 0;
    if (!sock_same_bytes(frame + 38, local_ip, 4)) return 0;
    local_memcpy(out_mac, frame + 22, 6);
    return 1;
}

static uint16_t sock_icmp6_checksum(const struct netdev* dev,
                                    const uint8_t dst[16],
                                    const uint8_t* icmp,
                                    uint16_t icmp_len) {
    uint32_t sum = 0;
    uint8_t pseudo[8];

    sum = sock_checksum_add(sum, dev->ipv6_addr, 16);
    sum = sock_checksum_add(sum, dst, 16);
    pseudo[0] = (uint8_t)(icmp_len >> 24);
    pseudo[1] = (uint8_t)(icmp_len >> 16);
    pseudo[2] = (uint8_t)(icmp_len >> 8);
    pseudo[3] = (uint8_t)icmp_len;
    pseudo[4] = 0;
    pseudo[5] = 0;
    pseudo[6] = 0;
    pseudo[7] = 58;
    sum = sock_checksum_add(sum, pseudo, 8);
    sum = sock_checksum_add(sum, icmp, icmp_len);
    return sock_checksum_finish(sum);
}

static void sock_solicited_node_multicast(const uint8_t target[16], uint8_t out[16]) {
    local_memset(out, 0, 16);
    out[0] = 0xff;
    out[1] = 0x02;
    out[11] = 0x01;
    out[12] = 0xff;
    out[13] = target[13];
    out[14] = target[14];
    out[15] = target[15];
}

static uint16_t sock_build_ndp_solicit(const struct netdev* dev,
                                       const uint8_t target_ip[16],
                                       uint8_t* frame) {
    uint8_t multicast_ip[16];
    uint8_t* ip = frame + 14;
    uint8_t* icmp = frame + 54;
    uint16_t sum;

    sock_solicited_node_multicast(target_ip, multicast_ip);
    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    frame[0] = 0x33;
    frame[1] = 0x33;
    frame[2] = 0xff;
    frame[3] = target_ip[13];
    frame[4] = target_ip[14];
    frame[5] = target_ip[15];
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x86;
    frame[13] = 0xdd;

    ip[0] = 0x60;
    sock_put_be16(ip + 4, 32);
    ip[6] = 58;
    ip[7] = 255;
    local_memcpy(ip + 8, dev->ipv6_addr, 16);
    local_memcpy(ip + 24, multicast_ip, 16);

    icmp[0] = 135;
    local_memcpy(icmp + 8, target_ip, 16);
    icmp[24] = 1;
    icmp[25] = 1;
    local_memcpy(icmp + 26, dev->mac, 6);
    sum = sock_icmp6_checksum(dev, multicast_ip, icmp, 32);
    sock_put_be16(icmp + 2, sum);
    return 86;
}

static int sock_parse_ndp_advert(const uint8_t* frame,
                                 int len,
                                 const uint8_t target_ip[16],
                                 uint8_t out_mac[6]) {
    const uint8_t* ip = frame + 14;
    const uint8_t* icmp = frame + 54;

    if (len < 86) return 0;
    if (frame[12] != 0x86 || frame[13] != 0xdd) return 0;
    if (ip[6] != 58 || icmp[0] != 136) return 0;
    if (!sock_same_bytes(ip + 8, target_ip, 16)) return 0;
    if (!sock_same_bytes(icmp + 8, target_ip, 16)) return 0;
    local_memcpy(out_mac, frame + 6, 6);
    if (icmp[24] == 2 && icmp[25] == 1) {
        local_memcpy(out_mac, icmp + 26, 6);
    }
    return 1;
}

static int sock_arp_lookup(struct socket_object* sock, const struct netdev* dev) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint16_t len;

    if (arp_cache_lookup(sock->dev_index, sock->next_hop_ip, sock->remote_mac)) {
        return 0;
    }

    len = sock_build_arp_request(dev, sock->next_hop_ip, tx);
    if (netdev_send(sock->dev_index, tx, len) < 0) {
        return -1;
    }
    for (int i = 0; i < 400000; i++) {
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (sock_parse_arp_reply(rx, n, sock->next_hop_ip, dev->ipv4_addr, sock->remote_mac)) {
            arp_cache_store(sock->dev_index, sock->next_hop_ip, sock->remote_mac);
            return 0;
        }
    }
    return -1;
}

static int sock_ndp_lookup(struct socket_object* sock, const struct netdev* dev) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint16_t len;

    if (ndp_cache_lookup(sock->dev_index, sock->next_hop_ip6, sock->remote_mac)) {
        return 0;
    }

    len = sock_build_ndp_solicit(dev, sock->next_hop_ip6, tx);
    if (netdev_send(sock->dev_index, tx, len) < 0) {
        return -1;
    }
    for (int i = 0; i < 500000; i++) {
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (sock_parse_ndp_advert(rx, n, sock->next_hop_ip6, sock->remote_mac)) {
            ndp_cache_store(sock->dev_index, sock->next_hop_ip6, sock->remote_mac);
            return 0;
        }
    }
    return -1;
}

static uint64_t sock_timeout_ticks(uint64_t seconds) {
    uint32_t frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;
    return (uint64_t)frequency * seconds;
}

static uint64_t sock_retry_timeout_ticks(int attempt) {
    uint64_t seconds = SOCKET_TCP_INITIAL_RTO_SECONDS;

    if (attempt < 0) attempt = 0;
    if (attempt > SOCKET_TCP_MAX_RTO_SHIFT) attempt = SOCKET_TCP_MAX_RTO_SHIFT;
    while (attempt-- > 0) {
        seconds <<= 1;
    }
    return sock_timeout_ticks(seconds);
}

static uint16_t sock_effective_mss(const struct socket_object* sock) {
    uint16_t mss = sock ? sock->peer_mss : SOCKET_TCP_DEFAULT_PEER_MSS;

    if (mss == 0 || mss > SOCKET_TCP_LOCAL_MSS) {
        mss = SOCKET_TCP_LOCAL_MSS;
    }
    return mss;
}

static void sock_init_congestion(struct socket_object* sock) {
    uint32_t mss = sock_effective_mss(sock);

    if (!sock) return;
    sock->cwnd_bytes = mss * SOCKET_TCP_INITIAL_CWND_SEGMENTS;
    sock->ssthresh_bytes = mss * SOCKET_TCP_INITIAL_SSTHRESH_SEGMENTS;
}

static void sock_note_ack(struct socket_object* sock, uint16_t acked_bytes) {
    uint32_t mss;

    if (!sock || acked_bytes == 0) return;
    mss = sock_effective_mss(sock);
    if (sock->cwnd_bytes < mss) sock->cwnd_bytes = mss;
    if (sock->ssthresh_bytes < mss) sock->ssthresh_bytes = mss;

    if (sock->cwnd_bytes < sock->ssthresh_bytes) {
        sock->cwnd_bytes += mss;
    } else {
        uint32_t add = (mss * mss) / sock->cwnd_bytes;
        if (add == 0) add = 1;
        sock->cwnd_bytes += add;
    }
}

static void sock_note_retransmit_timeout(struct socket_object* sock) {
    uint32_t mss;

    if (!sock) return;
    mss = sock_effective_mss(sock);
    sock->ssthresh_bytes = sock->cwnd_bytes / 2U;
    if (sock->ssthresh_bytes < 2U * mss) {
        sock->ssthresh_bytes = 2U * mss;
    }
    sock->cwnd_bytes = mss;
}

static uint32_t socket_rx_available(const struct socket_object* sock) {
    if (!sock || sock->rx_off >= sock->rx_len) return 0;
    return sock->rx_len - sock->rx_off;
}

static uint32_t socket_rx_free_bytes(const struct socket_object* sock) {
    uint32_t used = socket_rx_available(sock);

    if (used >= SOCKET_RX_BUFFER_SIZE) return 0;
    return SOCKET_RX_BUFFER_SIZE - used;
}

static uint16_t sock_advertised_window(const struct socket_object* sock, uint8_t flags) {
    uint32_t free_bytes = socket_rx_free_bytes(sock);
    uint8_t scale = sock ? sock->local_window_scale : 0;

    if ((flags & 0x02) == 0 && scale > 0) {
        free_bytes >>= scale;
    }
    if (free_bytes > 65535U) free_bytes = 65535U;
    return (uint16_t)free_bytes;
}

static uint16_t sock_tcp_header_len(uint8_t flags) {
    return (flags & 0x02) ? 32U : 20U;
}

static void sock_write_tcp_syn_options(struct socket_object* sock, uint8_t* tcp) {
    tcp[20] = 2;
    tcp[21] = 4;
    sock_put_be16(tcp + 22, SOCKET_TCP_LOCAL_MSS);
    tcp[24] = 1;
    tcp[25] = 3;
    tcp[26] = 3;
    tcp[27] = sock ? sock->local_window_scale : SOCKET_TCP_LOCAL_WINDOW_SCALE;
    tcp[28] = 0;
    tcp[29] = 0;
    tcp[30] = 0;
    tcp[31] = 0;
}

static uint16_t sock_build_tcp_frame(struct socket_object* sock,
                                     uint8_t flags,
                                     const uint8_t* payload,
                                     uint16_t payload_len,
                                     uint8_t* frame) {
    const struct netdev* dev = netdev_get(sock->dev_index);
    uint8_t* ip = frame + 14;
    uint16_t tcp_hlen = sock_tcp_header_len(flags);
    uint16_t tcp_len = (uint16_t)(tcp_hlen + payload_len);
    uint8_t* tcp = sock->family == LINUX_AF_INET6 ? frame + 54 : frame + 34;
    uint16_t tcp_sum;

    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    local_memcpy(frame, sock->remote_mac, 6);
    local_memcpy(frame + 6, dev->mac, 6);

    if (sock->family == LINUX_AF_INET6) {
        frame[12] = 0x86;
        frame[13] = 0xdd;
        ip[0] = 0x60;
        sock_put_be16(ip + 4, tcp_len);
        ip[6] = LINUX_IPPROTO_TCP;
        ip[7] = 64;
        local_memcpy(ip + 8, dev->ipv6_addr, 16);
        local_memcpy(ip + 24, sock->remote_ip6, 16);
    } else {
        uint16_t total_len = (uint16_t)(20 + tcp_len);
        uint16_t ip_sum;

        frame[12] = 0x08;
        frame[13] = 0x00;
        ip[0] = 0x45;
        ip[1] = 0;
        sock_put_be16(ip + 2, total_len);
        sock_put_be16(ip + 4, 0xa055);
        ip[8] = 64;
        ip[9] = LINUX_IPPROTO_TCP;
        local_memcpy(ip + 12, dev->ipv4_addr, 4);
        local_memcpy(ip + 16, sock->remote_ip, 4);
        ip_sum = sock_ipv4_checksum(ip);
        sock_put_be16(ip + 10, ip_sum);
    }

    sock_put_be16(tcp + 0, sock->local_port);
    sock_put_be16(tcp + 2, sock->remote_port);
    sock_put_be32(tcp + 4, sock->seq);
    sock_put_be32(tcp + 8, sock->ack);
    tcp[12] = (uint8_t)((tcp_hlen / 4U) << 4);
    tcp[13] = flags;
    sock_put_be16(tcp + 14, sock_advertised_window(sock, flags));
    if (tcp_hlen > 20U) {
        sock_write_tcp_syn_options(sock, tcp);
    }
    if (payload_len && payload) {
        local_memcpy(tcp + tcp_hlen, payload, payload_len);
    }
    tcp_sum = sock->family == LINUX_AF_INET6 ?
              sock_tcp6_checksum(dev, sock, tcp, tcp_len) :
              sock_tcp_checksum(dev, sock, tcp, tcp_len);
    sock_put_be16(tcp + 16, tcp_sum);
    return (uint16_t)(sock->family == LINUX_AF_INET6 ? 54 + tcp_len : 34 + tcp_len);
}

static void sock_parse_tcp_options(struct socket_object* sock,
                                   const uint8_t* tcp,
                                   uint16_t tcp_hlen,
                                   uint8_t flags) {
    uint16_t off = 20;

    if (!sock || tcp_hlen <= 20U || (flags & 0x02) == 0) return;
    while (off < tcp_hlen) {
        uint8_t kind = tcp[off];
        uint8_t opt_len;

        if (kind == 0) break;
        if (kind == 1) {
            off++;
            continue;
        }
        if (off + 1U >= tcp_hlen) break;
        opt_len = tcp[off + 1U];
        if (opt_len < 2U || off + opt_len > tcp_hlen) break;
        if (kind == 2 && opt_len == 4U) {
            uint16_t mss = sock_get_be16(tcp + off + 2U);
            if (mss != 0) {
                sock->peer_mss = mss > SOCKET_TCP_LOCAL_MSS ? SOCKET_TCP_LOCAL_MSS : mss;
            }
        } else if (kind == 3 && opt_len == 3U) {
            uint8_t scale = tcp[off + 2U];
            sock->peer_window_scale = scale > 14U ? 14U : scale;
        }
        off = (uint16_t)(off + opt_len);
    }
}

static int sock_parse_tcp_frame(struct socket_object* sock,
                                const uint8_t* frame,
                                int len,
                                uint8_t* flags,
                                uint32_t* seq,
                                uint32_t* ack,
                                const uint8_t** payload,
                                uint16_t* payload_len) {
    const struct netdev* dev = netdev_get(sock->dev_index);
    uint16_t ip_len;
    uint16_t ip_hlen;
    uint16_t tcp_hlen;
    uint16_t tcp_off;

    if (!dev) return 0;
    if (sock->family == LINUX_AF_INET6) {
        if (len < 74) return 0;
        if (frame[12] != 0x86 || frame[13] != 0xdd) return 0;
        if (frame[20] != LINUX_IPPROTO_TCP) return 0;
        if (!sock_same_bytes(frame + 22, sock->remote_ip6, 16)) return 0;
        if (!sock_same_bytes(frame + 38, dev->ipv6_addr, 16)) return 0;
        ip_hlen = 40;
        ip_len = (uint16_t)(40 + sock_get_be16(frame + 18));
        tcp_off = 54;
    } else {
        if (len < 54) return 0;
        if (frame[12] != 0x08 || frame[13] != 0x00) return 0;
        if (frame[23] != LINUX_IPPROTO_TCP) return 0;
        if (!sock_same_bytes(frame + 26, sock->remote_ip, 4)) return 0;
        if (!sock_same_bytes(frame + 30, dev->ipv4_addr, 4)) return 0;
        ip_hlen = (uint16_t)((frame[14] & 0x0f) * 4);
        if (ip_hlen < 20) return 0;
        ip_len = sock_get_be16(frame + 16);
        tcp_off = (uint16_t)(14 + ip_hlen);
    }
    if (sock_get_be16(frame + tcp_off) != sock->remote_port) return 0;
    if (sock_get_be16(frame + tcp_off + 2) != sock->local_port) return 0;
    tcp_hlen = (uint16_t)((frame[tcp_off + 12] >> 4) * 4);
    if (tcp_hlen < 20) return 0;
    if (ip_len < ip_hlen + tcp_hlen) return 0;
    if (14U + ip_len > (uint16_t)len) return 0;
    *flags = frame[tcp_off + 13];
    *seq = sock_get_be32(frame + tcp_off + 4);
    if (ack) *ack = sock_get_be32(frame + tcp_off + 8);
    sock->peer_window = sock_get_be16(frame + tcp_off + 14);
    sock->peer_window_bytes = (uint32_t)sock->peer_window << sock->peer_window_scale;
    sock_parse_tcp_options(sock, frame + tcp_off, tcp_hlen, *flags);
    sock->peer_window_bytes = (uint32_t)sock->peer_window << sock->peer_window_scale;
    *payload = frame + tcp_off + tcp_hlen;
    *payload_len = (uint16_t)(ip_len - ip_hlen - tcp_hlen);
    return 1;
}

static int sock_wait_tcp_ticks(struct socket_object* sock,
                               uint8_t want_flags,
                               uint64_t timeout_ticks,
                               uint8_t* out_flags,
                               uint32_t* out_seq,
                               uint32_t* out_ack,
                               const uint8_t** out_payload,
                               uint16_t* out_payload_len) {
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start = timer_get_ticks();

    if (timeout_ticks == 0) timeout_ticks = 1;

    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        if ((flags & 0x04) ||
            (want_flags == 0x12 && (flags & 0x12) == 0x12) ||
            (want_flags != 0x12 && (payload_len > 0 || (flags & 0x01) || (flags & 0x04)))) {
            if (out_flags) *out_flags = flags;
            if (out_seq) *out_seq = seq;
            if (out_ack) *out_ack = ack;
            if (out_payload) *out_payload = payload;
            if (out_payload_len) *out_payload_len = payload_len;
            return 0;
        }
    }
    return -1;
}

static int sock_wait_tcp(struct socket_object* sock,
                         uint8_t want_flags,
                         uint8_t* out_flags,
                         uint32_t* out_seq,
                         uint32_t* out_ack,
                         const uint8_t** out_payload,
                         uint16_t* out_payload_len) {
    uint32_t frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;
    return sock_wait_tcp_ticks(sock,
                               want_flags,
                               (uint64_t)frequency * 8ULL,
                               out_flags,
                               out_seq,
                               out_ack,
                               out_payload,
                               out_payload_len);
}

static int socket_send_ack(struct socket_object* sock) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint16_t len = sock_build_tcp_frame(sock, 0x10, NULL, 0, tx);
    return netdev_send(sock->dev_index, tx, len);
}

static void socket_rx_compact(struct socket_object* sock) {
    uint32_t avail;

    if (!sock || sock->rx_off == 0) return;
    avail = socket_rx_available(sock);
    if (avail > 0) {
        local_memcpy(sock->rx_buffer, sock->rx_buffer + sock->rx_off, avail);
    }
    sock->rx_off = 0;
    sock->rx_len = avail;
}

static uint16_t socket_rx_append(struct socket_object* sock,
                                 const uint8_t* payload,
                                 uint16_t payload_len) {
    uint32_t free_bytes;
    uint16_t store;

    if (!sock || !payload || payload_len == 0) return 0;
    if (sock->rx_len > SOCKET_RX_BUFFER_SIZE) {
        sock->rx_len = SOCKET_RX_BUFFER_SIZE;
    }
    free_bytes = SOCKET_RX_BUFFER_SIZE - sock->rx_len;
    if (free_bytes < payload_len && sock->rx_off > 0) {
        socket_rx_compact(sock);
        free_bytes = SOCKET_RX_BUFFER_SIZE - sock->rx_len;
    }
    store = payload_len;
    if (store > free_bytes) store = (uint16_t)free_bytes;
    if (store == 0) return 0;
    local_memcpy(sock->rx_buffer + sock->rx_len, payload, store);
    sock->rx_len += store;
    return store;
}

static int socket_store_rx_payload(struct socket_object* sock,
                                   const uint8_t* payload,
                                   uint16_t payload_len,
                                   uint32_t seq,
                                   int includes_fin) {
    uint16_t stored;

    if (!sock || !payload || payload_len == 0) {
        return 0;
    }
    if ((int32_t)(seq - sock->ack) < 0) {
        (void)socket_send_ack(sock);
        return 0;
    }
    if (seq != sock->ack) {
        (void)socket_send_ack(sock);
        return 0;
    }
    stored = socket_rx_append(sock, payload, payload_len);
    if (stored == 0) {
        (void)socket_send_ack(sock);
        return 0;
    }
    sock->ack = seq + stored + ((stored == payload_len && includes_fin) ? 1U : 0U);
    socket_send_ack(sock);
    return stored == payload_len ? 1 : 0;
}

static int sock_wait_for_send_window(struct socket_object* sock, uint64_t timeout_ticks) {
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start = timer_get_ticks();

    if (!sock || sock->state != SOCKET_STATE_CONNECTED) return -1;
    if (sock->peer_window_bytes > 0) return 0;
    if (timeout_ticks == 0) timeout_ticks = 1;

    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        (void)ack;
        if (flags & 0x04) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
        if (payload_len > 0) {
            if (socket_store_rx_payload(sock, payload, payload_len, seq, (flags & 0x01) != 0) &&
                (flags & 0x01) != 0) {
                sock->state = SOCKET_STATE_CLOSED;
                return -1;
            }
        } else if (flags & 0x01) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
        if (sock->peer_window_bytes > 0) {
            return 0;
        }
    }
    return -1;
}

static int sock_wait_ack(struct socket_object* sock, uint32_t expected_ack, uint64_t timeout_ticks) {
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start = timer_get_ticks();

    if (timeout_ticks == 0) timeout_ticks = 1;

    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        if (flags & 0x04) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
        if (payload_len > 0) {
            if (socket_store_rx_payload(sock, payload, payload_len, seq, (flags & 0x01) != 0) &&
                (flags & 0x01) != 0) {
                sock->state = SOCKET_STATE_CLOSED;
            }
        }
        if ((int32_t)(ack - expected_ack) >= 0) {
            return 0;
        }
        if (flags & 0x01) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
    }
    return -1;
}

int socket_send_fin_close(struct socket_object* sock) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start;
    uint64_t timeout_ticks;
    uint32_t frequency = timer_get_frequency();
    uint32_t fin_seq;

    if (!sock || sock->state != SOCKET_STATE_CONNECTED) {
        return -1;
    }

    fin_seq = sock->seq;
    {
        uint16_t len = sock_build_tcp_frame(sock, 0x11, NULL, 0, tx);
        if (netdev_send(sock->dev_index, tx, len) < 0) {
            return -1;
        }
    }
    sock->seq++;

    if (frequency == 0) frequency = 100;
    timeout_ticks = (uint64_t)frequency * 2ULL;
    start = timer_get_ticks();
    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        if (payload_len > 0) {
            if (socket_store_rx_payload(sock, payload, payload_len, seq, (flags & 0x01) != 0) &&
                (flags & 0x01) != 0) {
                sock->state = SOCKET_STATE_CLOSED;
                return 0;
            }
            continue;
        }
        if ((flags & 0x01) != 0) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            return 0;
        }
        if ((flags & 0x10) != 0 && ack == fin_seq + 1) {
            sock->state = SOCKET_STATE_CLOSED;
            return 0;
        }
        if ((flags & 0x04) != 0) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
    }

    sock->state = SOCKET_STATE_CLOSED;
    return -1;
}

int64_t sys_socket(struct syscall_regs* regs) {
    return install_socket_fd((int)regs->rdi, (int)regs->rsi, (int)regs->rdx);
}

int64_t sys_socket_bind_netdev(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    uint64_t index = regs->rsi;
    const struct netdev* dev;

    if (!sock) return -(int64_t)LINUX_EBADF;
    if (index > 255U) return -(int64_t)LINUX_EINVAL;
    dev = netdev_get((size_t)index);
    if (!dev) return -(int64_t)LINUX_ENODEV;
    if (sock->state != SOCKET_STATE_CREATED) return -(int64_t)LINUX_EINVAL;
    sock->dev_index = (uint8_t)index;
    return 0;
}

int64_t sys_connect(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    const struct linux_sockaddr_in* addr = (const struct linux_sockaddr_in*)(uintptr_t)regs->rsi;
    const struct linux_sockaddr_in6* addr6 = (const struct linux_sockaddr_in6*)(uintptr_t)regs->rsi;
    uint64_t addr_len = regs->rdx;
    const struct netdev* dev;
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t flags = 0;
    uint32_t reply_seq = 0;
    uint32_t reply_ack = 0;
    const uint8_t* payload = NULL;
    uint16_t payload_len = 0;

    if (!sock) return -(int64_t)LINUX_EBADF;
    if (!addr) return -(int64_t)LINUX_EFAULT;
    dev = netdev_get(sock->dev_index);
    if (!dev || !dev->link_up) return -(int64_t)LINUX_ENODEV;

    if (sock->family == LINUX_AF_INET6) {
        if (addr_len < sizeof(*addr6)) return -(int64_t)LINUX_EFAULT;
        if (addr6->sin6_family != LINUX_AF_INET6) return -(int64_t)LINUX_EINVAL;
        if (!dev->ipv6_configured) return -(int64_t)LINUX_ENODEV;

        local_memcpy(sock->remote_ip6, addr6->sin6_addr, 16);
        sock->remote_port = sock_get_be16((const uint8_t*)&addr6->sin6_port);
        if (sock_ipv6_is_link_local(sock->remote_ip6) ||
            sock_ipv6_same_prefix(sock->remote_ip6, dev->ipv6_addr, dev->ipv6_prefix)) {
            local_memcpy(sock->next_hop_ip6, sock->remote_ip6, 16);
        } else {
            if (sock_ipv6_is_zero(dev->ipv6_gateway)) {
                sock->state = SOCKET_STATE_CLOSED;
                serial_print("socket: no IPv6 gateway for off-link connect\n");
                return -(int64_t)LINUX_EIO;
            }
            local_memcpy(sock->next_hop_ip6, dev->ipv6_gateway, 16);
        }
        if (sock_ndp_lookup(sock, dev) != 0) {
            sock->state = SOCKET_STATE_CLOSED;
            serial_print("socket: NDP failed\n");
            return -(int64_t)LINUX_EIO;
        }
    } else {
        if (addr_len < sizeof(*addr)) return -(int64_t)LINUX_EFAULT;
        if (addr->sin_family != LINUX_AF_INET) return -(int64_t)LINUX_EINVAL;
        if (!dev->ipv4_configured) return -(int64_t)LINUX_ENODEV;

        local_memcpy(sock->remote_ip, addr->sin_addr, 4);
        sock->remote_port = sock_get_be16((const uint8_t*)&addr->sin_port);
        if (sock_ipv4_same_subnet(sock->remote_ip, dev->ipv4_addr, dev->ipv4_prefix)) {
            local_memcpy(sock->next_hop_ip, sock->remote_ip, 4);
        } else {
            if (sock_ipv4_is_zero(dev->ipv4_gateway)) {
                sock->state = SOCKET_STATE_CLOSED;
                serial_print("socket: no IPv4 gateway for off-link connect\n");
                return -(int64_t)LINUX_EIO;
            }
            local_memcpy(sock->next_hop_ip, dev->ipv4_gateway, 4);
        }
        if (sock_arp_lookup(sock, dev) != 0) {
            sock->state = SOCKET_STATE_CLOSED;
            serial_print("socket: ARP failed\n");
            return -(int64_t)LINUX_EIO;
        }
    }

    sock->ack = 0;
    for (int attempt = 0; attempt < SOCKET_TCP_RETRIES; attempt++) {
        uint16_t len = sock_build_tcp_frame(sock, 0x02, NULL, 0, tx);
        if (attempt > 0) {
            sock->retransmits++;
        }
        if (netdev_send(sock->dev_index, tx, len) < 0) {
            sock->state = SOCKET_STATE_CLOSED;
            serial_print("socket: SYN send failed\n");
            return -(int64_t)LINUX_EIO;
        }
        if (sock_wait_tcp_ticks(sock,
                                0x12,
                                sock_retry_timeout_ticks(attempt),
                                &flags,
                                &reply_seq,
                                &reply_ack,
                                &payload,
                                &payload_len) == 0) {
            break;
        }
        flags = 0;
    }
    if (flags & 0x04) {
        sock->state = SOCKET_STATE_CLOSED;
        serial_print("socket: TCP reset during connect\n");
        return -(int64_t)LINUX_EIO;
    }
    if ((flags & 0x12) != 0x12) {
        sock->state = SOCKET_STATE_CLOSED;
        serial_print("socket: SYN-ACK wait failed\n");
        return -(int64_t)LINUX_EIO;
    }
    (void)payload;
    (void)payload_len;
    if ((flags & 0x12) != 0x12 || reply_ack != sock->seq + 1) {
        sock->state = SOCKET_STATE_CLOSED;
        serial_print("socket: unexpected TCP flags\n");
        return -(int64_t)LINUX_EIO;
    }
    sock->seq++;
    sock->ack = reply_seq + 1;
    sock_init_congestion(sock);
    socket_send_ack(sock);
    sock->state = SOCKET_STATE_CONNECTED;
    return 0;
}

int64_t socket_send_data(struct socket_object* sock, const uint8_t* data, uint64_t len) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint64_t sent = 0;

    if (!sock || sock->state != SOCKET_STATE_CONNECTED) return -(int64_t)LINUX_EBADF;
    if (!data && len > 0) return -(int64_t)LINUX_EFAULT;
    while (sent < len) {
        uint16_t chunk = (uint16_t)(len - sent);
        uint32_t seq_before;
        uint32_t expected_ack;
        int acked = 0;
        if (sock->peer_window_bytes == 0 &&
            sock_wait_for_send_window(sock, sock_timeout_ticks(SOCKET_TCP_WINDOW_WAIT_SECONDS)) != 0) {
            return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
        }
        if (chunk > sock_effective_mss(sock)) chunk = sock_effective_mss(sock);
        if (sock->cwnd_bytes > 0 && chunk > sock->cwnd_bytes) {
            chunk = (uint16_t)sock->cwnd_bytes;
        }
        if (sock->peer_window_bytes > 0 && chunk > sock->peer_window_bytes) {
            chunk = (uint16_t)sock->peer_window_bytes;
        }
        if (chunk == 0) {
            return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
        }
        seq_before = sock->seq;
        expected_ack = seq_before + chunk;
        for (int attempt = 0; attempt < SOCKET_TCP_RETRIES; attempt++) {
            uint16_t frame_len = sock_build_tcp_frame(sock, 0x18, data + sent, chunk, tx);
            if (attempt > 0) {
                sock->retransmits++;
            }
            if (netdev_send(sock->dev_index, tx, frame_len) < 0) {
                return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
            }
            if (sock_wait_ack(sock,
                              expected_ack,
                              sock_retry_timeout_ticks(attempt)) == 0) {
                acked = 1;
                break;
            }
            if (sock->state == SOCKET_STATE_CLOSED) {
                return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
            }
        }
        if (!acked) {
            sock_note_retransmit_timeout(sock);
            sock->seq = seq_before;
            return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
        }
        sock_note_ack(sock, chunk);
        sock->seq = expected_ack;
        sent += chunk;
    }
    return (int64_t)sent;
}

int64_t socket_recv_data(struct socket_object* sock, uint8_t* dst, uint64_t len) {
    const uint8_t* payload = NULL;
    uint16_t payload_len = 0;
    uint8_t flags = 0;
    uint32_t seq = 0;
    uint32_t ack = 0;
    uint64_t copied = 0;
    int recv_retries = 0;

    if (!sock) return -(int64_t)LINUX_EBADF;
    if (sock->state == SOCKET_STATE_CLOSED && sock->rx_off >= sock->rx_len) return 0;
    if (sock->state != SOCKET_STATE_CONNECTED && sock->state != SOCKET_STATE_CLOSED) return -(int64_t)LINUX_EBADF;
    if (!dst && len > 0) return -(int64_t)LINUX_EFAULT;
    if (len == 0) return 0;

    while (copied < len) {
        if (sock->rx_off < sock->rx_len) {
            uint64_t avail = sock->rx_len - sock->rx_off;
            if (avail > len - copied) avail = len - copied;
            local_memcpy(dst + copied, sock->rx_buffer + sock->rx_off, avail);
            sock->rx_off += (uint32_t)avail;
            copied += avail;
            if (copied > 0) break;
            continue;
        }

        sock->rx_off = 0;
        sock->rx_len = 0;
        if (sock_wait_tcp(sock, 0x10, &flags, &seq, &ack, &payload, &payload_len) != 0) {
            if (sock->state == SOCKET_STATE_CONNECTED && recv_retries < SOCKET_TCP_RECV_RETRIES) {
                recv_retries++;
                (void)socket_send_ack(sock);
                continue;
            }
            break;
        }
        recv_retries = 0;
        (void)ack;
        if (flags & 0x04) {
            sock->state = SOCKET_STATE_CLOSED;
            break;
        }
        if (payload_len > 0 && (flags & 0x01)) {
            if (socket_store_rx_payload(sock, payload, payload_len, seq, 1)) {
                sock->state = SOCKET_STATE_CLOSED;
            }
            continue;
        }
        if (payload_len > 0) {
            socket_store_rx_payload(sock, payload, payload_len, seq, 0);
            continue;
        }
        if (flags & 0x01) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            break;
        }
    }
    if (copied == 0 && sock->state == SOCKET_STATE_CONNECTED) {
        return -(int64_t)LINUX_EIO;
    }
    return (int64_t)copied;
}

int64_t sys_sendto(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    const uint8_t* buf = (const uint8_t*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;
    (void)regs->r10;
    (void)regs->r8;
    (void)regs->r9;
    return socket_send_data(sock, buf, len);
}

int64_t sys_recvfrom(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;
    (void)regs->r10;
    (void)regs->r8;
    (void)regs->r9;
    return socket_recv_data(sock, buf, len);
}

int64_t sys_dns_lookup(struct syscall_regs* regs) {
    const char* user_name = (const char*)(uintptr_t)regs->rdi;
    uint8_t* out_ip = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t requested_dev = regs->rdx;
    const struct netdev* dev;
    struct socket_object dns_sock;
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint8_t answer[4];
    char name[SOCKET_DNS_NAME_MAX];
    char current[SOCKET_DNS_NAME_MAX];
    char cname[SOCKET_DNS_NAME_MAX];

    if (!user_name || !out_ip) return -(int64_t)LINUX_EFAULT;
    if (requested_dev > 255U) return -(int64_t)LINUX_EINVAL;
    dev = netdev_get((size_t)requested_dev);
    if (!dev || !dev->link_up || !dev->ipv4_configured) return -(int64_t)LINUX_ENODEV;
    if (dev->ipv4_dns[0] == 0 && dev->ipv4_dns[1] == 0 &&
        dev->ipv4_dns[2] == 0 && dev->ipv4_dns[3] == 0) {
        return -(int64_t)LINUX_ENODEV;
    }
    if (sock_copy_user_name(name, sizeof(name), user_name) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (dns_cache_lookup((uint8_t)requested_dev, name, 4, answer)) {
        local_memcpy(out_ip, answer, 4);
        return 0;
    }
    local_memcpy(current, name, sizeof(current));

    local_memset(&dns_sock, 0, sizeof(dns_sock));
    dns_sock.dev_index = (uint8_t)requested_dev;
    if (sock_ipv4_same_subnet(dev->ipv4_dns, dev->ipv4_addr, dev->ipv4_prefix)) {
        local_memcpy(dns_sock.next_hop_ip, dev->ipv4_dns, 4);
    } else {
        local_memcpy(dns_sock.next_hop_ip, dev->ipv4_gateway, 4);
    }
    if (sock_arp_lookup(&dns_sock, dev) != 0) {
        return -(int64_t)LINUX_EIO;
    }

    for (int depth = 0; depth < 3; depth++) {
        uint16_t query_len;
        int followed_cname = 0;

        cname[0] = '\0';
        query_len = sock_build_dns_query(dev, dns_sock.remote_mac, current, 1, tx);
        if (query_len == 0) return -(int64_t)LINUX_EINVAL;

        for (int attempt = 0; attempt < SOCKET_DNS_RETRIES; attempt++) {
            if (netdev_send((size_t)requested_dev, tx, query_len) < 0) {
                return -(int64_t)LINUX_EIO;
            }

            uint64_t start = timer_get_ticks();
            uint64_t timeout = sock_timeout_ticks(SOCKET_DNS_TIMEOUT_SECONDS);
            while (timer_get_ticks() - start < timeout) {
                int parsed;
                int n = netdev_recv((size_t)requested_dev, rx, sizeof(rx));
                if (n < 0) return -(int64_t)LINUX_EIO;
                parsed = sock_parse_dns_response(dev, rx, n, 1, answer, cname);
                if (parsed == 1) {
                    dns_cache_store((uint8_t)requested_dev, name, 4, answer);
                    local_memcpy(out_ip, answer, 4);
                    return 0;
                }
                if (parsed == 2) {
                    local_memcpy(current, cname, sizeof(current));
                    followed_cname = 1;
                    break;
                }
                if (parsed < 0) {
                    return -(int64_t)LINUX_ENOENT;
                }
            }
            if (followed_cname) break;
        }
        if (!followed_cname) break;
    }

    return -(int64_t)LINUX_ENOENT;
}

int64_t sys_dns_lookup6(struct syscall_regs* regs) {
    const char* user_name = (const char*)(uintptr_t)regs->rdi;
    uint8_t* out_ip = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t requested_dev = regs->rdx;
    const struct netdev* dev;
    struct socket_object dns_sock;
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint8_t answer[16];
    char name[SOCKET_DNS_NAME_MAX];
    char current[SOCKET_DNS_NAME_MAX];
    char cname[SOCKET_DNS_NAME_MAX];

    if (!user_name || !out_ip) return -(int64_t)LINUX_EFAULT;
    if (requested_dev > 255U) return -(int64_t)LINUX_EINVAL;
    dev = netdev_get((size_t)requested_dev);
    if (!dev || !dev->link_up) return -(int64_t)LINUX_ENODEV;
    if (sock_copy_user_name(name, sizeof(name), user_name) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (dns_cache_lookup((uint8_t)requested_dev, name, 6, answer)) {
        local_memcpy(out_ip, answer, 16);
        return 0;
    }
    local_memcpy(current, name, sizeof(current));

    if (dev->ipv6_configured && !sock_ipv6_is_zero(dev->ipv6_dns)) {
        local_memset(&dns_sock, 0, sizeof(dns_sock));
        dns_sock.dev_index = (uint8_t)requested_dev;
        if (sock_ipv6_is_link_local(dev->ipv6_dns) ||
            sock_ipv6_same_prefix(dev->ipv6_dns, dev->ipv6_addr, dev->ipv6_prefix)) {
            local_memcpy(dns_sock.next_hop_ip6, dev->ipv6_dns, 16);
        } else if (!sock_ipv6_is_zero(dev->ipv6_gateway)) {
            local_memcpy(dns_sock.next_hop_ip6, dev->ipv6_gateway, 16);
        }

        if (!sock_ipv6_is_zero(dns_sock.next_hop_ip6) &&
            sock_ndp_lookup(&dns_sock, dev) == 0) {
            for (int depth = 0; depth < 3; depth++) {
                uint16_t query_len;
                int followed_cname = 0;

                cname[0] = '\0';
                query_len = sock_build_dns_query6(dev, dns_sock.remote_mac, current, 28, tx);
                if (query_len == 0) return -(int64_t)LINUX_EINVAL;

                for (int attempt = 0; attempt < SOCKET_DNS_RETRIES; attempt++) {
                    if (netdev_send((size_t)requested_dev, tx, query_len) < 0) {
                        return -(int64_t)LINUX_EIO;
                    }

                    uint64_t start = timer_get_ticks();
                    uint64_t timeout = sock_timeout_ticks(SOCKET_DNS_TIMEOUT_SECONDS);
                    while (timer_get_ticks() - start < timeout) {
                        int parsed;
                        int n = netdev_recv((size_t)requested_dev, rx, sizeof(rx));
                        if (n < 0) return -(int64_t)LINUX_EIO;
                        parsed = sock_parse_dns_response6(dev, rx, n, 28, answer, cname);
                        if (parsed == 1) {
                            dns_cache_store((uint8_t)requested_dev, name, 6, answer);
                            local_memcpy(out_ip, answer, 16);
                            return 0;
                        }
                        if (parsed == 2) {
                            local_memcpy(current, cname, sizeof(current));
                            followed_cname = 1;
                            break;
                        }
                        if (parsed < 0) {
                            return -(int64_t)LINUX_ENOENT;
                        }
                    }
                    if (followed_cname) break;
                }
                if (!followed_cname) break;
            }
            local_memcpy(current, name, sizeof(current));
        }
    }

    if (!dev->ipv4_configured ||
        (dev->ipv4_dns[0] == 0 && dev->ipv4_dns[1] == 0 &&
         dev->ipv4_dns[2] == 0 && dev->ipv4_dns[3] == 0)) {
        return -(int64_t)LINUX_ENODEV;
    }

    local_memset(&dns_sock, 0, sizeof(dns_sock));
    dns_sock.dev_index = (uint8_t)requested_dev;
    if (sock_ipv4_same_subnet(dev->ipv4_dns, dev->ipv4_addr, dev->ipv4_prefix)) {
        local_memcpy(dns_sock.next_hop_ip, dev->ipv4_dns, 4);
    } else {
        local_memcpy(dns_sock.next_hop_ip, dev->ipv4_gateway, 4);
    }
    if (sock_arp_lookup(&dns_sock, dev) != 0) {
        return -(int64_t)LINUX_EIO;
    }

    for (int depth = 0; depth < 3; depth++) {
        uint16_t query_len;
        int followed_cname = 0;

        cname[0] = '\0';
        query_len = sock_build_dns_query(dev, dns_sock.remote_mac, current, 28, tx);
        if (query_len == 0) return -(int64_t)LINUX_EINVAL;

        for (int attempt = 0; attempt < SOCKET_DNS_RETRIES; attempt++) {
            if (netdev_send((size_t)requested_dev, tx, query_len) < 0) {
                return -(int64_t)LINUX_EIO;
            }

            uint64_t start = timer_get_ticks();
            uint64_t timeout = sock_timeout_ticks(SOCKET_DNS_TIMEOUT_SECONDS);
            while (timer_get_ticks() - start < timeout) {
                int parsed;
                int n = netdev_recv((size_t)requested_dev, rx, sizeof(rx));
                if (n < 0) return -(int64_t)LINUX_EIO;
                parsed = sock_parse_dns_response(dev, rx, n, 28, answer, cname);
                if (parsed == 1) {
                    dns_cache_store((uint8_t)requested_dev, name, 6, answer);
                    local_memcpy(out_ip, answer, 16);
                    return 0;
                }
                if (parsed == 2) {
                    local_memcpy(current, cname, sizeof(current));
                    followed_cname = 1;
                    break;
                }
                if (parsed < 0) {
                    return -(int64_t)LINUX_ENOENT;
                }
            }
            if (followed_cname) break;
        }
        if (!followed_cname) break;
    }

    return -(int64_t)LINUX_ENOENT;
}
