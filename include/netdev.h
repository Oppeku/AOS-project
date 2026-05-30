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
    int (*send)(const uint8_t* frame, uint16_t length);
    int (*recv)(uint8_t* frame, uint16_t max_length);
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
size_t netdev_count(void);
const struct netdev* netdev_get(size_t index);
int netdev_configure_ipv4(size_t index,
                          const uint8_t addr[4],
                          uint8_t prefix,
                          const uint8_t gateway[4],
                          const uint8_t dns[4]);
int netdev_send(size_t index, const uint8_t* frame, uint16_t length);
int netdev_recv(size_t index, uint8_t* frame, uint16_t max_length);

#endif
