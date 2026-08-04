/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stddef.h>
#include <stdint.h>

#define BLUETOOTH_DRIVER_MAX 32
#define BLUETOOTH_STATUS_MAX 64
#define BLUETOOTH_NAME_MAX 64
#define BLUETOOTH_LINK_KEY_SIZE 16

#define BLUETOOTH_CONTROLLER_NONE 0
#define BLUETOOTH_CONTROLLER_USB_HCI 1
#define BLUETOOTH_MAX_DISCOVERED 16
#define BLUETOOTH_MAX_BONDS 16

#define BLUETOOTH_CONTROL_SCAN 1
#define BLUETOOTH_CONTROL_PAIR 2

struct bluetooth_status {
    uint8_t present;
    uint8_t controller_type;
    uint8_t interface_number;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t endpoints;
    uint8_t hci_ready;
    uint8_t event_endpoint;
    uint8_t event_interval;
    uint8_t last_event;
    uint8_t last_status;
    uint16_t event_max_packet;
    uint16_t hci_revision;
    uint16_t manufacturer;
    uint16_t lmp_subversion;
    uint8_t hci_version;
    uint8_t lmp_version;
    uint8_t scanning;
    uint8_t pairing;
    uint8_t discovered_count;
    uint8_t last_scan_status;
    uint8_t last_pair_status;
    char driver[BLUETOOTH_DRIVER_MAX];
    char status[BLUETOOTH_STATUS_MAX];
};

struct bluetooth_discovered_device {
    uint8_t valid;
    uint8_t address[6];
    uint8_t page_scan_repetition_mode;
    uint8_t has_rssi;
    int8_t rssi;
    uint8_t has_name;
    uint8_t name_status;
    uint8_t connected;
    uint8_t link_type;
    uint8_t encryption_enabled;
    uint8_t pair_status;
    uint8_t bonded;
    uint16_t connection_handle;
    char name[BLUETOOTH_NAME_MAX];
};

struct bluetooth_bond_info {
    uint8_t valid;
    uint8_t address[6];
    uint8_t key_type;
    uint8_t authenticated;
    uint8_t has_name;
    uint8_t reserved[3];
    char name[BLUETOOTH_NAME_MAX];
};

void bluetooth_register_driver(void);
void bluetooth_usb_interface_seen(uint8_t interface_number,
                                  uint8_t interface_class,
                                  uint8_t interface_subclass,
                                  uint8_t interface_protocol,
                                  uint8_t endpoints);
void bluetooth_usb_event_endpoint_seen(uint8_t endpoint_address,
                                       uint16_t max_packet,
                                       uint8_t interval);
void bluetooth_hci_reset_complete(uint8_t hci_status);
void bluetooth_hci_version_complete(uint8_t hci_status,
                                    uint8_t hci_version,
                                    uint16_t hci_revision,
                                    uint8_t lmp_version,
                                    uint16_t manufacturer,
                                    uint16_t lmp_subversion);
void bluetooth_hci_transport_error(const char* status);
int bluetooth_get_status(struct bluetooth_status* out);
int bluetooth_start_scan(void);
int bluetooth_pair_device(size_t index);
void bluetooth_discovery_clear(void);
void bluetooth_discovery_add(const uint8_t address[6],
                             uint8_t page_scan_repetition_mode,
                             uint8_t has_rssi,
                             int8_t rssi);
void bluetooth_discovery_set_name(const uint8_t address[6],
                                  uint8_t hci_status,
                                  const char* name,
                                  size_t name_len);
void bluetooth_scan_started(void);
void bluetooth_scan_complete(uint8_t hci_status);
void bluetooth_pair_started(size_t index);
void bluetooth_pair_complete(const uint8_t address[6],
                             uint8_t hci_status,
                             uint16_t connection_handle,
                             uint8_t link_type,
                             uint8_t encryption_enabled);
void bluetooth_auth_event(const uint8_t address[6], const char* status);
void bluetooth_link_key_store(const uint8_t address[6],
                              const uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE],
                              uint8_t key_type);
int bluetooth_find_link_key(const uint8_t address[6],
                            uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE],
                            uint8_t* key_type);
int bluetooth_get_discovered(size_t index, struct bluetooth_discovered_device* out);
int bluetooth_get_bond(size_t index, struct bluetooth_bond_info* out);

#endif
