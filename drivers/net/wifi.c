/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <pci.h>
#include <wifi.h>
#include <stddef.h>
#include <stdint.h>

#define PCI_CLASS_NETWORK 0x02U
#define PCI_SUBCLASS_ETHERNET 0x00U
#define PCI_SUBCLASS_OTHER_NETWORK 0x80U

struct wifi_vendor_name {
    uint16_t vendor_id;
    const char* driver;
    const char* status;
};

static const struct wifi_vendor_name wifi_vendors[] = {
    {0x8086, "aos-iwlwifi", "detected: Intel WiFi, firmware/mac80211 needed"},
    {0x10EC, "aos-rtlwifi", "detected: Realtek WiFi, firmware/mac80211 needed"},
    {0x14E4, "aos-b43", "detected: Broadcom WiFi, firmware/mac80211 needed"},
    {0x168C, "aos-athwifi", "detected: Qualcomm Atheros WiFi, firmware/mac80211 needed"},
    {0x1814, "aos-rt2x00", "detected: Ralink WiFi, firmware/mac80211 needed"},
    {0x14C3, "aos-mtwifi", "detected: MediaTek WiFi, firmware/mac80211 needed"},
};

static const struct wifi_vendor_name* wifi_vendor(uint16_t vendor_id) {
    for (size_t i = 0; i < sizeof(wifi_vendors) / sizeof(wifi_vendors[0]); i++) {
        if (wifi_vendors[i].vendor_id == vendor_id) {
            return &wifi_vendors[i];
        }
    }
    return 0;
}

void wifi_register_driver(void) {
    size_t found = 0;

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
            found += (size_t)driver_claim_pci(dev->vendor_id, dev->device_id, vendor->driver, vendor->status);
        } else {
            found += (size_t)driver_claim_pci(dev->vendor_id, dev->device_id,
                                              "aos-wifi-pci",
                                              "detected: 802.11 controller, chipset driver needed");
        }
    }

    if (found == 0) {
        driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                               "not found: no PCI 802.11 controller; ethernet only");
    } else {
        driver_register_system(DRIVER_CLASS_NETWORK, "aos-wifi",
                               "probe ready: wireless hardware discovered");
    }

    (void)driver_claim_pci_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET,
                                 "aos-net-pci", "detected: non-e1000 ethernet candidate");
}
