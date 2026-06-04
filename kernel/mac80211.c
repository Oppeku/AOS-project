/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <mac80211.h>
#include <wifi.h>
#include <stddef.h>
#include <stdint.h>

#define WLAN_EID_SSID 0U
#define WLAN_EID_DS_PARAMS 3U
#define WLAN_EID_RSN 48U
#define WLAN_CAPABILITY_PRIVACY 0x0010U
#define WLAN_STATUS_SUCCESS 0U
#define IEEE80211_FC_TODS 0x0100U
#define ETH_HEADER_LEN 14U
#define LLC_SNAP_LEN 8U
#define IPV4_PROTO_ICMP 1U
#define IPV4_PROTO_TCP 6U
#define IPV4_PROTO_UDP 17U
#define DNS_PORT 53U
#define HTTP_PORT 80U
#define SIM_TCP_SEQ 0xa0053000U
#define SIM_HTTP_BODY_LEN 1024U
#define SIM_HTTP_CHUNK_SIZE 384U
#define DHCP_CLIENT_PORT 68U
#define DHCP_SERVER_PORT 67U
#define DHCP_BOOTREQUEST 1U
#define DHCP_BOOTREPLY 2U
#define DHCP_OPT_MSG_TYPE 53U
#define DHCP_OPT_SUBNET_MASK 1U
#define DHCP_OPT_ROUTER 3U
#define DHCP_OPT_DNS 6U
#define DHCP_OPT_LEASE_TIME 51U
#define DHCP_OPT_SERVER_ID 54U
#define DHCP_DISCOVER 1U
#define DHCP_OFFER 2U
#define DHCP_REQUEST 3U
#define DHCP_ACK 5U

static struct mac80211_scan_result g_scan_results[MAC80211_SCAN_MAX_RESULTS];
static uint32_t g_scan_count;
static struct mac80211_state g_state;
static const uint8_t g_station_mac[6] = {0xa0, 0x05, 0x00, 0x00, 0x00, 0x02};
static const uint8_t g_broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t g_sim_ap_ipv4[4] = {10, 0, 3, 1};
static const uint8_t g_sim_station_ipv4[4] = {10, 0, 3, 15};
static const uint8_t g_sim_mask_ipv4[4] = {255, 255, 255, 0};
static const uint8_t g_sim_broadcast_ipv4[4] = {255, 255, 255, 255};
static struct {
    uint8_t active;
    uint16_t client_port;
    uint32_t client_next_seq;
    uint32_t sent;
} g_sim_http;
static const uint8_t g_aos_lab_probe_resp[] = {
    0x50, 0x00, 0x00, 0x00,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x02,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x10, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00,
    0x11, 0x04,
    WLAN_EID_SSID, 0x07, 'A', 'O', 'S', '-', 'L', 'a', 'b',
    WLAN_EID_DS_PARAMS, 0x01, 0x06,
    WLAN_EID_RSN, 0x02, 0x01, 0x00,
};
static const uint8_t g_aos_lab_auth_resp[] = {
    0xb0, 0x00, 0x00, 0x00,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x02,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x20, 0x00,
    0x00, 0x00,
    0x02, 0x00,
    0x00, 0x00,
};
static const uint8_t g_aos_lab_assoc_resp[] = {
    0x10, 0x00, 0x00, 0x00,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x02,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
    0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x30, 0x00,
    0x11, 0x04,
    0x00, 0x00,
    0x01, 0xc0,
};

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void copy_ssid(char* dst, const char* src) {
    size_t i = 0;

    if (!dst) {
        return;
    }

    while (src && src[i] && i + 1 < MAC80211_SSID_MAX) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void copy_bssid(uint8_t dst[6], const uint8_t src[6]) {
    for (size_t i = 0; i < 6; i++) {
        dst[i] = src ? src[i] : 0;
    }
}

static void local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
}

static uint16_t get_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
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

static uint16_t checksum_finish(uint32_t sum) {
    while (sum >> 16U) {
        sum = (sum & 0xffffU) + (sum >> 16U);
    }
    return (uint16_t)~sum;
}

static uint16_t ipv4_checksum(const uint8_t* data, uint16_t len) {
    uint32_t sum = 0;

    while (len > 1U) {
        sum += (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)((uint16_t)data[0] << 8);
    }
    return checksum_finish(sum);
}

static uint32_t checksum_add(uint32_t sum, const uint8_t* data, uint16_t len) {
    while (len > 1U) {
        sum += (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)((uint16_t)data[0] << 8);
    }
    return sum;
}

static uint16_t tcp_checksum(const uint8_t src_ip[4],
                             const uint8_t dst_ip[4],
                             const uint8_t* tcp,
                             uint16_t tcp_len) {
    uint32_t sum = 0;

    sum = checksum_add(sum, src_ip, 4U);
    sum = checksum_add(sum, dst_ip, 4U);
    sum += IPV4_PROTO_TCP;
    sum += tcp_len;
    sum = checksum_add(sum, tcp, tcp_len);
    return checksum_finish(sum);
}

