/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_MAC80211_H
#define AOS_MAC80211_H

#include <stdint.h>

#define MAC80211_FTYPE_MANAGEMENT 0U
#define MAC80211_FTYPE_CONTROL 1U
#define MAC80211_FTYPE_DATA 2U

#define MAC80211_STYPE_ASSOC_REQ 0U
#define MAC80211_STYPE_ASSOC_RESP 1U
#define MAC80211_STYPE_DATA 0U
#define MAC80211_STYPE_PROBE_REQ 4U
#define MAC80211_STYPE_PROBE_RESP 5U
#define MAC80211_STYPE_BEACON 8U
#define MAC80211_STYPE_AUTH 11U
#define MAC80211_STYPE_DEAUTH 12U

#define MAC80211_SCAN_MAX_RESULTS 16
#define MAC80211_SSID_MAX 32

#define MAC80211_SECURITY_OPEN 0U
#define MAC80211_SECURITY_WPA2 1U
#define MAC80211_SECURITY_WPA3 2U
#define MAC80211_SECURITY_UNKNOWN 255U

#define MAC80211_STATE_DOWN 0U
#define MAC80211_STATE_SCANNING 1U
#define MAC80211_STATE_SCANNED 2U
#define MAC80211_STATE_AUTHENTICATING 3U
#define MAC80211_STATE_AUTHENTICATED 4U
#define MAC80211_STATE_ASSOCIATING 5U
#define MAC80211_STATE_ASSOCIATED 6U

struct mac80211_frame_header {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
} __attribute__((packed));

struct mac80211_scan_result {
    char ssid[MAC80211_SSID_MAX];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t security;
    uint8_t reserved[7];
} __attribute__((packed));

struct mac80211_state {
    uint8_t state;
    uint8_t scan_count;
    uint8_t selected;
    uint8_t security;
    char ssid[MAC80211_SSID_MAX];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t reserved[4];
    uint32_t auth_attempts;
    uint32_t assoc_attempts;
    uint32_t rx_mgmt;
    uint32_t rx_beacon;
    uint32_t rx_probe_resp;
    uint32_t rx_data;
    uint32_t rx_other;
    uint32_t tx_mgmt;
    uint32_t tx_probe_req;
    uint32_t last_tx_len;
    uint32_t driver_tx_calls;
    uint32_t driver_tx_hw;
    uint32_t driver_tx_sim;
    uint32_t driver_tx_errors;
    uint32_t driver_rx_calls;
    uint32_t driver_rx_accepted;
    uint32_t driver_rx_errors;
    uint32_t driver_rx_last_len;
    int8_t driver_rx_last_rssi;
    uint8_t reserved2[3];
} __attribute__((packed));

void mac80211_init(void);
void mac80211_scan_clear(void);
int mac80211_scan_add(const char* ssid,
                      const uint8_t bssid[6],
                      uint8_t channel,
                      int8_t rssi_dbm,
                      uint8_t security);
int mac80211_rx_frame(const void* frame, uint32_t len, int8_t rssi_dbm);
void mac80211_begin_scan(void);
void mac80211_finish_scan(void);
int mac80211_select_network(uint32_t index);
int mac80211_authenticate_selected(void);
int mac80211_associate_selected(void);
int mac80211_build_probe_request(const char* ssid, uint8_t* out, uint32_t out_len);
int mac80211_build_auth_request(uint8_t* out, uint32_t out_len);
int mac80211_build_assoc_request(uint8_t* out, uint32_t out_len);
int mac80211_build_data_from_ethernet(const uint8_t* ethernet_frame,
                                      uint16_t ethernet_len,
                                      uint8_t* out,
                                      uint32_t out_len);
int mac80211_build_ethernet_from_data(const uint8_t* data_frame,
                                      uint32_t data_len,
                                      uint8_t* out,
                                      uint32_t out_len);
int mac80211_send_ethernet(const uint8_t* ethernet_frame, uint16_t ethernet_len);
int mac80211_active_probe(void);
int mac80211_test_rx_probe_response(void);
const struct mac80211_state* mac80211_get_state(void);
uint32_t mac80211_scan_count(void);
const struct mac80211_scan_result* mac80211_scan_get(uint32_t index);
uint8_t mac80211_frame_type(uint16_t frame_control);
uint8_t mac80211_frame_subtype(uint16_t frame_control);
int mac80211_is_beacon(uint16_t frame_control);
int mac80211_is_probe_response(uint16_t frame_control);
int mac80211_is_auth(uint16_t frame_control);
int mac80211_is_assoc_response(uint16_t frame_control);
int mac80211_is_data(uint16_t frame_control);
uint16_t mac80211_channel_to_freq_mhz(uint8_t channel);
uint8_t mac80211_freq_mhz_to_channel(uint16_t freq_mhz);

#endif
