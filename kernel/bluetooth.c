/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <bluetooth.h>
#include <driver.h>
#include <aosfs.h>
#include <vfs.h>
#include <xhci.h>
#include <stddef.h>
#include <stdint.h>

extern void serial_print(const char* s);

#define USB_CLASS_WIRELESS_CONTROLLER 0xe0
#define USB_SUBCLASS_RF_CONTROLLER 0x01
#define USB_PROTOCOL_BLUETOOTH 0x01
#define BLUETOOTH_BOND_STORE_PATH "etc/bluetooth.keys"
#define BLUETOOTH_BOND_STORE_VERSION 1

static struct bluetooth_status g_bluetooth;
static struct bluetooth_discovered_device g_discovered[BLUETOOTH_MAX_DISCOVERED];
static struct bluetooth_bond_record {
    struct bluetooth_bond_info info;
    uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE];
} g_bonds[BLUETOOTH_MAX_BONDS];
static uint8_t g_registered;
static uint8_t g_bonds_loaded;

struct bluetooth_bond_store_header {
    uint8_t magic[8];
    uint32_t version;
    uint32_t count;
};

struct bluetooth_bond_store_record {
    uint8_t valid;
    uint8_t address[6];
    uint8_t key_type;
    uint8_t authenticated;
    uint8_t has_name;
    uint8_t reserved[2];
    uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE];
    char name[BLUETOOTH_NAME_MAX];
};

struct bluetooth_bond_store_file {
    struct bluetooth_bond_store_header header;
    struct bluetooth_bond_store_record records[BLUETOOTH_MAX_BONDS];
};

static const uint8_t g_bond_store_magic[8] = {
    'A', 'O', 'S', 'B', 'T', 'K', 'Y', '1'
};

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void copy_cstr(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int address_equal(const uint8_t a[6], const uint8_t b[6]) {
    for (size_t i = 0; i < 6; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static void copy_address(uint8_t dst[6], const uint8_t src[6]) {
    for (size_t i = 0; i < 6; i++) {
        dst[i] = src[i];
    }
}

static void copy_bytes(uint8_t* dst, const uint8_t* src, size_t n) {
    while (n--) {
        *dst++ = *src++;
    }
}

static int bytes_equal(const uint8_t* a, const uint8_t* b, size_t n) {
    while (n--) {
        if (*a++ != *b++) return 0;
    }
    return 1;
}

static size_t bond_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < BLUETOOTH_MAX_BONDS; i++) {
        if (g_bonds[i].info.valid) {
            count++;
        }
    }
    return count;
}

static void copy_discovered_name_to_bond(struct bluetooth_bond_info* bond,
                                         const uint8_t address[6]) {
    if (!bond || !address) {
        return;
    }

    for (size_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
        if (!g_discovered[i].valid || !address_equal(g_discovered[i].address, address)) {
            continue;
        }
        if (g_discovered[i].has_name && g_discovered[i].name[0]) {
            copy_cstr(bond->name, sizeof(bond->name), g_discovered[i].name);
            bond->has_name = 1;
        }
        return;
    }
}

static void mark_discovered_bonded(const uint8_t address[6]) {
    if (!address) {
        return;
    }

    for (size_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
        if (g_discovered[i].valid && address_equal(g_discovered[i].address, address)) {
            g_discovered[i].bonded = 1;
        }
    }
}

static int bluetooth_save_bonds(void) {
    struct bluetooth_bond_store_file file;
    struct vfs_node node;
    uint64_t written = 0;
    uint32_t new_size = 0;
    size_t out = 0;

    local_memset(&file, 0, sizeof(file));
    copy_bytes(file.header.magic, g_bond_store_magic, sizeof(file.header.magic));
    file.header.version = BLUETOOTH_BOND_STORE_VERSION;
    file.header.count = (uint32_t)bond_count();

    for (size_t i = 0; i < BLUETOOTH_MAX_BONDS && out < BLUETOOTH_MAX_BONDS; i++) {
        if (!g_bonds[i].info.valid) {
            continue;
        }
        file.records[out].valid = 1;
        copy_address(file.records[out].address, g_bonds[i].info.address);
        file.records[out].key_type = g_bonds[i].info.key_type;
        file.records[out].authenticated = g_bonds[i].info.authenticated;
        file.records[out].has_name = g_bonds[i].info.has_name;
        copy_bytes(file.records[out].link_key, g_bonds[i].link_key, BLUETOOTH_LINK_KEY_SIZE);
        copy_cstr(file.records[out].name, sizeof(file.records[out].name), g_bonds[i].info.name);
        out++;
    }

    if (vfs_lookup(BLUETOOTH_BOND_STORE_PATH, &node) != 0) {
        if (aosfs_create_path(BLUETOOTH_BOND_STORE_PATH, &node) != 0) {
            copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                      "warn: Bluetooth bond store create failed");
            driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
            return -1;
        }
    } else {
        (void)aosfs_truncate_path(BLUETOOTH_BOND_STORE_PATH);
        (void)vfs_lookup(BLUETOOTH_BOND_STORE_PATH, &node);
    }

    if (vfs_write_node(&node, 0, (const uint8_t*)&file, sizeof(file), &written, &new_size) != 0 ||
        written != sizeof(file)) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "warn: Bluetooth bond store write failed");
        driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
        return -1;
    }
    return 0;
}