static int same_bytes(const uint8_t* a, const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static uint8_t ssid_len(const char* ssid) {
    uint8_t len = 0;

    while (ssid && ssid[len] && len < MAC80211_SSID_MAX) {
        len++;
    }
    return len;
}

static int bssid_equals(const uint8_t a[6], const uint8_t b[6]) {
    for (size_t i = 0; i < 6; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static void copy_ssid_bytes(char* dst, const uint8_t* src, uint8_t len) {
    uint8_t n = len;

    if (n >= MAC80211_SSID_MAX) {
        n = MAC80211_SSID_MAX - 1;
    }

    for (uint8_t i = 0; i < n; i++) {
        dst[i] = (char)src[i];
    }
    dst[n] = '\0';
}

static int parse_dhcp_message_type(const uint8_t* bootp, uint16_t bootp_len, uint8_t* msg_type) {
    uint16_t pos = 240U;

    if (!bootp || !msg_type || bootp_len < 240U ||
        bootp[236] != 99U || bootp[237] != 130U ||
        bootp[238] != 83U || bootp[239] != 99U) {
        return -1;
    }

    while (pos < bootp_len) {
        uint8_t code = bootp[pos++];
        uint8_t len;

        if (code == 0U) {
            continue;
        }
        if (code == 255U) {
            break;
        }
        if (pos >= bootp_len) {
            break;
        }

        len = bootp[pos++];
        if ((uint16_t)(pos + len) > bootp_len) {
            break;
        }
        if (code == DHCP_OPT_MSG_TYPE && len >= 1U) {
            *msg_type = bootp[pos];
            return 0;
        }
        pos = (uint16_t)(pos + len);
    }

    return -1;
}

static int build_sim_dhcp_reply(const uint8_t* request,
                                uint16_t request_len,
                                uint8_t* out,
                                uint32_t out_len) {
    const uint8_t* req_ip;
    const uint8_t* req_udp;
    const uint8_t* req_bootp;
    uint16_t req_ihl;
    uint16_t req_udp_len;
    uint16_t req_bootp_len;
    uint8_t msg_type = 0;
    uint8_t reply_type;
    uint8_t* eth;
    uint8_t* ip;
    uint8_t* udp;
    uint8_t* bootp;
    uint8_t* opt;
    uint16_t bootp_len;
    uint16_t udp_len;
    uint16_t ip_len;
    uint16_t frame_len;

    if (!request || !out ||
        request_len < ETH_HEADER_LEN + 20U + 8U + 240U ||
        out_len < ETH_HEADER_LEN + 20U + 8U + 280U ||
        request[12] != 0x08U || request[13] != 0x00U) {
        return 0;
    }

    req_ip = request + ETH_HEADER_LEN;
    if ((req_ip[0] >> 4U) != 4U || req_ip[9] != IPV4_PROTO_UDP) {
        return 0;
    }

    req_ihl = (uint16_t)((req_ip[0] & 0x0fU) * 4U);
    if (req_ihl < 20U || request_len < ETH_HEADER_LEN + req_ihl + 8U + 240U) {
        return 0;
    }

    req_udp = req_ip + req_ihl;
    if (get_be16(req_udp) != DHCP_CLIENT_PORT ||
        get_be16(req_udp + 2U) != DHCP_SERVER_PORT) {
        return 0;
    }

    req_udp_len = get_be16(req_udp + 4U);
    if (req_udp_len < 248U) {
        return 0;
    }

    req_bootp = req_udp + 8U;
    req_bootp_len = (uint16_t)(req_udp_len - 8U);
    if (req_bootp[0] != DHCP_BOOTREQUEST ||
        parse_dhcp_message_type(req_bootp, req_bootp_len, &msg_type) != 0) {
        return 0;
    }

    if (msg_type == DHCP_DISCOVER) {
        reply_type = DHCP_OFFER;
    } else if (msg_type == DHCP_REQUEST) {
        reply_type = DHCP_ACK;
    } else {
        return 0;
    }

    local_memset(out, 0, out_len);
    eth = out;
    ip = out + ETH_HEADER_LEN;
    udp = ip + 20U;
    bootp = udp + 8U;

    local_memcpy(eth, request + 6U, 6U);
    local_memcpy(eth + 6U, g_state.bssid, 6U);
    eth[12] = 0x08U;
    eth[13] = 0x00U;

    ip[0] = 0x45U;
    ip[8] = 64U;
    ip[9] = IPV4_PROTO_UDP;
    local_memcpy(ip + 12U, g_sim_ap_ipv4, 4U);
    local_memcpy(ip + 16U, g_sim_broadcast_ipv4, 4U);

    put_be16(udp, DHCP_SERVER_PORT);
    put_be16(udp + 2U, DHCP_CLIENT_PORT);

    bootp[0] = DHCP_BOOTREPLY;
    bootp[1] = 1U;
    bootp[2] = 6U;
    local_memcpy(bootp + 4U, req_bootp + 4U, 4U);
    local_memcpy(bootp + 10U, req_bootp + 10U, 2U);
    local_memcpy(bootp + 16U, g_sim_station_ipv4, 4U);
    local_memcpy(bootp + 20U, g_sim_ap_ipv4, 4U);
    local_memcpy(bootp + 28U, req_bootp + 28U, 16U);
    bootp[236] = 99U;
    bootp[237] = 130U;
    bootp[238] = 83U;
    bootp[239] = 99U;

    opt = bootp + 240U;
    *opt++ = DHCP_OPT_MSG_TYPE;
    *opt++ = 1U;
    *opt++ = reply_type;
    *opt++ = DHCP_OPT_SERVER_ID;
    *opt++ = 4U;
    local_memcpy(opt, g_sim_ap_ipv4, 4U);
    opt += 4U;
    *opt++ = DHCP_OPT_SUBNET_MASK;
    *opt++ = 4U;
    local_memcpy(opt, g_sim_mask_ipv4, 4U);
    opt += 4U;
    *opt++ = DHCP_OPT_ROUTER;
    *opt++ = 4U;
    local_memcpy(opt, g_sim_ap_ipv4, 4U);
    opt += 4U;
    *opt++ = DHCP_OPT_DNS;
    *opt++ = 4U;
    local_memcpy(opt, g_sim_ap_ipv4, 4U);
    opt += 4U;
    *opt++ = DHCP_OPT_LEASE_TIME;
    *opt++ = 4U;
    put_be32(opt, 3600U);
    opt += 4U;
    *opt++ = 255U;

    bootp_len = (uint16_t)(opt - bootp);
    udp_len = (uint16_t)(8U + bootp_len);
    ip_len = (uint16_t)(20U + udp_len);
    frame_len = (uint16_t)(ETH_HEADER_LEN + ip_len);

    put_be16(ip + 2U, ip_len);
    put_be16(ip + 4U, 0xa005U);
    put_be16(udp + 4U, udp_len);
    put_be16(ip + 10U, ipv4_checksum(ip, 20U));
    return (int)frame_len;
}

static int build_sim_dns_reply(const uint8_t* request,
                               uint16_t request_len,
                               uint8_t* out,
                               uint32_t out_len) {
    const uint8_t* req_ip;
    const uint8_t* req_udp;
    const uint8_t* req_dns;
    const uint8_t* question;
    uint16_t req_ihl;
    uint16_t req_udp_len;
    uint16_t req_dns_len;
    uint16_t qname_len = 0;
    uint16_t question_len;
    uint8_t* eth;
    uint8_t* ip;
    uint8_t* udp;
    uint8_t* dns;
    uint8_t* answer;
    uint16_t dns_len;
    uint16_t udp_len;
    uint16_t ip_len;
    uint16_t frame_len;

    if (!request || !out ||
        request_len < ETH_HEADER_LEN + 20U + 8U + 17U ||
        out_len < 128U ||
        request[12] != 0x08U || request[13] != 0x00U) {
        return 0;
    }

    req_ip = request + ETH_HEADER_LEN;
    if ((req_ip[0] >> 4U) != 4U || req_ip[9] != IPV4_PROTO_UDP ||
        !same_bytes(req_ip + 16U, g_sim_ap_ipv4, 4U)) {
        return 0;
    }

    req_ihl = (uint16_t)((req_ip[0] & 0x0fU) * 4U);
    if (req_ihl < 20U || request_len < ETH_HEADER_LEN + req_ihl + 8U + 17U) {
        return 0;
    }

    req_udp = req_ip + req_ihl;
    if (get_be16(req_udp + 2U) != DNS_PORT) {
        return 0;
    }

    req_udp_len = get_be16(req_udp + 4U);
    if (req_udp_len < 20U || request_len < ETH_HEADER_LEN + req_ihl + req_udp_len) {
        return 0;
    }

    req_dns = req_udp + 8U;
    req_dns_len = (uint16_t)(req_udp_len - 8U);
    if (req_dns_len < 17U || get_be16(req_dns + 4U) != 1U) {
        return 0;
    }

    question = req_dns + 12U;
    while (qname_len < (uint16_t)(req_dns_len - 12U)) {
        uint8_t label_len = question[qname_len++];
        if (label_len == 0U) {
            break;
        }
        if ((uint16_t)(qname_len + label_len) > (uint16_t)(req_dns_len - 12U)) {
            return 0;
        }
        qname_len = (uint16_t)(qname_len + label_len);
    }
    if (qname_len == 0U || qname_len + 4U > (uint16_t)(req_dns_len - 12U) ||
        get_be16(question + qname_len) != 1U ||
        get_be16(question + qname_len + 2U) != 1U) {
        return 0;
    }

    question_len = (uint16_t)(qname_len + 4U);
    dns_len = (uint16_t)(12U + question_len + 16U);
    udp_len = (uint16_t)(8U + dns_len);
    ip_len = (uint16_t)(20U + udp_len);
    frame_len = (uint16_t)(ETH_HEADER_LEN + ip_len);
    if (out_len < frame_len) {
        return 0;
    }

    local_memset(out, 0, out_len);
    eth = out;
    ip = out + ETH_HEADER_LEN;
    udp = ip + 20U;
    dns = udp + 8U;

    local_memcpy(eth, request + 6U, 6U);
    local_memcpy(eth + 6U, g_state.bssid, 6U);
    eth[12] = 0x08U;
    eth[13] = 0x00U;

    ip[0] = 0x45U;
    ip[8] = 64U;
    ip[9] = IPV4_PROTO_UDP;
    local_memcpy(ip + 12U, g_sim_ap_ipv4, 4U);
    local_memcpy(ip + 16U, req_ip + 12U, 4U);
    put_be16(ip + 2U, ip_len);
    put_be16(ip + 4U, 0xa007U);
    put_be16(ip + 10U, ipv4_checksum(ip, 20U));

    put_be16(udp, DNS_PORT);
    put_be16(udp + 2U, get_be16(req_udp));
    put_be16(udp + 4U, udp_len);

    local_memcpy(dns, req_dns, 2U);
    put_be16(dns + 2U, 0x8180U);
    put_be16(dns + 4U, 1U);
    put_be16(dns + 6U, 1U);
    put_be16(dns + 8U, 0U);
    put_be16(dns + 10U, 0U);
    local_memcpy(dns + 12U, question, question_len);

    answer = dns + 12U + question_len;
    answer[0] = 0xc0U;
    answer[1] = 0x0cU;
    put_be16(answer + 2U, 1U);
    put_be16(answer + 4U, 1U);
    put_be32(answer + 6U, 60U);
    put_be16(answer + 10U, 4U);
    local_memcpy(answer + 12U, g_sim_ap_ipv4, 4U);
    return frame_len < 60U ? 60 : (int)frame_len;
}

static int build_sim_arp_reply(const uint8_t* request,
                               uint16_t request_len,
                               uint8_t* out,
                               uint32_t out_len) {
    uint8_t* arp;

    if (!request || !out || request_len < 42U || out_len < 60U ||
        request[12] != 0x08U || request[13] != 0x06U ||
        get_be16(request + 14U) != 1U ||
        get_be16(request + 16U) != 0x0800U ||
        request[18] != 6U || request[19] != 4U ||
        get_be16(request + 20U) != 1U ||
        !same_bytes(request + 38U, g_sim_ap_ipv4, 4U)) {
        return 0;
    }

    local_memset(out, 0, out_len);
    local_memcpy(out, request + 6U, 6U);
    local_memcpy(out + 6U, g_state.bssid, 6U);
    out[12] = 0x08U;
    out[13] = 0x06U;

    arp = out + ETH_HEADER_LEN;
    put_be16(arp + 0U, 1U);
    put_be16(arp + 2U, 0x0800U);
    arp[4] = 6U;
    arp[5] = 4U;
    put_be16(arp + 6U, 2U);
    local_memcpy(arp + 8U, g_state.bssid, 6U);
    local_memcpy(arp + 14U, g_sim_ap_ipv4, 4U);
    local_memcpy(arp + 18U, request + 6U, 6U);
    local_memcpy(arp + 24U, request + 28U, 4U);
    return 60;
}

static int build_sim_icmp_echo_reply(const uint8_t* request,
                                     uint16_t request_len,
                                     uint8_t* out,
                                     uint32_t out_len) {
    const uint8_t* req_ip;
    const uint8_t* req_icmp;
    uint8_t* ip;
    uint8_t* icmp;
    uint16_t req_ihl;
    uint16_t req_ip_len;
    uint16_t icmp_len;
    uint16_t frame_len;

    if (!request || !out || request_len < ETH_HEADER_LEN + 20U + 8U ||
        out_len < 60U ||
        request[12] != 0x08U || request[13] != 0x00U) {
        return 0;
    }

    req_ip = request + ETH_HEADER_LEN;
    if ((req_ip[0] >> 4U) != 4U || req_ip[9] != IPV4_PROTO_ICMP ||
        !same_bytes(req_ip + 16U, g_sim_ap_ipv4, 4U)) {
        return 0;
    }

    req_ihl = (uint16_t)((req_ip[0] & 0x0fU) * 4U);
    req_ip_len = get_be16(req_ip + 2U);
    if (req_ihl < 20U || req_ip_len < req_ihl + 8U ||
        request_len < ETH_HEADER_LEN + req_ip_len) {
        return 0;
    }

    req_icmp = req_ip + req_ihl;
    if (req_icmp[0] != 8U || req_icmp[1] != 0U) {
        return 0;
    }

    icmp_len = (uint16_t)(req_ip_len - req_ihl);
    frame_len = (uint16_t)(ETH_HEADER_LEN + 20U + icmp_len);
    if (out_len < frame_len) {
        return 0;
    }

    local_memset(out, 0, out_len);
    local_memcpy(out, request + 6U, 6U);
    local_memcpy(out + 6U, g_state.bssid, 6U);
    out[12] = 0x08U;
    out[13] = 0x00U;

    ip = out + ETH_HEADER_LEN;
    ip[0] = 0x45U;
    ip[8] = 64U;
    ip[9] = IPV4_PROTO_ICMP;
    put_be16(ip + 2U, (uint16_t)(20U + icmp_len));
    put_be16(ip + 4U, 0xa006U);
    local_memcpy(ip + 12U, g_sim_ap_ipv4, 4U);
    local_memcpy(ip + 16U, req_ip + 12U, 4U);
    put_be16(ip + 10U, ipv4_checksum(ip, 20U));

    icmp = ip + 20U;
    local_memcpy(icmp, req_icmp, icmp_len);
    icmp[0] = 0U;
    icmp[1] = 0U;
    icmp[2] = 0U;
    icmp[3] = 0U;
    put_be16(icmp + 2U, ipv4_checksum(icmp, icmp_len));
    return frame_len < 60U ? 60 : (int)frame_len;
}

static uint32_t sim_http_header_len(void) {
    static const uint8_t header[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: AOS-WiFi-Sim\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 1024\r\n"
        "Connection: close\r\n"
        "\r\n";
    return (uint32_t)(sizeof(header) - 1U);
}

static uint32_t sim_http_total_len(void) {
    return sim_http_header_len() + SIM_HTTP_BODY_LEN;
}

static uint8_t sim_http_byte_at(uint32_t offset) {
    static const uint8_t header[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: AOS-WiFi-Sim\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 1024\r\n"
        "Connection: close\r\n"
        "\r\n";
    static const uint8_t body_pattern[] =
        "AOS Wi-Fi TCP multi-packet download data.\n";
    uint32_t header_len = (uint32_t)(sizeof(header) - 1U);
    uint32_t body_len = (uint32_t)(sizeof(body_pattern) - 1U);

    if (offset < header_len) {
        return header[offset];
    }
    offset -= header_len;
    return body_pattern[offset % body_len];
}

static uint16_t sim_http_copy(uint32_t offset, uint8_t* out, uint16_t max_len) {
    uint32_t total = sim_http_total_len();
    uint16_t copied = 0;

    if (!out || offset >= total) {
        return 0;
    }
    while (copied < max_len && offset + copied < total) {
        out[copied] = sim_http_byte_at(offset + copied);
        copied++;
    }
    return copied;
}

static int build_sim_tcp_http_reply(const uint8_t* request,
                                    uint16_t request_len,
                                    uint8_t* out,
                                    uint32_t out_len) {
    const uint8_t* req_ip;
    const uint8_t* req_tcp;
    uint8_t* ip;
    uint8_t* tcp;
    uint16_t req_ihl;
    uint16_t req_ip_len;
    uint16_t req_tcp_hlen;
    uint16_t req_tcp_len;
    uint16_t req_src_port;
    uint16_t req_dst_port;
    uint32_t req_seq;
    uint32_t req_ack;
    uint32_t reply_seq = SIM_TCP_SEQ;
    uint32_t reply_ack = 0;
    uint32_t reply_offset = 0;
    uint32_t tcp_payload_len32;
    uint16_t payload_len = 0;
    uint8_t flags;
    uint16_t tcp_len;
    uint16_t ip_len;
    uint16_t frame_len;

    if (!request || !out || request_len < ETH_HEADER_LEN + 20U + 20U ||
        out_len < 60U ||
        request[12] != 0x08U || request[13] != 0x00U) {
        return 0;
    }

    req_ip = request + ETH_HEADER_LEN;
    if ((req_ip[0] >> 4U) != 4U || req_ip[9] != IPV4_PROTO_TCP ||
        !same_bytes(req_ip + 16U, g_sim_ap_ipv4, 4U)) {
        return 0;
    }

    req_ihl = (uint16_t)((req_ip[0] & 0x0fU) * 4U);
    req_ip_len = get_be16(req_ip + 2U);
    if (req_ihl < 20U || req_ip_len < req_ihl + 20U ||
        request_len < ETH_HEADER_LEN + req_ip_len) {
        return 0;
    }

    req_tcp = req_ip + req_ihl;
    req_tcp_hlen = (uint16_t)((req_tcp[12] >> 4U) * 4U);
    if (req_tcp_hlen < 20U || req_ip_len < req_ihl + req_tcp_hlen) {
        return 0;
    }
    req_tcp_len = (uint16_t)(req_ip_len - req_ihl);
    req_src_port = get_be16(req_tcp);
    req_dst_port = get_be16(req_tcp + 2U);
    if (req_dst_port != HTTP_PORT) {
        return 0;
    }

    req_seq = ((uint32_t)req_tcp[4] << 24) |
              ((uint32_t)req_tcp[5] << 16) |
              ((uint32_t)req_tcp[6] << 8) |
              (uint32_t)req_tcp[7];
    req_ack = ((uint32_t)req_tcp[8] << 24) |
              ((uint32_t)req_tcp[9] << 16) |
              ((uint32_t)req_tcp[10] << 8) |
              (uint32_t)req_tcp[11];
    flags = req_tcp[13];
    tcp_payload_len32 = (uint32_t)(req_tcp_len - req_tcp_hlen);

    if ((flags & 0x02U) != 0U) {
        flags = 0x12U;
        reply_seq = SIM_TCP_SEQ;
        reply_ack = req_seq + 1U;
        g_sim_http.active = 0U;
    } else if ((flags & 0x01U) != 0U) {
        flags = 0x10U;
        reply_seq = req_ack >= SIM_TCP_SEQ ? req_ack : SIM_TCP_SEQ + 1U + g_sim_http.sent;
        reply_ack = req_seq + 1U;
        payload_len = 0U;
        g_sim_http.active = 0U;
    } else if ((flags & 0x18U) != 0U &&
               req_tcp_len > req_tcp_hlen &&
               req_ack == SIM_TCP_SEQ + 1U) {
        g_sim_http.active = 1U;
        g_sim_http.client_port = req_src_port;
        g_sim_http.client_next_seq = req_seq + tcp_payload_len32;
        g_sim_http.sent = 0U;
        reply_offset = 0U;
        reply_seq = SIM_TCP_SEQ + 1U;
        reply_ack = g_sim_http.client_next_seq;
        payload_len = SIM_HTTP_CHUNK_SIZE;
        if (payload_len > sim_http_total_len()) {
            payload_len = (uint16_t)sim_http_total_len();
        }
        flags = (payload_len >= sim_http_total_len()) ? 0x19U : 0x18U;
        g_sim_http.sent = payload_len;
    } else if ((flags & 0x10U) != 0U &&
               tcp_payload_len32 == 0U &&
               g_sim_http.active &&
               g_sim_http.client_port == req_src_port &&
               req_ack >= SIM_TCP_SEQ + 1U &&
               req_ack <= SIM_TCP_SEQ + 1U + g_sim_http.sent) {
        uint32_t remaining;
        reply_offset = req_ack - (SIM_TCP_SEQ + 1U);
        remaining = sim_http_total_len() - reply_offset;
        payload_len = remaining > SIM_HTTP_CHUNK_SIZE ? SIM_HTTP_CHUNK_SIZE : (uint16_t)remaining;
        reply_seq = SIM_TCP_SEQ + 1U + reply_offset;
        reply_ack = g_sim_http.client_next_seq;
        flags = (reply_offset + payload_len >= sim_http_total_len()) ? 0x19U : 0x18U;
        if (reply_offset + payload_len > g_sim_http.sent) {
            g_sim_http.sent = reply_offset + payload_len;
        }
    } else {
        return 0;
    }

    tcp_len = (uint16_t)(20U + payload_len);
    ip_len = (uint16_t)(20U + tcp_len);
    frame_len = (uint16_t)(ETH_HEADER_LEN + ip_len);
    if (out_len < frame_len) {
        return 0;
    }

    local_memset(out, 0, out_len);
    local_memcpy(out, request + 6U, 6U);
    local_memcpy(out + 6U, g_state.bssid, 6U);
    out[12] = 0x08U;
    out[13] = 0x00U;

    ip = out + ETH_HEADER_LEN;
    ip[0] = 0x45U;
    ip[8] = 64U;
    ip[9] = IPV4_PROTO_TCP;
    put_be16(ip + 2U, ip_len);
    put_be16(ip + 4U, 0xa008U);
    local_memcpy(ip + 12U, g_sim_ap_ipv4, 4U);
    local_memcpy(ip + 16U, req_ip + 12U, 4U);
    put_be16(ip + 10U, ipv4_checksum(ip, 20U));

    tcp = ip + 20U;
    put_be16(tcp, HTTP_PORT);
    put_be16(tcp + 2U, req_src_port);
    put_be32(tcp + 4U, reply_seq);
    put_be32(tcp + 8U, reply_ack);
    tcp[12] = 0x50U;
    tcp[13] = flags;
    put_be16(tcp + 14U, 64240U);
    if (payload_len > 0U) {
        (void)sim_http_copy(reply_offset, tcp + 20U, payload_len);
    }
    put_be16(tcp + 16U, tcp_checksum(g_sim_ap_ipv4, req_ip + 12U, tcp, tcp_len));
    return frame_len < 60U ? 60 : (int)frame_len;
}

static int scan_store(const char* ssid,
                      const uint8_t bssid[6],
                      uint8_t channel,
                      int8_t rssi_dbm,
                      uint8_t security) {
    struct mac80211_scan_result* result;

    if (!ssid || !bssid || channel == 0 || mac80211_channel_to_freq_mhz(channel) == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < g_scan_count; i++) {
        if (bssid_equals(g_scan_results[i].bssid, bssid)) {
            copy_ssid(g_scan_results[i].ssid, ssid);
            g_scan_results[i].channel = channel;
            g_scan_results[i].rssi_dbm = rssi_dbm;
            g_scan_results[i].security = security;
            g_state.scan_count = (uint8_t)g_scan_count;
            return 0;
        }
    }

    if (g_scan_count >= MAC80211_SCAN_MAX_RESULTS) {
        return -1;
    }

    result = &g_scan_results[g_scan_count++];
    local_memset(result, 0, sizeof(*result));
    copy_ssid(result->ssid, ssid);
    copy_bssid(result->bssid, bssid);
    result->channel = channel;
    result->rssi_dbm = rssi_dbm;
    result->security = security;
    g_state.scan_count = (uint8_t)g_scan_count;
    return 0;
}

static void sync_wifi_driver_stats(void) {
    const struct wifi_mgmt_tx_stats* tx_stats = wifi_mgmt_tx_stats();
    const struct wifi_mgmt_rx_stats* rx_stats = wifi_mgmt_rx_stats();

    if (tx_stats) {
        g_state.driver_tx_calls = tx_stats->calls;
        g_state.driver_tx_hw = tx_stats->hardware_frames;
        g_state.driver_tx_sim = tx_stats->simulated_frames;
        g_state.driver_tx_errors = tx_stats->errors;
    }
    if (rx_stats) {
        g_state.driver_rx_calls = rx_stats->calls;
        g_state.driver_rx_accepted = rx_stats->accepted_frames;
        g_state.driver_rx_errors = rx_stats->errors;
        g_state.driver_rx_last_len = rx_stats->last_length;
        g_state.driver_rx_last_rssi = rx_stats->last_rssi_dbm;
    }
}

void mac80211_init(void) {
    static const uint8_t aos_lab_beacon[] = {
        0x80, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
        0xa0, 0x05, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x64, 0x00,
        0x11, 0x04,
        WLAN_EID_SSID, 0x07, 'A', 'O', 'S', '-', 'L', 'a', 'b',
        WLAN_EID_DS_PARAMS, 0x01, 0x06,
        WLAN_EID_RSN, 0x02, 0x01, 0x00,
    };

    local_memset(&g_state, 0, sizeof(g_state));
    g_state.selected = 0xff;
    mac80211_begin_scan();
    (void)mac80211_rx_frame(aos_lab_beacon, sizeof(aos_lab_beacon), -42);
    mac80211_finish_scan();
    (void)mac80211_select_network(0);
    driver_register_system(DRIVER_CLASS_NETWORK, "aos-mac80211",
                           "ready: scan/auth/assoc state machine");
}

void mac80211_scan_clear(void) {
    local_memset(g_scan_results, 0, sizeof(g_scan_results));
    g_scan_count = 0;
    g_state.scan_count = 0;
    g_state.selected = 0xff;
    local_memset(g_state.ssid, 0, sizeof(g_state.ssid));
    local_memset(g_state.bssid, 0, sizeof(g_state.bssid));
    g_state.channel = 0;
    g_state.rssi_dbm = 0;
    g_state.security = MAC80211_SECURITY_UNKNOWN;
}

int mac80211_scan_add(const char* ssid,
                      const uint8_t bssid[6],
                      uint8_t channel,
                      int8_t rssi_dbm,
                      uint8_t security) {
    return scan_store(ssid, bssid, channel, rssi_dbm, security);
}

int mac80211_rx_frame(const void* frame, uint32_t len, int8_t rssi_dbm) {
    const uint8_t* bytes = (const uint8_t*)frame;
    const struct mac80211_frame_header* hdr;
    char ssid[MAC80211_SSID_MAX];
    uint8_t channel = 0;
    uint8_t security = MAC80211_SECURITY_OPEN;
    uint16_t capability;
    uint32_t pos;

    if (!bytes || len < 2U) {
        g_state.rx_other++;
        return -1;
    }

    if (mac80211_frame_type((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8)) ==
        MAC80211_FTYPE_MANAGEMENT) {
        g_state.rx_mgmt++;
    } else if (mac80211_frame_type((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8)) ==
               MAC80211_FTYPE_DATA) {
        g_state.rx_data++;
    } else {
        g_state.rx_other++;
    }

    if (mac80211_is_beacon((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8))) {
        g_state.rx_beacon++;
    } else if (mac80211_is_probe_response((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8))) {
        g_state.rx_probe_resp++;
    }

    if (len < sizeof(struct mac80211_frame_header)) {
        return -1;
    }

    hdr = (const struct mac80211_frame_header*)bytes;
    if (mac80211_is_auth(hdr->frame_control)) {
        uint16_t auth_seq;
        uint16_t status;

        if (len < sizeof(struct mac80211_frame_header) + 6U ||
            g_state.selected == 0xff ||
            !bssid_equals(hdr->addr2, g_state.bssid)) {
            return -1;
        }

        auth_seq = (uint16_t)bytes[26] | ((uint16_t)bytes[27] << 8);
        status = (uint16_t)bytes[28] | ((uint16_t)bytes[29] << 8);
        if (auth_seq != 2U || status != WLAN_STATUS_SUCCESS) {
            g_state.state = MAC80211_STATE_SCANNED;
            return -1;
        }

        g_state.state = MAC80211_STATE_AUTHENTICATED;
        return 0;
    }

    if (mac80211_is_assoc_response(hdr->frame_control)) {
        uint16_t status;

        if (len < sizeof(struct mac80211_frame_header) + 6U ||
            g_state.selected == 0xff ||
            g_state.state < MAC80211_STATE_AUTHENTICATED ||
            !bssid_equals(hdr->addr2, g_state.bssid)) {
            return -1;
        }

        status = (uint16_t)bytes[26] | ((uint16_t)bytes[27] << 8);
        if (status != WLAN_STATUS_SUCCESS) {
            g_state.state = MAC80211_STATE_AUTHENTICATED;
            return -1;
        }

        g_state.state = MAC80211_STATE_ASSOCIATED;
        (void)wifi_register_wlan0(g_station_mac);
        return 0;
    }

    if (len < sizeof(struct mac80211_frame_header) + 12U) {
        return -1;
    }

    if (!mac80211_is_beacon(hdr->frame_control) &&
        !mac80211_is_probe_response(hdr->frame_control)) {
        return -1;
    }

    capability = (uint16_t)bytes[34] | ((uint16_t)bytes[35] << 8);
    if ((capability & WLAN_CAPABILITY_PRIVACY) != 0) {
        security = MAC80211_SECURITY_UNKNOWN;
    }

    local_memset(ssid, 0, sizeof(ssid));
    pos = sizeof(struct mac80211_frame_header) + 12U;
    while (pos + 2U <= len) {
        uint8_t id = bytes[pos];
        uint8_t ie_len = bytes[pos + 1U];
        const uint8_t* data = &bytes[pos + 2U];

        pos += 2U;
        if (pos + ie_len > len) {
            break;
        }

        if (id == WLAN_EID_SSID) {
            copy_ssid_bytes(ssid, data, ie_len);
        } else if (id == WLAN_EID_DS_PARAMS && ie_len >= 1U) {
            channel = data[0];
        } else if (id == WLAN_EID_RSN) {
            security = MAC80211_SECURITY_WPA2;
        }

        pos += ie_len;
    }

    if (ssid[0] == '\0' || channel == 0) {
        return -1;
    }

    return scan_store(ssid, hdr->addr3, channel, rssi_dbm, security);
}

void mac80211_begin_scan(void) {
    mac80211_scan_clear();
    g_state.state = MAC80211_STATE_SCANNING;
}

void mac80211_finish_scan(void) {
    g_state.scan_count = (uint8_t)g_scan_count;
    g_state.state = g_scan_count > 0 ? MAC80211_STATE_SCANNED : MAC80211_STATE_DOWN;
}

int mac80211_select_network(uint32_t index) {
    const struct mac80211_scan_result* result = mac80211_scan_get(index);

    if (!result) {
        return -1;
    }

    g_state.selected = (uint8_t)index;
    copy_ssid(g_state.ssid, result->ssid);
    copy_bssid(g_state.bssid, result->bssid);
    g_state.channel = result->channel;
    g_state.rssi_dbm = result->rssi_dbm;
    g_state.security = result->security;
    if (g_state.state == MAC80211_STATE_DOWN || g_state.state == MAC80211_STATE_SCANNING) {
        g_state.state = MAC80211_STATE_SCANNED;
    }
    return 0;
}

int mac80211_authenticate_selected(void) {
    uint8_t auth_req[32];
    int len;
    int rc;

    if (g_state.selected == 0xff || g_state.scan_count == 0) {
        return -1;
    }

    g_state.auth_attempts++;
    g_state.state = MAC80211_STATE_AUTHENTICATING;

    len = mac80211_build_auth_request(auth_req, sizeof(auth_req));
    if (len < 0) {
        g_state.state = MAC80211_STATE_SCANNED;
        return -1;
    }

    rc = wifi_send_management_frame(auth_req, (uint32_t)len);
    sync_wifi_driver_stats();
    if (rc < 0) {
        g_state.state = MAC80211_STATE_SCANNED;
        return -1;
    }

    rc = wifi_receive_management_frame(g_aos_lab_auth_resp,
                                       sizeof(g_aos_lab_auth_resp),
                                       g_state.rssi_dbm);
    sync_wifi_driver_stats();
    return rc;
}

int mac80211_associate_selected(void) {
    uint8_t assoc_req[96];
    int len;
    int rc;

    if (g_state.selected == 0xff || g_state.state < MAC80211_STATE_AUTHENTICATED) {
        return -1;
    }

    g_state.assoc_attempts++;
    g_state.state = MAC80211_STATE_ASSOCIATING;

    len = mac80211_build_assoc_request(assoc_req, sizeof(assoc_req));
    if (len < 0) {
        g_state.state = MAC80211_STATE_AUTHENTICATED;
        return -1;
    }

    rc = wifi_send_management_frame(assoc_req, (uint32_t)len);
    sync_wifi_driver_stats();
    if (rc < 0) {
        g_state.state = MAC80211_STATE_AUTHENTICATED;
        return -1;
    }

    rc = wifi_receive_management_frame(g_aos_lab_assoc_resp,
                                       sizeof(g_aos_lab_assoc_resp),
                                       g_state.rssi_dbm);
    sync_wifi_driver_stats();
    return rc;
}

int mac80211_build_probe_request(const char* ssid, uint8_t* out, uint32_t out_len) {
    struct mac80211_frame_header* hdr;
    uint8_t len = ssid_len(ssid);
    uint32_t pos;

    if (!out || out_len < sizeof(struct mac80211_frame_header) + 4U + len) {
        return -1;
    }

    local_memset(out, 0, out_len);
    hdr = (struct mac80211_frame_header*)out;
    hdr->frame_control = (uint16_t)(MAC80211_STYPE_PROBE_REQ << 4);
    copy_bssid(hdr->addr1, g_broadcast_mac);
    copy_bssid(hdr->addr2, g_station_mac);
    copy_bssid(hdr->addr3, g_broadcast_mac);

    pos = sizeof(struct mac80211_frame_header);
    out[pos++] = WLAN_EID_SSID;
    out[pos++] = len;
    if (len > 0) {
        local_memcpy(&out[pos], ssid, len);
        pos += len;
    }

    out[pos++] = 1;
    out[pos++] = 2;
    out[pos++] = 0x82;
    out[pos++] = 0x84;

    g_state.tx_mgmt++;
    g_state.tx_probe_req++;
    g_state.last_tx_len = pos;
    return (int)pos;
}

int mac80211_build_auth_request(uint8_t* out, uint32_t out_len) {
    struct mac80211_frame_header* hdr;
    uint32_t pos;

    if (!out || out_len < sizeof(struct mac80211_frame_header) + 6U ||
        g_state.selected == 0xff) {
        return -1;
    }

    local_memset(out, 0, out_len);
    hdr = (struct mac80211_frame_header*)out;
    hdr->frame_control = (uint16_t)(MAC80211_STYPE_AUTH << 4);
    copy_bssid(hdr->addr1, g_state.bssid);
    copy_bssid(hdr->addr2, g_station_mac);
    copy_bssid(hdr->addr3, g_state.bssid);

    pos = sizeof(struct mac80211_frame_header);
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x01;
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x00;

    g_state.tx_mgmt++;
    g_state.last_tx_len = pos;
    return (int)pos;
}

int mac80211_build_assoc_request(uint8_t* out, uint32_t out_len) {
    struct mac80211_frame_header* hdr;
    uint8_t len = ssid_len(g_state.ssid);
    uint32_t pos;

    if (!out || out_len < sizeof(struct mac80211_frame_header) + 10U + len ||
        g_state.selected == 0xff || len == 0) {
        return -1;
    }

    local_memset(out, 0, out_len);
    hdr = (struct mac80211_frame_header*)out;
    hdr->frame_control = (uint16_t)(MAC80211_STYPE_ASSOC_REQ << 4);
    copy_bssid(hdr->addr1, g_state.bssid);
    copy_bssid(hdr->addr2, g_station_mac);
    copy_bssid(hdr->addr3, g_state.bssid);

    pos = sizeof(struct mac80211_frame_header);
    out[pos++] = 0x11;
    out[pos++] = 0x00;
    out[pos++] = 0x0a;
    out[pos++] = 0x00;
    out[pos++] = WLAN_EID_SSID;
    out[pos++] = len;
    local_memcpy(&out[pos], g_state.ssid, len);
    pos += len;
    out[pos++] = 1;
    out[pos++] = 2;
    out[pos++] = 0x82;
    out[pos++] = 0x84;

    g_state.tx_mgmt++;
    g_state.last_tx_len = pos;
    return (int)pos;
}

int mac80211_build_data_from_ethernet(const uint8_t* ethernet_frame,
                                      uint16_t ethernet_len,
                                      uint8_t* out,
                                      uint32_t out_len) {
    struct mac80211_frame_header* hdr;
    uint32_t payload_len;
    uint32_t pos;

    if (!ethernet_frame || !out || ethernet_len < ETH_HEADER_LEN ||
        g_state.state != MAC80211_STATE_ASSOCIATED) {
        return -1;
    }

    payload_len = (uint32_t)ethernet_len - ETH_HEADER_LEN;
    if (out_len < sizeof(struct mac80211_frame_header) + LLC_SNAP_LEN + payload_len) {
        return -1;
    }

    local_memset(out, 0, out_len);
    hdr = (struct mac80211_frame_header*)out;
    hdr->frame_control = (uint16_t)((MAC80211_STYPE_DATA << 4) |
                                    (MAC80211_FTYPE_DATA << 2) |
                                    IEEE80211_FC_TODS);
    copy_bssid(hdr->addr1, g_state.bssid);
    copy_bssid(hdr->addr2, g_station_mac);
    copy_bssid(hdr->addr3, ethernet_frame);

    pos = sizeof(struct mac80211_frame_header);
    out[pos++] = 0xaa;
    out[pos++] = 0xaa;
    out[pos++] = 0x03;
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = ethernet_frame[12];
    out[pos++] = ethernet_frame[13];
    local_memcpy(&out[pos], &ethernet_frame[ETH_HEADER_LEN], payload_len);
    pos += payload_len;

    g_state.last_tx_len = pos;
    return (int)pos;
}

int mac80211_build_ethernet_from_data(const uint8_t* data_frame,
                                      uint32_t data_len,
                                      uint8_t* out,
                                      uint32_t out_len) {
    const struct mac80211_frame_header* hdr;
    uint32_t payload_len;
    const uint8_t* llc;
    const uint8_t* payload;

    if (!data_frame || !out ||
        data_len < sizeof(struct mac80211_frame_header) + LLC_SNAP_LEN ||
        out_len < ETH_HEADER_LEN ||
        g_state.state != MAC80211_STATE_ASSOCIATED) {
        return -1;
    }

    hdr = (const struct mac80211_frame_header*)data_frame;
    if (!mac80211_is_data(hdr->frame_control) ||
        !bssid_equals(hdr->addr1, g_station_mac) ||
        !bssid_equals(hdr->addr3, g_state.bssid)) {
        return -1;
    }

    llc = data_frame + sizeof(struct mac80211_frame_header);
    if (llc[0] != 0xaa || llc[1] != 0xaa || llc[2] != 0x03 ||
        llc[3] != 0x00 || llc[4] != 0x00 || llc[5] != 0x00) {
        return -1;
    }

    payload = llc + LLC_SNAP_LEN;
    payload_len = data_len - sizeof(struct mac80211_frame_header) - LLC_SNAP_LEN;
    if (out_len < ETH_HEADER_LEN + payload_len) {
        return -1;
    }

    copy_bssid(out, hdr->addr1);
    copy_bssid(out + 6, hdr->addr2);
    out[12] = llc[6];
    out[13] = llc[7];
    if (payload_len > 0) {
        local_memcpy(out + ETH_HEADER_LEN, payload, payload_len);
    }
    return (int)(ETH_HEADER_LEN + payload_len);
}

int mac80211_send_ethernet(const uint8_t* ethernet_frame, uint16_t ethernet_len) {
    uint8_t data_frame[1600];
    uint8_t rx_frame[1600];
    uint8_t rx_eth[1518];
    struct mac80211_frame_header* rx_hdr;
    int len;
    int rc;
    int rx_len;
    int dhcp_len;

    len = mac80211_build_data_from_ethernet(ethernet_frame,
                                            ethernet_len,
                                            data_frame,
                                            sizeof(data_frame));
    if (len < 0) {
        return -1;
    }

    rc = wifi_send_data_frame(data_frame, (uint32_t)len);
    if (rc < 0) {
        return -1;
    }

    dhcp_len = build_sim_dhcp_reply(ethernet_frame, ethernet_len, rx_eth, sizeof(rx_eth));
    if (dhcp_len > 0) {
        (void)wifi_queue_ethernet_frame(rx_eth, (uint32_t)dhcp_len);
        sync_wifi_driver_stats();
        return 0;
    }

    rx_len = build_sim_dns_reply(ethernet_frame, ethernet_len, rx_eth, sizeof(rx_eth));
    if (rx_len > 0) {
        (void)wifi_queue_ethernet_frame(rx_eth, (uint32_t)rx_len);
        sync_wifi_driver_stats();
        return 0;
    }

    rx_len = build_sim_arp_reply(ethernet_frame, ethernet_len, rx_eth, sizeof(rx_eth));
    if (rx_len > 0) {
        (void)wifi_queue_ethernet_frame(rx_eth, (uint32_t)rx_len);
        sync_wifi_driver_stats();
        return 0;
    }

    rx_len = build_sim_icmp_echo_reply(ethernet_frame, ethernet_len, rx_eth, sizeof(rx_eth));
    if (rx_len > 0) {
        (void)wifi_queue_ethernet_frame(rx_eth, (uint32_t)rx_len);
        sync_wifi_driver_stats();
        return 0;
    }

    rx_len = build_sim_tcp_http_reply(ethernet_frame, ethernet_len, rx_eth, sizeof(rx_eth));
    if (rx_len > 0) {
        (void)wifi_queue_ethernet_frame(rx_eth, (uint32_t)rx_len);
        sync_wifi_driver_stats();
        return 0;
    }

    local_memcpy(rx_frame, data_frame, (size_t)len);
    rx_hdr = (struct mac80211_frame_header*)rx_frame;
    copy_bssid(rx_hdr->addr1, g_station_mac);
    copy_bssid(rx_hdr->addr2, g_state.bssid);
    copy_bssid(rx_hdr->addr3, g_state.bssid);
    rx_len = mac80211_build_ethernet_from_data(rx_frame,
                                               (uint32_t)len,
                                               rx_eth,
                                               sizeof(rx_eth));
    if (rx_len > 0) {
        (void)wifi_queue_ethernet_frame(rx_eth, (uint32_t)rx_len);
    }

    sync_wifi_driver_stats();
    return 0;
}

int mac80211_active_probe(void) {
    uint8_t probe_req[64];
    int len;
    int tx_rc;

    mac80211_begin_scan();
    len = mac80211_build_probe_request(0, probe_req, sizeof(probe_req));
    if (len < 0) {
        mac80211_finish_scan();
        return -1;
    }

    tx_rc = wifi_send_management_frame(probe_req, (uint32_t)len);
    sync_wifi_driver_stats();
    if (tx_rc < 0) {
        mac80211_finish_scan();
        return -1;
    }

    (void)wifi_receive_management_frame(g_aos_lab_probe_resp,
                                        sizeof(g_aos_lab_probe_resp),
                                        -41);
    sync_wifi_driver_stats();
    mac80211_finish_scan();
    (void)mac80211_select_network(0);
    return 0;
}

int mac80211_test_rx_probe_response(void) {
    int rc;

    rc = wifi_receive_management_frame(g_aos_lab_probe_resp,
                                       sizeof(g_aos_lab_probe_resp),
                                       -40);
    sync_wifi_driver_stats();
    if (rc == 0) {
        mac80211_finish_scan();
        (void)mac80211_select_network(0);
    }
    return rc;
}

const struct mac80211_state* mac80211_get_state(void) {
    return &g_state;
}

uint32_t mac80211_scan_count(void) {
    return g_scan_count;
}

const struct mac80211_scan_result* mac80211_scan_get(uint32_t index) {
    if (index >= g_scan_count) {
        return 0;
    }
    return &g_scan_results[index];
}

uint8_t mac80211_frame_type(uint16_t frame_control) {
    return (uint8_t)((frame_control >> 2) & 0x3U);
}

uint8_t mac80211_frame_subtype(uint16_t frame_control) {
    return (uint8_t)((frame_control >> 4) & 0xFU);
}

int mac80211_is_beacon(uint16_t frame_control) {
    return mac80211_frame_type(frame_control) == MAC80211_FTYPE_MANAGEMENT &&
           mac80211_frame_subtype(frame_control) == MAC80211_STYPE_BEACON;
}

int mac80211_is_probe_response(uint16_t frame_control) {
    return mac80211_frame_type(frame_control) == MAC80211_FTYPE_MANAGEMENT &&
           mac80211_frame_subtype(frame_control) == MAC80211_STYPE_PROBE_RESP;
}

int mac80211_is_auth(uint16_t frame_control) {
    return mac80211_frame_type(frame_control) == MAC80211_FTYPE_MANAGEMENT &&
           mac80211_frame_subtype(frame_control) == MAC80211_STYPE_AUTH;
}

int mac80211_is_assoc_response(uint16_t frame_control) {
    return mac80211_frame_type(frame_control) == MAC80211_FTYPE_MANAGEMENT &&
           mac80211_frame_subtype(frame_control) == MAC80211_STYPE_ASSOC_RESP;
}

int mac80211_is_data(uint16_t frame_control) {
    return mac80211_frame_type(frame_control) == MAC80211_FTYPE_DATA;
}

uint16_t mac80211_channel_to_freq_mhz(uint8_t channel) {
    if (channel >= 1 && channel <= 13) {
        return (uint16_t)(2407U + (uint16_t)channel * 5U);
    }
    if (channel == 14) {
        return 2484;
    }
    if (channel >= 32 && channel <= 177) {
        return (uint16_t)(5000U + (uint16_t)channel * 5U);
    }
    return 0;
}

uint8_t mac80211_freq_mhz_to_channel(uint16_t freq_mhz) {
    if (freq_mhz == 2484) {
        return 14;
    }
    if (freq_mhz >= 2412 && freq_mhz <= 2472 && ((freq_mhz - 2407U) % 5U) == 0) {
        return (uint8_t)((freq_mhz - 2407U) / 5U);
    }
    if (freq_mhz >= 5160 && freq_mhz <= 5885 && ((freq_mhz - 5000U) % 5U) == 0) {
        return (uint8_t)((freq_mhz - 5000U) / 5U);
    }
    return 0;
}
