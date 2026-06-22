/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <firmware.h>
#include <mac80211.h>
#include <netdev.h>
#include <pci.h>
#include <vmm.h>
#include <wifi.h>
#include <stddef.h>
#include <stdint.h>

extern void serial_print(const char* s);
extern uint64_t p4_table[];

#define PCI_CLASS_NETWORK 0x02U
#define PCI_SUBCLASS_ETHERNET 0x00U
#define PCI_SUBCLASS_OTHER_NETWORK 0x80U
#define PCI_VENDOR_INTEL 0x8086U
#define PCI_COMMAND_REG 0x04U
#define PCI_COMMAND_MEMORY 0x2U
#define PCI_BAR_MMIO_MASK 0xFFFFFFF0U
#define PCI_BAR_IO_SPACE 0x1U
#define PCI_BAR_TYPE_MASK 0x6U
#define PCI_BAR_TYPE_64BIT 0x4U
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define INTEL_WIFI_MMIO_SIZE 0x1000ULL
#define INTEL_WIFI_MMIO_VIRT_BASE 0xFFFF800000500000ULL
#define INTEL_WIFI_CSR_GP_CNTRL 0x024U
#define INTEL_WIFI_CSR_HW_REV 0x028U
#define INTEL_WIFI_CSR_EEPROM_GP 0x030U
#define INTEL_WIFI_FW_MAGIC "AOS-IWLFW\n"
#define INTEL_WIFI_FW_REQUIRED "driver=aos-iwlwifi\n"
#define INTEL_WIFI_FW_END "ENDHDR\n"
#define INTEL_WIFI_ENABLE_HW_UPLOAD 0
#define WIFI_RX_QUEUE_FRAME_SIZE 1518U

enum intel_wifi_firmware_state {
    INTEL_WIFI_FW_MISSING = 0,
    INTEL_WIFI_FW_FOUND = 1,
    INTEL_WIFI_FW_STAGED = 2,
    INTEL_WIFI_FW_UPLOAD_READY = 3,
    INTEL_WIFI_FW_UPLOAD_DISABLED = 4
};

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
static volatile uint8_t* g_intel_wifi_mmio;
static const uint8_t* g_intel_wifi_fw_payload;
static uint32_t g_intel_wifi_fw_payload_size;
static enum intel_wifi_firmware_state g_intel_wifi_upload_state;

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

static size_t local_strlen(const char* s) {
    size_t n = 0;

    while (s && s[n]) {
        n++;
    }
    return n;
}

static int mem_equal(const uint8_t* data, const char* text, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (data[i] != (uint8_t)text[i]) {
            return 0;
        }
    }
    return 1;
}

static int mem_find(const uint8_t* data, uint32_t size, const char* text, uint32_t* offset_out) {
    size_t text_len = local_strlen(text);

    if (!data || !text || text_len == 0 || size < text_len) {
        return 0;
    }

    for (uint32_t i = 0; i + text_len <= size; i++) {
        if (mem_equal(data + i, text, text_len)) {
            if (offset_out) {
                *offset_out = i;
            }
            return 1;
        }
    }
    return 0;
}

static int mem_contains(const uint8_t* data, uint32_t size, const char* text) {
    return mem_find(data, size, text, 0);
}

static enum intel_wifi_firmware_state intel_wifi_stage_firmware(const uint8_t* data, uint32_t size) {
    size_t magic_len = local_strlen(INTEL_WIFI_FW_MAGIC);
    uint32_t end_offset = 0;
    uint32_t payload_offset;

    g_intel_wifi_fw_payload = 0;
    g_intel_wifi_fw_payload_size = 0;
    g_intel_wifi_upload_state = INTEL_WIFI_FW_MISSING;

    if (!data || size == 0) {
        return INTEL_WIFI_FW_MISSING;
    }

    if (size < magic_len || !mem_equal(data, INTEL_WIFI_FW_MAGIC, magic_len)) {
        return INTEL_WIFI_FW_FOUND;
    }

    if (!mem_contains(data, size, INTEL_WIFI_FW_REQUIRED) ||
        !mem_find(data, size, INTEL_WIFI_FW_END, &end_offset)) {
        return INTEL_WIFI_FW_FOUND;
    }

    serial_print("iwlwifi: firmware header valid, staged in memory\n");