static void bluetooth_load_bonds_once(void) {
    struct bluetooth_bond_store_file file;
    struct vfs_node node;
    uint32_t count;

    if (g_bonds_loaded) {
        return;
    }
    g_bonds_loaded = 1;

    if (vfs_lookup(BLUETOOTH_BOND_STORE_PATH, &node) != 0 ||
        node.size < sizeof(file) ||
        vfs_read_node(&node, 0, (uint8_t*)&file, sizeof(file)) != 0) {
        return;
    }
    if (!bytes_equal(file.header.magic, g_bond_store_magic, sizeof(g_bond_store_magic)) ||
        file.header.version != BLUETOOTH_BOND_STORE_VERSION) {
        return;
    }

    local_memset(g_bonds, 0, sizeof(g_bonds));
    count = file.header.count;
    if (count > BLUETOOTH_MAX_BONDS) {
        count = BLUETOOTH_MAX_BONDS;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!file.records[i].valid) {
            continue;
        }
        g_bonds[i].info.valid = 1;
        copy_address(g_bonds[i].info.address, file.records[i].address);
        g_bonds[i].info.key_type = file.records[i].key_type;
        g_bonds[i].info.authenticated = file.records[i].authenticated ? 1U : 0U;
        g_bonds[i].info.has_name = file.records[i].has_name ? 1U : 0U;
        copy_cstr(g_bonds[i].info.name, sizeof(g_bonds[i].info.name), file.records[i].name);
        copy_bytes(g_bonds[i].link_key, file.records[i].link_key, BLUETOOTH_LINK_KEY_SIZE);
        mark_discovered_bonded(g_bonds[i].info.address);
    }
}

