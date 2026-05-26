/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef DRIVER_H
#define DRIVER_H

#include <stdint.h>
#include <stddef.h>

#define DRIVER_MAX_DEVICES 64
#define DRIVER_NAME_MAX 32
#define DRIVER_STATUS_MAX 64

#define DRIVER_DEVICE_PCI 1

struct driver_device {
    uint8_t type;
    uint8_t claimed;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t irq_line;
    uint8_t reserved[7];
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar[6];
    char driver[DRIVER_NAME_MAX];
    char status[DRIVER_STATUS_MAX];
};

void driver_init(void);
void driver_import_pci_devices(void);
int driver_claim_pci(uint16_t vendor_id, uint16_t device_id, const char* driver, const char* status);
int driver_update_pci_status(uint16_t vendor_id, uint16_t device_id, const char* status);
size_t driver_count(void);
const struct driver_device* driver_get(size_t index);

#endif
