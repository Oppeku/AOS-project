/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <firmware.h>
#include <mac80211.h>
#include <netdev.h>
#include <pci.h>
#include <wifi.h>
#include <stddef.h>
#include <stdint.h>

#define PCI_CLASS_NETWORK 0x02U
#define PCI_SUBCLASS_ETHERNET 0x00U
#define PCI_SUBCLASS_OTHER_NETWORK 0x80U
#define WIFI_RX_QUEUE_FRAME_SIZE 1518U

struct wifi_vendor_name {
    uint16_t vendor_id;
    const char* driver;
    const char* firmware;
    const char* firmware_found_status;
    const char* firmware_missing_status;
};

static const struct wifi_vendor_name wifi_vendors[] = {
    {0x8086, "aos-iwlwifi", "firmware/iwlwifi-test.fw",
     "detected: Intel WiFi, firmware found, mac80211 needed",
     "detected: Intel WiFi, missing firmware/iwlwifi-test.fw"},
    {0x10EC, "aos-rtlwifi", "firmware/rtlwifi-test.fw",
     "detected: Realtek WiFi, firmware found, mac80211 needed",
     "detected: Realtek WiFi, missing firmware/rtlwifi-test.fw"},
    {0x14E4, "aos-b43", "firmware/b43-test.fw",
     "detected: Broadcom WiFi, firmware found, mac80211 needed",
     "detected: Broadcom WiFi, missing firmware/b43-test.fw"},
    {0x168C, "aos-athwifi", "firmware/athwifi-test.fw",
     "detected: Qualcomm Atheros WiFi, firmware found, mac80211 needed",
     "detected: Qualcomm Atheros WiFi, missing firmware/athwifi-test.fw"},
    {0x1814, "aos-rt2x00", "firmware/rt2x00-test.fw",
     "detected: Ralink WiFi, firmware found, mac80211 needed",
     "detected: Ralink WiFi, missing firmware/rt2x00-test.fw"},
    {0x14C3, "aos-mtwifi", "firmware/mtwifi-test.fw",
     "detected: MediaTek WiFi, firmware found, mac80211 needed",
     "detected: MediaTek WiFi, missing firmware/mtwifi-test.fw"},
};

static int g_wifi_hardware_found;
static uint8_t g_wifi_bus;
static uint8_t g_wifi_slot;
static uint8_t g_wifi_function;
static struct wifi_mgmt_tx_stats g_mgmt_tx_stats;
static struct wifi_mgmt_rx_stats g_mgmt_rx_stats;
static struct wifi_data_tx_stats g_data_tx_stats;
static struct wifi_data_rx_stats g_data_rx_stats;
static uint8_t g_rx_frame[WIFI_RX_QUEUE_FRAME_SIZE];
static uint32_t g_rx_frame_len;

static const struct wifi_vendor_name* wifi_vendor(uint16_t vendor_id) {
    for (size_t i = 0; i < sizeof(wifi_vendors) / sizeof(wifi_vendors[0]); i++) {
        if (wifi_vendors[i].vendor_id == vendor_id) {
            return &wifi_vendors[i];
        }
    }
    return 0;
}

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
}

static int wifi_netdev_send(const uint8_t* frame, uint16_t length) {
    return mac80211_send_ethernet(frame, length);
}

static int wifi_netdev_recv(uint8_t* frame, uint16_t max_length) {
    uint32_t len;

    if (!frame || max_length == 0) {
        g_data_rx_stats.errors++;
        return -1;
    }

    if (g_rx_frame_len == 0) {
        return 0;
    }

    len = g_rx_frame_len;
    if (len > max_length) {
        g_data_rx_stats.errors++;
        return -1;
    }

    local_memcpy(frame, g_rx_frame, len);
    g_rx_frame_len = 0;
    g_data_rx_stats.delivered_frames++;
    return (int)len;
}