void bluetooth_register_driver(void) {
    local_memset(&g_bluetooth, 0, sizeof(g_bluetooth));
    local_memset(g_discovered, 0, sizeof(g_discovered));
    local_memset(g_bonds, 0, sizeof(g_bonds));
    g_bonds_loaded = 0;
    copy_cstr(g_bluetooth.driver, sizeof(g_bluetooth.driver), "aos-bthci");
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
              "not found: waiting for USB Bluetooth HCI");
    g_registered = 1;
    driver_register_system(DRIVER_CLASS_BLUETOOTH, g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_usb_interface_seen(uint8_t interface_number,
                                  uint8_t interface_class,
                                  uint8_t interface_subclass,
                                  uint8_t interface_protocol,
                                  uint8_t endpoints) {
    if (interface_class != USB_CLASS_WIRELESS_CONTROLLER ||
        interface_subclass != USB_SUBCLASS_RF_CONTROLLER ||
        interface_protocol != USB_PROTOCOL_BLUETOOTH) {
        return;
    }

    if (!g_registered) {
        bluetooth_register_driver();
    }

    g_bluetooth.present = 1;
    g_bluetooth.controller_type = BLUETOOTH_CONTROLLER_USB_HCI;
    g_bluetooth.interface_number = interface_number;
    g_bluetooth.interface_class = interface_class;
    g_bluetooth.interface_subclass = interface_subclass;
    g_bluetooth.interface_protocol = interface_protocol;
    g_bluetooth.endpoints = endpoints;
    copy_cstr(g_bluetooth.driver, sizeof(g_bluetooth.driver), "aos-bthci");
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
              "detected: USB Bluetooth HCI interface");
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
    serial_print("bluetooth: USB Bluetooth HCI interface detected\n");
}

void bluetooth_usb_event_endpoint_seen(uint8_t endpoint_address,
                                       uint16_t max_packet,
                                       uint8_t interval) {
    if (!g_registered) {
        bluetooth_register_driver();
    }

    g_bluetooth.event_endpoint = endpoint_address;
    g_bluetooth.event_max_packet = max_packet;
    g_bluetooth.event_interval = interval;
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
              "ready: USB HCI event endpoint armed");
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_hci_reset_complete(uint8_t hci_status) {
    if (!g_registered) {
        bluetooth_register_driver();
    }

    g_bluetooth.last_event = 0x0e;
    g_bluetooth.last_status = hci_status;
    if (hci_status == 0) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "ready: HCI reset complete");
    } else {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "warn: HCI reset failed");
    }
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_hci_version_complete(uint8_t hci_status,
                                    uint8_t hci_version,
                                    uint16_t hci_revision,
                                    uint8_t lmp_version,
                                    uint16_t manufacturer,
                                    uint16_t lmp_subversion) {
    if (!g_registered) {
        bluetooth_register_driver();
    }

    g_bluetooth.last_event = 0x0e;
    g_bluetooth.last_status = hci_status;
    if (hci_status == 0) {
        g_bluetooth.hci_ready = 1;
        g_bluetooth.hci_version = hci_version;
        g_bluetooth.hci_revision = hci_revision;
        g_bluetooth.lmp_version = lmp_version;
        g_bluetooth.manufacturer = manufacturer;
        g_bluetooth.lmp_subversion = lmp_subversion;
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "ready: HCI version read complete");
    } else {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "warn: HCI version read failed");
    }
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_hci_transport_error(const char* status) {
    if (!g_registered) {
        bluetooth_register_driver();
    }

    g_bluetooth.hci_ready = 0;
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), status);
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_discovery_clear(void) {
    local_memset(g_discovered, 0, sizeof(g_discovered));
    g_bluetooth.discovered_count = 0;
}

void bluetooth_discovery_add(const uint8_t address[6],
                             uint8_t page_scan_repetition_mode,
                             uint8_t has_rssi,
                             int8_t rssi) {
    size_t slot = BLUETOOTH_MAX_DISCOVERED;

    if (!address) {
        return;
    }

    for (size_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
        if (g_discovered[i].valid && address_equal(g_discovered[i].address, address)) {
            slot = i;
            break;
        }
        if (!g_discovered[i].valid && slot == BLUETOOTH_MAX_DISCOVERED) {
            slot = i;
        }
    }

    if (slot >= BLUETOOTH_MAX_DISCOVERED) {
        return;
    }
    if (!g_discovered[slot].valid && g_bluetooth.discovered_count < BLUETOOTH_MAX_DISCOVERED) {
        g_bluetooth.discovered_count++;
    }

    g_discovered[slot].valid = 1;
    copy_address(g_discovered[slot].address, address);
    g_discovered[slot].page_scan_repetition_mode = page_scan_repetition_mode;
    g_discovered[slot].has_rssi = has_rssi ? 1U : 0U;
    g_discovered[slot].rssi = rssi;
}

