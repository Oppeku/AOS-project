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

void netdev_init(void) {
    local_memset(g_netdevs, 0, sizeof(g_netdevs));
    g_netdev_count = 0;
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
    struct netdev* dev;

    if (g_netdev_count >= NETDEV_MAX_DEVICES) {
        return -1;
    }

    dev = &g_netdevs[g_netdev_count++];
    local_memset(dev, 0, sizeof(*dev));
    copy_string(dev->name, sizeof(dev->name), name);
    copy_string(dev->driver, sizeof(dev->driver), driver);
    copy_string(dev->status, sizeof(dev->status), status);
    dev->type = NETDEV_TYPE_ETHERNET;
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
    return 0;
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

int netdev_send(size_t index, const uint8_t* frame, uint16_t length) {
    const struct netdev* dev = netdev_get(index);
    if (!dev || !dev->send || !frame || length == 0) {
        return -1;
    }
    return dev->send(frame, length);
}

int netdev_recv(size_t index, uint8_t* frame, uint16_t max_length) {
    const struct netdev* dev = netdev_get(index);
    if (!dev || !dev->recv || !frame || max_length == 0) {
        return -1;
    }
    return dev->recv(frame, max_length);
}