    payload_offset = end_offset + (uint32_t)local_strlen(INTEL_WIFI_FW_END);
    while (payload_offset < size &&
           (data[payload_offset] == (uint8_t)'\n' || data[payload_offset] == (uint8_t)'\r')) {
        payload_offset++;
    }
    if (payload_offset < size) {
        g_intel_wifi_fw_payload = data + payload_offset;
        g_intel_wifi_fw_payload_size = size - payload_offset;
        serial_print("iwlwifi: firmware payload parsed, upload ready\n");
        return INTEL_WIFI_FW_UPLOAD_READY;
    }

    return INTEL_WIFI_FW_STAGED;
}

static enum intel_wifi_firmware_state intel_wifi_upload_firmware(void) {
    if (!g_intel_wifi_fw_payload || g_intel_wifi_fw_payload_size == 0) {
        return INTEL_WIFI_FW_STAGED;
    }

#if INTEL_WIFI_ENABLE_HW_UPLOAD
    serial_print("iwlwifi: hardware firmware upload enabled but not implemented\n");
    return INTEL_WIFI_FW_UPLOAD_READY;
#else
    serial_print("iwlwifi: firmware upload prepared, hardware upload disabled\n");
    return INTEL_WIFI_FW_UPLOAD_DISABLED;
#endif
}

static enum intel_wifi_firmware_state intel_wifi_current_firmware_state(void) {
    const uint8_t* data = 0;
    uint32_t size = 0;
    enum intel_wifi_firmware_state state;

    if (firmware_find("firmware/iwlwifi-test.fw", &data, &size) != 0) {
        return INTEL_WIFI_FW_MISSING;
    }
    state = intel_wifi_stage_firmware(data, size);
    if (state == INTEL_WIFI_FW_UPLOAD_READY) {
        state = intel_wifi_upload_firmware();
    }
    g_intel_wifi_upload_state = state;
    return state;
}

static void serial_print_hex8(uint8_t value) {
    const char* hex = "0123456789abcdef";
    char out[3];
    out[0] = hex[(value >> 4) & 0xF];
    out[1] = hex[value & 0xF];
    out[2] = '\0';
    serial_print(out);
}

static void serial_print_hex32(uint32_t value) {
    serial_print_hex8((uint8_t)(value >> 24));
    serial_print_hex8((uint8_t)(value >> 16));
    serial_print_hex8((uint8_t)(value >> 8));
    serial_print_hex8((uint8_t)value);
}

static uint32_t intel_wifi_mmio_read32(uint32_t offset) {
    return *(volatile uint32_t*)(g_intel_wifi_mmio + offset);
}

static void intel_wifi_map_mmio_window(uint64_t phys_base, uint64_t virt_base, uint64_t size) {
    uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_WRITE_THROUGH | PAGE_CACHE_DISABLE;

    for (uint64_t off = 0; off < size; off += 4096ULL) {
        vmm_map_page(p4_table, virt_base + off, phys_base + off, flags);
    }
}

static uint64_t pci_bar0_mmio_base(const struct pci_device* dev) {
    uint64_t base;

    if (!dev || (dev->bar[0] & PCI_BAR_IO_SPACE)) {
        return 0;
    }

    base = (uint64_t)(dev->bar[0] & PCI_BAR_MMIO_MASK);
    if ((dev->bar[0] & PCI_BAR_TYPE_MASK) == PCI_BAR_TYPE_64BIT) {
        base |= ((uint64_t)dev->bar[1] << 32);
    }
    return base;
}

static void intel_wifi_log_probe(const struct pci_device* dev, uint64_t phys_base) {
    uint32_t gp_cntrl;
    uint32_t hw_rev;
    uint32_t eeprom_gp;

    gp_cntrl = intel_wifi_mmio_read32(INTEL_WIFI_CSR_GP_CNTRL);
    hw_rev = intel_wifi_mmio_read32(INTEL_WIFI_CSR_HW_REV);
    eeprom_gp = intel_wifi_mmio_read32(INTEL_WIFI_CSR_EEPROM_GP);

    serial_print("iwlwifi: ");
    serial_print_hex8(dev->bus);
    serial_print(":");
    serial_print_hex8(dev->slot);
    serial_print(".");
    serial_print_hex8(dev->function);
    serial_print(" BAR0=0x");
    serial_print_hex32((uint32_t)phys_base);
    serial_print(" GP_CNTRL=0x");
    serial_print_hex32(gp_cntrl);
    serial_print(" HW_REV=0x");
    serial_print_hex32(hw_rev);
    serial_print(" EEPROM_GP=0x");
    serial_print_hex32(eeprom_gp);
    serial_print(" read-only probe\n");
}

