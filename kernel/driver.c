/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <pci.h>

static struct driver_device g_driver_devices[DRIVER_MAX_DEVICES];
static size_t g_driver_count;

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

void driver_init(void) {
    local_memset(g_driver_devices, 0, sizeof(g_driver_devices));
    g_driver_count = 0;
}

void driver_import_pci_devices(void) {
    size_t count = pci_count();

    for (size_t i = 0; i < count && g_driver_count < DRIVER_MAX_DEVICES; i++) {
        const struct pci_device* pci = pci_get(i);
        struct driver_device* dev;
        if (!pci) continue;

        dev = &g_driver_devices[g_driver_count++];
        local_memset(dev, 0, sizeof(*dev));
        dev->type = DRIVER_DEVICE_PCI;
        dev->bus = pci->bus;
        dev->slot = pci->slot;
        dev->function = pci->function;
        dev->class_code = pci->class_code;
        dev->subclass = pci->subclass;
        dev->prog_if = pci->prog_if;
        dev->irq_line = pci->irq_line;
        dev->vendor_id = pci->vendor_id;
        dev->device_id = pci->device_id;
        for (size_t bar = 0; bar < 6; bar++) {
            dev->bar[bar] = pci->bar[bar];
        }
        copy_string(dev->driver, sizeof(dev->driver), "unclaimed");
        copy_string(dev->status, sizeof(dev->status), "waiting for driver");
    }
}

int driver_claim_pci(uint16_t vendor_id, uint16_t device_id, const char* driver, const char* status) {
    int claimed = 0;

    for (size_t i = 0; i < g_driver_count; i++) {
        struct driver_device* dev = &g_driver_devices[i];
        if (dev->type != DRIVER_DEVICE_PCI) continue;
        if (dev->vendor_id != vendor_id || dev->device_id != device_id) continue;

        dev->claimed = 1;
        copy_string(dev->driver, sizeof(dev->driver), driver);
        copy_string(dev->status, sizeof(dev->status), status);
        claimed++;
    }

    return claimed;
}

int driver_update_pci_status(uint16_t vendor_id, uint16_t device_id, const char* status) {
    int updated = 0;

    for (size_t i = 0; i < g_driver_count; i++) {
        struct driver_device* dev = &g_driver_devices[i];
        if (dev->type != DRIVER_DEVICE_PCI) continue;
        if (dev->vendor_id != vendor_id || dev->device_id != device_id) continue;

        copy_string(dev->status, sizeof(dev->status), status);
        updated++;
    }

    return updated;
}

size_t driver_count(void) {
    return g_driver_count;
}

const struct driver_device* driver_get(size_t index) {
    if (index >= g_driver_count) {
        return 0;
    }
    return &g_driver_devices[index];
}
