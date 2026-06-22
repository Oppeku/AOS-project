/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_WIFI_H
#define AOS_WIFI_H

#include <stdint.h>

struct wifi_mgmt_tx_stats {
    uint32_t calls;
    uint32_t hardware_frames;
    uint32_t simulated_frames;
    uint32_t errors;
    uint32_t last_length;
};

struct wifi_mgmt_rx_stats {
    uint32_t calls;
    uint32_t accepted_frames;
    uint32_t errors;
    uint32_t last_length;
    int8_t last_rssi_dbm;
    uint8_t reserved[3];
};

struct wifi_data_tx_stats {
    uint32_t calls;
    uint32_t hardware_frames;
    uint32_t simulated_frames;
    uint32_t errors;
    uint32_t last_length;
};

struct wifi_data_rx_stats {
    uint32_t calls;
    uint32_t queued_frames;
    uint32_t delivered_frames;
    uint32_t errors;
    uint32_t last_length;
};

void wifi_register_driver(void);
void wifi_refresh_firmware_status(void);
int wifi_has_hardware(void);
int wifi_register_wlan0(const uint8_t mac[6]);
int wifi_send_management_frame(const uint8_t* frame, uint32_t length);
int wifi_receive_management_frame(const uint8_t* frame, uint32_t length, int8_t rssi_dbm);
int wifi_send_data_frame(const uint8_t* frame, uint32_t length);
int wifi_queue_ethernet_frame(const uint8_t* frame, uint32_t length);
const struct wifi_mgmt_tx_stats* wifi_mgmt_tx_stats(void);
const struct wifi_mgmt_rx_stats* wifi_mgmt_rx_stats(void);
const struct wifi_data_tx_stats* wifi_data_tx_stats(void);
const struct wifi_data_rx_stats* wifi_data_rx_stats(void);

#endif