static const char* intel_wifi_status_for_bar(const char* prefix, enum intel_wifi_firmware_state firmware_state) {
    if (firmware_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
        if (prefix == (const char*)0) {
            return "Intel WiFi: upload prepared, hw disabled";
        }
        return prefix;
    }
    if (firmware_state == INTEL_WIFI_FW_UPLOAD_READY) {
        if (prefix == (const char*)0) {
            return "Intel WiFi: upload ready, RX/TX TODO";
        }
        return prefix;
    }
    if (firmware_state == INTEL_WIFI_FW_STAGED) {
        if (prefix == (const char*)0) {
            return "Intel WiFi: firmware staged";
        }
        return prefix;
    }
    if (firmware_state == INTEL_WIFI_FW_FOUND) {
        return "Intel WiFi: firmware found, header invalid";
    }
    return "Intel WiFi: firmware missing";
}

static const char* intel_wifi_status(const struct pci_device* dev, enum intel_wifi_firmware_state firmware_state) {
    uint32_t command;
    uint64_t phys_base;
    uint64_t page_base;
    uint64_t page_offset;

    if (!dev) {
        return "Intel WiFi: no PCI device";
    }

    if (dev->bar[0] & PCI_BAR_IO_SPACE) {
        if (firmware_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
            return "Intel WiFi: I/O BAR, upload disabled";
        }
        if (firmware_state == INTEL_WIFI_FW_UPLOAD_READY) {
            return "Intel WiFi: I/O BAR, upload ready";
        }
        if (firmware_state == INTEL_WIFI_FW_STAGED) {
            return "Intel WiFi: I/O BAR, firmware staged";
        }
        if (firmware_state == INTEL_WIFI_FW_FOUND) {
            return "Intel WiFi: I/O BAR, fw header bad";
        }
        return "Intel WiFi: I/O BAR, firmware missing";
    }

    phys_base = pci_bar0_mmio_base(dev);
    if (phys_base == 0) {
        if (firmware_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
            return "Intel WiFi: BAR0 missing, upload disabled";
        }
        if (firmware_state == INTEL_WIFI_FW_UPLOAD_READY) {
            return "Intel WiFi: BAR0 missing, upload ready";
        }
        if (firmware_state == INTEL_WIFI_FW_STAGED) {
            return "Intel WiFi: BAR0 missing, firmware staged";
        }
        if (firmware_state == INTEL_WIFI_FW_FOUND) {
            return "Intel WiFi: BAR0 missing, fw header bad";
        }
        return "Intel WiFi: BAR0 missing, firmware missing";
    }

    command = pci_config_read32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG);
    if ((command & PCI_COMMAND_MEMORY) == 0) {
        if (firmware_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
            return "Intel WiFi: BAR0 present, upload disabled";
        }
        if (firmware_state == INTEL_WIFI_FW_UPLOAD_READY) {
            return "Intel WiFi: BAR0 present, upload ready";
        }
        if (firmware_state == INTEL_WIFI_FW_STAGED) {
            return "Intel WiFi: BAR0 present, firmware staged";
        }
        if (firmware_state == INTEL_WIFI_FW_FOUND) {
            return "Intel WiFi: BAR0 present, fw header bad";
        }
        return "Intel WiFi: BAR0 present, no firmware";
    }

    page_base = phys_base & ~0xFFFULL;
    page_offset = phys_base & 0xFFFULL;
    intel_wifi_map_mmio_window(page_base, INTEL_WIFI_MMIO_VIRT_BASE, INTEL_WIFI_MMIO_SIZE + page_offset);
    g_intel_wifi_mmio = (volatile uint8_t*)(INTEL_WIFI_MMIO_VIRT_BASE + page_offset);
    intel_wifi_log_probe(dev, phys_base);

    if (firmware_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
        return intel_wifi_status_for_bar("Intel WiFi: MMIO OK, upload hw disabled", firmware_state);
    }
    if (firmware_state == INTEL_WIFI_FW_UPLOAD_READY) {
        return intel_wifi_status_for_bar("Intel WiFi: MMIO OK, upload ready, RX/TX TODO", firmware_state);
    }
    return intel_wifi_status_for_bar("Intel WiFi: MMIO OK, firmware staged, RX/TX TODO", firmware_state);
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
    enum intel_wifi_firmware_state intel_fw_state = INTEL_WIFI_FW_MISSING;

    local_memset(&g_mgmt_tx_stats, 0, sizeof(g_mgmt_tx_stats));
    local_memset(&g_mgmt_rx_stats, 0, sizeof(g_mgmt_rx_stats));
    local_memset(&g_data_tx_stats, 0, sizeof(g_data_tx_stats));
    local_memset(&g_data_rx_stats, 0, sizeof(g_data_rx_stats));
    g_rx_frame_len = 0;
    g_wifi_hardware_found = 0;
    g_wifi_bus = 0xff;
    g_wifi_slot = 0xff;
    g_wifi_function = 0xff;
    g_intel_wifi_upload_state = INTEL_WIFI_FW_MISSING;

    intel_fw_state = intel_wifi_current_firmware_state();

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
            enum intel_wifi_firmware_state intel_fw_state = INTEL_WIFI_FW_MISSING;

            if (firmware_find(vendor->firmware, &data, &size) == 0 && data && size > 0) {
                status = vendor->firmware_found_status;
            }
            if (dev->vendor_id == PCI_VENDOR_INTEL) {
                intel_fw_state = intel_wifi_stage_firmware(data, size);
                status = intel_wifi_status(dev, intel_fw_state);
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
        if (intel_fw_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
            driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                                   "not found: iwlwifi upload disabled, ethernet only");
            goto claim_ethernet_candidates;
        }
        if (intel_fw_state == INTEL_WIFI_FW_UPLOAD_READY) {
            driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                                   "not found: iwlwifi upload ready, ethernet only");
            goto claim_ethernet_candidates;
        }
        if (intel_fw_state == INTEL_WIFI_FW_STAGED) {
            driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                                   "not found: iwlwifi firmware staged, ethernet only");
            goto claim_ethernet_candidates;
        }
        if (intel_fw_state == INTEL_WIFI_FW_FOUND) {
            driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                                   "not found: iwlwifi fw header bad, ethernet only");
            goto claim_ethernet_candidates;
        }
        driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                               "not found: mgmt TX hook simulating, ethernet only");
    } else {
        driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                               "ready: wireless mgmt TX hook available");
    }

