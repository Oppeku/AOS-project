/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <netdev.h>

static struct netdev g_netdevs[NETDEV_MAX_DEVICES];
static size_t g_netdev_count;

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void copy_string(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst_size) return;
    if (!src) src = "";
    while (i + 1 < dst_size && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int string_equals(const char* a, const char* b) {
    size_t i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

void netdev_init(void) {
    local_memset(g_netdevs, 0, sizeof(g_netdevs));
    g_netdev_count = 0;
}

static int netdev_register_typed(uint8_t type,
                                 const char* name,
                                 const char* driver,
                                 const uint8_t mac[6],
                                 uint8_t link_up,
                                 uint8_t bus,
                                 uint8_t slot,
                                 uint8_t function,
                                 const char* status,
                                 int (*send)(const uint8_t* frame, uint16_t length),
                                 int (*recv)(uint8_t* frame, uint16_t max_length)) {
    struct netdev* dev;

    if (g_netdev_count >= NETDEV_MAX_DEVICES) {
        return -1;
    }

    dev = &g_netdevs[g_netdev_count++];
    local_memset(dev, 0, sizeof(*dev));
    copy_string(dev->name, sizeof(dev->name), name);
    copy_string(dev->driver, sizeof(dev->driver), driver);
    copy_string(dev->status, sizeof(dev->status), status);
    dev->type = type;
    dev->link_up = link_up ? 1 : 0;
    if (mac) {
        for (size_t i = 0; i < 6; i++) {
            dev->mac[i] = mac[i];
        }
    }
    dev->bus = bus;
    dev->slot = slot;
    dev->function = function;
    dev->send = send;
    dev->recv = recv;
    return (int)(g_netdev_count - 1);
}

int netdev_register_ethernet(const char* name,
                             const char* driver,
                             const uint8_t mac[6],
                             uint8_t link_up,
                             uint8_t bus,
                             uint8_t slot,
                             uint8_t function,
                             const char* status,
                             int (*send)(const uint8_t* frame, uint16_t length),
                             int (*recv)(uint8_t* frame, uint16_t max_length)) {
    return netdev_register_typed(NETDEV_TYPE_ETHERNET, name, driver, mac, link_up,
                                 bus, slot, function, status, send, recv);
}

int netdev_register_wifi(const char* name,
                         const char* driver,
                         const uint8_t mac[6],
                         uint8_t link_up,
                         uint8_t bus,
                         uint8_t slot,
                         uint8_t function,
                         const char* status,
                         int (*send)(const uint8_t* frame, uint16_t length),
                         int (*recv)(uint8_t* frame, uint16_t max_length)) {
    return netdev_register_typed(NETDEV_TYPE_WIFI, name, driver, mac, link_up,
                                 bus, slot, function, status, send, recv);
}

size_t netdev_count(void) {
    return g_netdev_count;
}

const struct netdev* netdev_get(size_t index) {
    if (index >= g_netdev_count) {
        return 0;
    }
    return &g_netdevs[index];
}

int netdev_find_by_name(const char* name) {
    for (size_t i = 0; i < g_netdev_count; i++) {
        if (string_equals(g_netdevs[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

int netdev_get_stats(size_t index, struct netdev_stats* out) {
    const struct netdev* dev = netdev_get(index);

    if (!dev || !out) {
        return -1;
    }

    out->tx_packets = dev->tx_packets;
    out->rx_packets = dev->rx_packets;
    out->tx_bytes = dev->tx_bytes;
    out->rx_bytes = dev->rx_bytes;
    out->tx_errors = dev->tx_errors;
    out->rx_errors = dev->rx_errors;
    out->tx_dropped = dev->tx_dropped;
    out->rx_dropped = dev->rx_dropped;
    return 0;
}

int netdev_configure_ipv4(size_t index,
                          const uint8_t addr[4],
                          uint8_t prefix,
                          const uint8_t gateway[4],
                          const uint8_t dns[4]) {
    struct netdev* dev;

    if (index >= g_netdev_count || !addr || !gateway || !dns || prefix > 32) {
        return -1;
    }

    dev = &g_netdevs[index];
    for (size_t i = 0; i < 4; i++) {
        dev->ipv4_addr[i] = addr[i];
        dev->ipv4_gateway[i] = gateway[i];
        dev->ipv4_dns[i] = dns[i];
    }
    dev->ipv4_prefix = prefix;
    dev->ipv4_configured = 1;
    return 0;
}

int netdev_configure_ipv6(size_t index,
                          const uint8_t addr[16],
                          uint8_t prefix,
                          const uint8_t gateway[16],
                          const uint8_t dns[16]) {
    struct netdev* dev;

    if (index >= g_netdev_count || !addr || prefix > 128) {
        return -1;
    }

    dev = &g_netdevs[index];
    for (size_t i = 0; i < 16; i++) {
        dev->ipv6_addr[i] = addr[i];
        dev->ipv6_gateway[i] = gateway ? gateway[i] : 0;
        dev->ipv6_dns[i] = dns ? dns[i] : 0;
    }
    dev->ipv6_prefix = prefix;
    dev->ipv6_configured = 1;
    return 0;
}

int netdev_configure_ipv6_link_local(size_t index) {
    struct netdev* dev;
    uint8_t addr[16];
    uint8_t empty[16];

    if (index >= g_netdev_count) {
        return -1;
    }

    dev = &g_netdevs[index];
    local_memset(addr, 0, sizeof(addr));
    local_memset(empty, 0, sizeof(empty));
    addr[0] = 0xfe;
    addr[1] = 0x80;
    addr[8] = dev->mac[0] ^ 0x02U;
    addr[9] = dev->mac[1];
    addr[10] = dev->mac[2];
    addr[11] = 0xff;
    addr[12] = 0xfe;
    addr[13] = dev->mac[3];
    addr[14] = dev->mac[4];
    addr[15] = dev->mac[5];
    return netdev_configure_ipv6(index, addr, 64, empty, empty);
}

int netdev_send(size_t index, const uint8_t* frame, uint16_t length) {
    struct netdev* dev;
    int rc;

    if (index >= g_netdev_count) {
        return -1;
    }
    dev = &g_netdevs[index];
    if (!dev || !dev->send || !frame || length == 0) {
        if (dev) dev->tx_dropped++;
        return -1;
    }
    rc = dev->send(frame, length);
    if (rc < 0) {
        dev->tx_errors++;
        return rc;
    }
    dev->tx_packets++;
    dev->tx_bytes += length;
    return rc;
}

int netdev_recv(size_t index, uint8_t* frame, uint16_t max_length) {
    struct netdev* dev;
    int rc;

    if (index >= g_netdev_count) {
        return -1;
    }
    dev = &g_netdevs[index];
    if (!dev || !dev->recv || !frame || max_length == 0) {
        if (dev) dev->rx_dropped++;
        return -1;
    }
    rc = dev->recv(frame, max_length);
    if (rc < 0) {
        dev->rx_errors++;
        return rc;
    }
    if (rc > 0) {
        dev->rx_packets++;
        dev->rx_bytes += (uint64_t)rc;
    }
    return rc;
}