void wifi_register_driver(void) {
    size_t found = 0;

    local_memset(&g_mgmt_tx_stats, 0, sizeof(g_mgmt_tx_stats));
    local_memset(&g_mgmt_rx_stats, 0, sizeof(g_mgmt_rx_stats));
    local_memset(&g_data_tx_stats, 0, sizeof(g_data_tx_stats));
    local_memset(&g_data_rx_stats, 0, sizeof(g_data_rx_stats));
    g_rx_frame_len = 0;
    g_wifi_hardware_found = 0;
    g_wifi_bus = 0xff;
    g_wifi_slot = 0xff;
    g_wifi_function = 0xff;

    for (size_t i = 0; i < pci_count(); i++) {
        const struct pci_device* dev = pci_get(i);
        const struct wifi_vendor_name* vendor;

        if (!dev) {
            continue;
        }

        if (dev->class_code != PCI_CLASS_NETWORK || dev->subclass != PCI_SUBCLASS_OTHER_NETWORK) {
            continue;
        }

        vendor = wifi_vendor(dev->vendor_id);
        if (vendor) {
            const uint8_t* data = 0;
            uint32_t size = 0;
            const char* status = vendor->firmware_missing_status;

            if (firmware_find(vendor->firmware, &data, &size) == 0 && data && size > 0) {
                status = vendor->firmware_found_status;
            }
            found += (size_t)driver_claim_pci(dev->vendor_id, dev->device_id, vendor->driver, status);
            g_wifi_bus = dev->bus;
            g_wifi_slot = dev->slot;
            g_wifi_function = dev->function;
        } else {
            found += (size_t)driver_claim_pci(dev->vendor_id, dev->device_id,
                                              "aos-wifi-pci",
                                              "detected: 802.11 controller, chipset driver needed");
            g_wifi_bus = dev->bus;
            g_wifi_slot = dev->slot;
            g_wifi_function = dev->function;
        }
    }

    g_wifi_hardware_found = found > 0;
    if (found == 0) {
        driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                               "not found: mgmt TX hook simulating, ethernet only");
    } else {
        driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                               "ready: wireless mgmt TX hook available");
    }

    (void)driver_claim_pci_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET,
                                 "aos-net-pci", "detected: non-e1000 ethernet candidate");
}

int wifi_has_hardware(void) {
    return g_wifi_hardware_found;
}

int wifi_register_wlan0(const uint8_t mac[6]) {
    int index;

    if (!mac) {
        return -1;
    }

    index = netdev_find_by_name("wlan0");
    if (index >= 0) {
        return index;
    }

    index = netdev_register_wifi("wlan0",
                                 g_wifi_hardware_found ? "aos-wifi" : "aos-wifi-sim",
                                 mac,
                                 1,
                                 g_wifi_bus,
                                 g_wifi_slot,
                                 g_wifi_function,
                                 "associated: 802.11 tx bridge active",
                                 wifi_netdev_send,
                                 wifi_netdev_recv);
    if (index >= 0) {
        (void)netdev_configure_ipv6_link_local((size_t)index);
    }
    return index;
}

int wifi_send_management_frame(const uint8_t* frame, uint32_t length) {
    if (!frame || length < 24 || length > 4096) {
        g_mgmt_tx_stats.errors++;
        return -1;
    }

    g_mgmt_tx_stats.calls++;
    g_mgmt_tx_stats.last_length = length;
    if (g_wifi_hardware_found) {
        g_mgmt_tx_stats.hardware_frames++;
        return 1;
    }

    g_mgmt_tx_stats.simulated_frames++;
    return 0;
}

int wifi_receive_management_frame(const uint8_t* frame, uint32_t length, int8_t rssi_dbm) {
    int rc;

    if (!frame || length < 24 || length > 4096) {
        g_mgmt_rx_stats.errors++;
        return -1;
    }

    g_mgmt_rx_stats.calls++;
    g_mgmt_rx_stats.last_length = length;
    g_mgmt_rx_stats.last_rssi_dbm = rssi_dbm;
    rc = mac80211_rx_frame(frame, length, rssi_dbm);
    if (rc < 0) {
        g_mgmt_rx_stats.errors++;
        return -1;
    }

    g_mgmt_rx_stats.accepted_frames++;
    return 0;
}

int wifi_send_data_frame(const uint8_t* frame, uint32_t length) {
    if (!frame || length < 32 || length > 4096) {
        g_data_tx_stats.errors++;
        return -1;
    }

    g_data_tx_stats.calls++;
    g_data_tx_stats.last_length = length;
    if (g_wifi_hardware_found) {
        g_data_tx_stats.hardware_frames++;
        return 1;
    }

    g_data_tx_stats.simulated_frames++;
    return 0;
}

int wifi_queue_ethernet_frame(const uint8_t* frame, uint32_t length) {
    g_data_rx_stats.calls++;
    if (!frame || length < 14 || length > WIFI_RX_QUEUE_FRAME_SIZE) {
        g_data_rx_stats.errors++;
        return -1;
    }

    local_memcpy(g_rx_frame, frame, length);
    g_rx_frame_len = length;
    g_data_rx_stats.last_length = length;
    g_data_rx_stats.queued_frames++;
    return 0;
}

const struct wifi_mgmt_tx_stats* wifi_mgmt_tx_stats(void) {
    return &g_mgmt_tx_stats;
}

const struct wifi_mgmt_rx_stats* wifi_mgmt_rx_stats(void) {
    return &g_mgmt_rx_stats;
}

const struct wifi_data_tx_stats* wifi_data_tx_stats(void) {
    return &g_data_tx_stats;
}

const struct wifi_data_rx_stats* wifi_data_rx_stats(void) {
    return &g_data_rx_stats;
}
