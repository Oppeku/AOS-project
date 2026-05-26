/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stddef.h>

#define PCI_MAX_DEVICES 64

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint8_t irq_line;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar[6];
};

void pci_init(void);
size_t pci_count(void);
const struct pci_device* pci_get(size_t index);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);

#endif
