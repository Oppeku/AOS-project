/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <pci.h>
#include <vmm.h>
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);
extern void outb(uint16_t port, uint8_t val);
extern uint64_t p4_table[];

#define E1000_VENDOR_INTEL 0x8086
#define E1000_DEVICE_82540EM 0x100e
#define E1000_REG_CTRL 0x00000
#define E1000_REG_STATUS 0x00008
#define E1000_REG_EERD 0x00014
#define E1000_REG_IMS 0x000D0
#define E1000_REG_IMC 0x000D8
#define E1000_REG_RCTL 0x00100
#define E1000_REG_TCTL 0x00400
#define E1000_REG_TIPG 0x00410
#define E1000_MMIO_SIZE 0x20000ULL
#define E1000_MMIO_VIRT_BASE 0xFFFF800000100000ULL
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PCI_COMMAND_REG 0x04
#define PCI_COMMAND_IO 0x1
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_BUS_MASTER 0x4
#define E1000_CTRL_RST (1U << 26)
#define E1000_TCTL_EN (1U << 1)
#define E1000_TCTL_PSP (1U << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_RCTL_EN (1U << 1)
#define E1000_RCTL_SBP (1U << 2)
#define E1000_RCTL_UPE (1U << 3)
#define E1000_RCTL_MPE (1U << 4)
#define E1000_RCTL_LBM_NONE (0U << 6)
#define E1000_RCTL_RDMTS_HALF (0U << 8)
#define E1000_RCTL_BAM (1U << 15)
#define E1000_RCTL_SECRC (1U << 26)
#define E1000_RCTL_BSIZE_2048 (0U << 16)

static volatile uint8_t* g_e1000_mmio;

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

static uint32_t mmio_read32(uint32_t offset) {
    return *(volatile uint32_t*)(g_e1000_mmio + offset);
}

static void mmio_write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_e1000_mmio + offset) = value;
}

static void io_wait(void) {
    outb(0x80, 0);
}

static void pci_enable_bus_mastering(const struct pci_device* dev) {
    uint32_t command;

    command = pci_config_read32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG);
    command |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER);
    pci_config_write32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG, command);
}

static void map_mmio_window(uint64_t phys_base, uint64_t virt_base, uint64_t size) {
    uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_WRITE_THROUGH | PAGE_CACHE_DISABLE;
    for (uint64_t off = 0; off < size; off += 4096ULL) {
        vmm_map_page(p4_table, virt_base + off, phys_base + off, flags);
    }
}

static const struct pci_device* find_e1000(void) {
    size_t count = pci_count();

    for (size_t i = 0; i < count; i++) {
        const struct pci_device* dev = pci_get(i);
        if (!dev) continue;
        if (dev->vendor_id == E1000_VENDOR_INTEL && dev->device_id == E1000_DEVICE_82540EM) {
            return dev;
        }
    }

    return 0;
}

static void e1000_reset(void) {
    uint32_t ctrl;
    int tries;

    ctrl = mmio_read32(E1000_REG_CTRL);
    mmio_write32(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);
    io_wait();

    for (tries = 0; tries < 10000; tries++) {
        if ((mmio_read32(E1000_REG_CTRL) & E1000_CTRL_RST) == 0) {
            break;
        }
    }
}

static void e1000_init_regs(void) {
    uint32_t status;

    mmio_write32(E1000_REG_IMC, 0xFFFFFFFFU);
    mmio_write32(E1000_REG_TCTL, 0);
    mmio_write32(E1000_REG_RCTL, 0);
    mmio_write32(E1000_REG_TIPG, 10 | (8 << 10) | (6 << 20));
    mmio_write32(E1000_REG_TCTL,
                 E1000_TCTL_EN |
                 E1000_TCTL_PSP |
                 (0x10U << E1000_TCTL_CT_SHIFT) |
                 (0x40U << E1000_TCTL_COLD_SHIFT));
    mmio_write32(E1000_REG_RCTL,
                 E1000_RCTL_EN |
                 E1000_RCTL_BAM |
                 E1000_RCTL_SECRC |
                 E1000_RCTL_BSIZE_2048 |
                 E1000_RCTL_LBM_NONE |
                 E1000_RCTL_RDMTS_HALF |
                 E1000_RCTL_SBP);
    mmio_write32(E1000_REG_IMS, 0x0000001FU);
    status = mmio_read32(E1000_REG_STATUS);
    driver_update_pci_status(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "mmio ready: ctrl/tctl/rctl programmed");
    serial_print("e1000: STATUS=0x");
    serial_print_hex32(status);
    serial_print(" init complete\n");
}

void e1000_register_driver(void) {
    const struct pci_device* dev = find_e1000();
    uint32_t raw_bar0;
    uint64_t phys_base;
    uint64_t page_base;
    uint64_t page_offset;
    int count = driver_claim_pci(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "e1000", "claimed: pci stub");

    if (count <= 0 || !dev) {
        serial_print("e1000: no supported PCI device found\n");
        return;
    }

    pci_enable_bus_mastering(dev);

    raw_bar0 = dev->bar[0];
    if (raw_bar0 & 1U) {
        driver_update_pci_status(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "claimed: io BAR not supported yet");
        serial_print("e1000: BAR0 is I/O space, MMIO not available\n");
        return;
    }

    phys_base = (uint64_t)(raw_bar0 & 0xFFFFFFF0U);
    page_base = phys_base & ~0xFFFULL;
    page_offset = phys_base & 0xFFFULL;

    map_mmio_window(page_base, E1000_MMIO_VIRT_BASE, E1000_MMIO_SIZE + page_offset);
    g_e1000_mmio = (volatile uint8_t*)(E1000_MMIO_VIRT_BASE + page_offset);
    e1000_reset();
    e1000_init_regs();
    serial_print("e1000: MMIO BAR0=0x");
    serial_print_hex32((uint32_t)phys_base);
    serial_print(" bus mastering enabled\n");
}
