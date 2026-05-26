/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <pci.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static struct pci_device g_pci_devices[PCI_MAX_DEVICES];
static size_t g_pci_count;

extern void outb(uint16_t port, uint8_t val);
extern void serial_print(const char* s);
extern uint8_t aos_boot_verbose;

static inline void outl_local(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl_local(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void serial_print_hex8(uint8_t value) {
    const char* hex = "0123456789abcdef";
    char out[3];
    out[0] = hex[(value >> 4) & 0xF];
    out[1] = hex[value & 0xF];
    out[2] = '\0';
    serial_print(out);
}

static void serial_print_hex16(uint16_t value) {
    serial_print_hex8((uint8_t)(value >> 8));
    serial_print_hex8((uint8_t)value);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFC);

    outl_local(PCI_CONFIG_ADDRESS, address);
    return inl_local(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address =
        (1U << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFC);

    outl_local(PCI_CONFIG_ADDRESS, address);
    outl_local(PCI_CONFIG_DATA, value);
}

static uint16_t pci_vendor_id(uint8_t bus, uint8_t slot, uint8_t function) {
    return (uint16_t)(pci_config_read32(bus, slot, function, 0x00) & 0xFFFF);
}

static void pci_add_device(uint8_t bus, uint8_t slot, uint8_t function) {
    uint32_t id;
    uint32_t class_reg;
    uint32_t header_reg;
    struct pci_device* dev;

    if (g_pci_count >= PCI_MAX_DEVICES) {
        return;
    }

    id = pci_config_read32(bus, slot, function, 0x00);
    if ((id & 0xFFFF) == 0xFFFF) {
        return;
    }

    dev = &g_pci_devices[g_pci_count++];
    local_memset(dev, 0, sizeof(*dev));

    dev->bus = bus;
    dev->slot = slot;
    dev->function = function;
    dev->vendor_id = (uint16_t)(id & 0xFFFF);
    dev->device_id = (uint16_t)(id >> 16);

    class_reg = pci_config_read32(bus, slot, function, 0x08);
    dev->revision = (uint8_t)(class_reg & 0xFF);
    dev->prog_if = (uint8_t)((class_reg >> 8) & 0xFF);
    dev->subclass = (uint8_t)((class_reg >> 16) & 0xFF);
    dev->class_code = (uint8_t)((class_reg >> 24) & 0xFF);

    header_reg = pci_config_read32(bus, slot, function, 0x0C);
    dev->header_type = (uint8_t)((header_reg >> 16) & 0xFF);
    dev->irq_line = (uint8_t)(pci_config_read32(bus, slot, function, 0x3C) & 0xFF);

    for (uint8_t i = 0; i < 6; i++) {
        dev->bar[i] = pci_config_read32(bus, slot, function, (uint8_t)(0x10 + i * 4));
    }

    if (aos_boot_verbose) {
        serial_print("PCI: ");
        serial_print_hex8(bus);
        serial_print(":");
        serial_print_hex8(slot);
        serial_print(".");
        serial_print_hex8(function);
        serial_print(" vendor=");
        serial_print_hex16(dev->vendor_id);
        serial_print(" device=");
        serial_print_hex16(dev->device_id);
        serial_print(" class=");
        serial_print_hex8(dev->class_code);
        serial_print(":");
        serial_print_hex8(dev->subclass);
        serial_print("\n");
    }
}

void pci_init(void) {
    local_memset(g_pci_devices, 0, sizeof(g_pci_devices));
    g_pci_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            if (pci_vendor_id((uint8_t)bus, slot, 0) == 0xFFFF) {
                continue;
            }
            for (uint8_t function = 0; function < 8; function++) {
                pci_add_device((uint8_t)bus, slot, function);
            }
        }
    }
}

size_t pci_count(void) {
    return g_pci_count;
}

const struct pci_device* pci_get(size_t index) {
    if (index >= g_pci_count) {
        return 0;
    }
    return &g_pci_devices[index];
}