void bluetooth_discovery_set_name(const uint8_t address[6],
                                  uint8_t hci_status,
                                  const char* name,
                                  size_t name_len) {
    if (!address) {
        return;
    }

    for (size_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
        if (!g_discovered[i].valid || !address_equal(g_discovered[i].address, address)) {
            continue;
        }

        g_discovered[i].name_status = hci_status;
        if (hci_status == 0 && name && name_len > 0) {
            size_t copy_len = name_len;
            if (copy_len >= sizeof(g_discovered[i].name)) {
                copy_len = sizeof(g_discovered[i].name) - 1;
            }
            for (size_t j = 0; j < copy_len; j++) {
                g_discovered[i].name[j] = name[j];
            }
            g_discovered[i].name[copy_len] = '\0';
            g_discovered[i].has_name = 1;
        }
        return;
    }
}

void bluetooth_scan_started(void) {
    g_bluetooth.scanning = 1;
    g_bluetooth.last_scan_status = 0xff;
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "scan: inquiry running");
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_scan_complete(uint8_t hci_status) {
    g_bluetooth.scanning = 0;
    g_bluetooth.last_scan_status = hci_status;
    if (hci_status == 0) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "ready: inquiry complete");
    } else {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "warn: inquiry failed");
    }
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_pair_started(size_t index) {
    (void)index;
    g_bluetooth.pairing = 1;
    g_bluetooth.last_pair_status = 0xff;
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "pair: connection attempt running");
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_pair_complete(const uint8_t address[6],
                             uint8_t hci_status,
                             uint16_t connection_handle,
                             uint8_t link_type,
                             uint8_t encryption_enabled) {
    g_bluetooth.pairing = 0;
    g_bluetooth.last_pair_status = hci_status;

    if (address) {
        for (size_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
            if (!g_discovered[i].valid || !address_equal(g_discovered[i].address, address)) {
                continue;
            }
            g_discovered[i].pair_status = hci_status;
            if (hci_status == 0) {
                g_discovered[i].connected = 1;
                g_discovered[i].connection_handle = connection_handle;
                g_discovered[i].link_type = link_type;
                g_discovered[i].encryption_enabled = encryption_enabled;
                if (bluetooth_find_link_key(address, 0, 0) == 0) {
                    g_discovered[i].bonded = 1;
                }
            }
            break;
        }
    }

    if (hci_status == 0) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "ready: device connected");
    } else {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "warn: device connection failed");
    }
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_auth_event(const uint8_t address[6], const char* status) {
    (void)address;
    if (!g_registered) {
        bluetooth_register_driver();
    }
    copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), status);
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

void bluetooth_link_key_store(const uint8_t address[6],
                              const uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE],
                              uint8_t key_type) {
    size_t slot = BLUETOOTH_MAX_BONDS;

    if (!address || !link_key) {
        return;
    }
    if (!g_registered) {
        bluetooth_register_driver();
    }
    bluetooth_load_bonds_once();

    for (size_t i = 0; i < BLUETOOTH_MAX_BONDS; i++) {
        if (g_bonds[i].info.valid && address_equal(g_bonds[i].info.address, address)) {
            slot = i;
            break;
        }
        if (!g_bonds[i].info.valid && slot == BLUETOOTH_MAX_BONDS) {
            slot = i;
        }
    }
    if (slot >= BLUETOOTH_MAX_BONDS) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "warn: bond table full");
        driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
        return;
    }

    g_bonds[slot].info.valid = 1;
    copy_address(g_bonds[slot].info.address, address);
    g_bonds[slot].info.key_type = key_type;
    g_bonds[slot].info.authenticated =
        (key_type == 0x04 || key_type == 0x05 || key_type == 0x07) ? 1U : 0U;
    copy_discovered_name_to_bond(&g_bonds[slot].info, address);
    copy_bytes(g_bonds[slot].link_key, link_key, BLUETOOTH_LINK_KEY_SIZE);
    mark_discovered_bonded(address);
    if (bluetooth_save_bonds() == 0) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "ready: Bluetooth link key stored");
    } else {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "warn: Bluetooth link key stored in memory only");
    }
    driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
}