claim_ethernet_candidates:
    (void)driver_claim_pci_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET,
                                 "aos-net-pci", "detected: non-e1000 ethernet candidate");
}

void wifi_refresh_firmware_status(void) {
    enum intel_wifi_firmware_state intel_fw_state = intel_wifi_current_firmware_state();

    if (!g_wifi_hardware_found) {
        if (intel_fw_state == INTEL_WIFI_FW_UPLOAD_DISABLED) {
            (void)driver_update_system_status("aos-wifi",
                                              "not found: iwlwifi upload disabled, ethernet only");
        } else if (intel_fw_state == INTEL_WIFI_FW_UPLOAD_READY) {
            (void)driver_update_system_status("aos-wifi",
                                              "not found: iwlwifi upload ready, ethernet only");
        } else if (intel_fw_state == INTEL_WIFI_FW_STAGED) {
            (void)driver_update_system_status("aos-wifi",
                                              "not found: iwlwifi firmware staged, ethernet only");
        } else if (intel_fw_state == INTEL_WIFI_FW_FOUND) {
            (void)driver_update_system_status("aos-wifi",
                                              "not found: iwlwifi fw header bad, ethernet only");
        } else {
            (void)driver_update_system_status("aos-wifi",
                                              "not found: mgmt TX hook simulating, ethernet only");
        }
        return;
    }

    for (size_t i = 0; i < pci_count(); i++) {
        const struct pci_device* dev = pci_get(i);

        if (!dev || dev->vendor_id != PCI_VENDOR_INTEL) {
            continue;
        }
        if (dev->class_code != PCI_CLASS_NETWORK || dev->subclass != PCI_SUBCLASS_OTHER_NETWORK) {
            continue;
        }
        (void)driver_update_pci_status(dev->vendor_id, dev->device_id,
                                       intel_wifi_status(dev, intel_fw_state));
    }
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
