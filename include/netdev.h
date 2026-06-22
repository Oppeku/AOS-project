/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef NETDEV_H
#define NETDEV_H

#include <stdint.h>
#include <stddef.h>

#define NETDEV_MAX_DEVICES 8
#define NETDEV_NAME_MAX 16
#define NETDEV_DRIVER_MAX 32
#define NETDEV_STATUS_MAX 64

#define NETDEV_TYPE_ETHERNET 1
#define NETDEV_TYPE_WIFI 2

struct netdev {
    char name[NETDEV_NAME_MAX];
    char driver[NETDEV_DRIVER_MAX];
    char status[NETDEV_STATUS_MAX];
    uint8_t type;
    uint8_t link_up;
    uint8_t mac[6];
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t ipv4_addr[4];
    uint8_t ipv4_gateway[4];
    uint8_t ipv4_dns[4];
    uint8_t ipv4_prefix;
    uint8_t ipv4_configured;
    uint8_t ipv6_configured;
    uint8_t ipv6_prefix;
    uint8_t ipv6_addr[16];
    uint8_t ipv6_gateway[16];
    uint8_t ipv6_dns[16];
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t tx_dropped;
    uint64_t rx_dropped;
    uint64_t tx_arp;
    uint64_t rx_arp;
    uint64_t tx_ipv4;
    uint64_t rx_ipv4;
    uint64_t tx_ipv6;
    uint64_t rx_ipv6;
    uint64_t tx_icmp;
    uint64_t rx_icmp;
    uint64_t tx_udp;
    uint64_t rx_udp;
    uint64_t tx_tcp;
    uint64_t rx_tcp;
    int (*send)(const uint8_t* frame, uint16_t length);
    int (*recv)(uint8_t* frame, uint16_t max_length);
};

struct netdev_stats {
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t tx_dropped;
    uint64_t rx_dropped;
    uint64_t tx_arp;
    uint64_t rx_arp;
    uint64_t tx_ipv4;
    uint64_t rx_ipv4;
    uint64_t tx_ipv6;
    uint64_t rx_ipv6;
    uint64_t tx_icmp;
    uint64_t rx_icmp;
    uint64_t tx_udp;
    uint64_t rx_udp;
    uint64_t tx_tcp;
    uint64_t rx_tcp;
};

void netdev_init(void);
int netdev_register_ethernet(const char* name,
                             const char* driver,
                             const uint8_t mac[6],
                             uint8_t link_up,
                             uint8_t bus,
                             uint8_t slot,
                             uint8_t function,
                             const char* status,
                             int (*send)(const uint8_t* frame, uint16_t length),
                             int (*recv)(uint8_t* frame, uint16_t max_length));
int netdev_register_wifi(const char* name,
                         const char* driver,
                         const uint8_t mac[6],
                         uint8_t link_up,
                         uint8_t bus,
                         uint8_t slot,
                         uint8_t function,
                         const char* status,
                         int (*send)(const uint8_t* frame, uint16_t length),
                         int (*recv)(uint8_t* frame, uint16_t max_length));
size_t netdev_count(void);
const struct netdev* netdev_get(size_t index);
int netdev_find_by_name(const char* name);
int netdev_get_stats(size_t index, struct netdev_stats* out);
int netdev_configure_ipv4(size_t index,
                          const uint8_t addr[4],
                          uint8_t prefix,
                          const uint8_t gateway[4],
                          const uint8_t dns[4]);
int netdev_configure_ipv6(size_t index,
                          const uint8_t addr[16],
                          uint8_t prefix,
                          const uint8_t gateway[16],
                          const uint8_t dns[16]);
int netdev_configure_ipv6_link_local(size_t index);
int netdev_send(size_t index, const uint8_t* frame, uint16_t length);
int netdev_recv(size_t index, uint8_t* frame, uint16_t max_length);

#endif