int bluetooth_find_link_key(const uint8_t address[6],
                            uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE],
                            uint8_t* key_type) {
    if (!address) {
        return -1;
    }

    bluetooth_load_bonds_once();

    for (size_t i = 0; i < BLUETOOTH_MAX_BONDS; i++) {
        if (!g_bonds[i].info.valid || !address_equal(g_bonds[i].info.address, address)) {
            continue;
        }
        if (link_key) {
            copy_bytes(link_key, g_bonds[i].link_key, BLUETOOTH_LINK_KEY_SIZE);
        }
        if (key_type) {
            *key_type = g_bonds[i].info.key_type;
        }
        return 0;
    }
    return -1;
}

int bluetooth_start_scan(void) {
    int rc;

    if (!g_registered) {
        bluetooth_register_driver();
    }
    if (!g_bluetooth.present || !g_bluetooth.hci_ready) {
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "scan: no initialized Bluetooth HCI adapter");
        driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
        return -1;
    }

    bluetooth_load_bonds_once();
    bluetooth_discovery_clear();
    bluetooth_scan_started();
    rc = xhci_bluetooth_scan();
    if (rc != 0 && g_bluetooth.scanning) {
        bluetooth_scan_complete(1);
    }
    return rc;
}

int bluetooth_pair_device(size_t index) {
    int rc;

    if (!g_registered) {
        bluetooth_register_driver();
    }
    if (!g_bluetooth.present || !g_bluetooth.hci_ready) {
        g_bluetooth.pairing = 0;
        g_bluetooth.last_pair_status = 1;
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "pair: no initialized Bluetooth HCI adapter");
        driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
        return -1;
    }
    if (index >= g_bluetooth.discovered_count) {
        g_bluetooth.pairing = 0;
        g_bluetooth.last_pair_status = 1;
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status),
                  "pair: discovered device index missing");
        driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
        return -1;
    }

    bluetooth_load_bonds_once();
    bluetooth_pair_started(index);
    rc = xhci_bluetooth_pair(index);
    if (rc != 0 && g_bluetooth.pairing) {
        g_bluetooth.pairing = 0;
        g_bluetooth.last_pair_status = 1;
        copy_cstr(g_bluetooth.status, sizeof(g_bluetooth.status), "warn: pair transport failed");
        driver_update_system_status(g_bluetooth.driver, g_bluetooth.status);
    }
    return rc;
}

int bluetooth_get_discovered(size_t index, struct bluetooth_discovered_device* out) {
    size_t seen = 0;

    if (!out) {
        return -1;
    }

    for (size_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
        if (!g_discovered[i].valid) {
            continue;
        }
        if (seen == index) {
            *out = g_discovered[i];
            return 0;
        }
        seen++;
    }
    return -1;
}

int bluetooth_get_status(struct bluetooth_status* out) {
    if (!out) {
        return -1;
    }
    if (!g_registered) {
        bluetooth_register_driver();
    }
    *out = g_bluetooth;
    return 0;
}

int bluetooth_get_bond(size_t index, struct bluetooth_bond_info* out) {
    size_t seen = 0;

    if (!out) {
        return -1;
    }
    bluetooth_load_bonds_once();

    for (size_t i = 0; i < BLUETOOTH_MAX_BONDS; i++) {
        if (!g_bonds[i].info.valid) {
            continue;
        }
        if (seen == index) {
            *out = g_bonds[i].info;
            return 0;
        }
        seen++;
    }
    return -1;
}
