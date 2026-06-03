/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <netdev.h>
#include <pci.h>
#include <pmm.h>
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
#define E1000_REG_RDBAL 0x02800
#define E1000_REG_RDBAH 0x02804
#define E1000_REG_RDLEN 0x02808
#define E1000_REG_RDH 0x02810
#define E1000_REG_RDT 0x02818
#define E1000_REG_TDBAL 0x03800
#define E1000_REG_TDBAH 0x03804
#define E1000_REG_TDLEN 0x03808
#define E1000_REG_TDH 0x03810
#define E1000_REG_TDT 0x03818
#define E1000_REG_TCTL 0x00400
#define E1000_REG_TIPG 0x00410
#define E1000_REG_RAL0 0x05400
#define E1000_REG_RAH0 0x05404
#define E1000_MMIO_SIZE 0x20000ULL
#define E1000_MMIO_VIRT_BASE 0xFFFF800000100000ULL
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PCI_COMMAND_REG 0x04
#define PCI_COMMAND_IO 0x1
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_BUS_MASTER 0x4
#define E1000_CTRL_RST (1U << 26)
#define E1000_STATUS_LU (1U << 1)
#define E1000_EERD_START (1U << 0)
#define E1000_EERD_DONE (1U << 4)
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_DATA_SHIFT 16
#define E1000_TCTL_EN (1U << 1)
#define E1000_TCTL_PSP (1U << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TX_CMD_EOP (1U << 0)
#define E1000_TX_CMD_IFCS (1U << 1)
#define E1000_TX_CMD_RS (1U << 3)
#define E1000_TX_STATUS_DD (1U << 0)
#define E1000_RX_STATUS_DD (1U << 0)
#define E1000_RCTL_EN (1U << 1)
#define E1000_RCTL_SBP (1U << 2)
#define E1000_RCTL_UPE (1U << 3)
#define E1000_RCTL_MPE (1U << 4)
#define E1000_RCTL_LBM_NONE (0U << 6)
#define E1000_RCTL_RDMTS_HALF (0U << 8)
#define E1000_RCTL_BAM (1U << 15)
#define E1000_RCTL_SECRC (1U << 26)
#define E1000_RCTL_BSIZE_2048 (0U << 16)
#define E1000_RAH_AV (1U << 31)
#define E1000_RX_DESC_COUNT 32
#define E1000_TX_DESC_COUNT 32
#define E1000_PACKET_BUFFER_SIZE 2048

static volatile uint8_t* g_e1000_mmio;
static uint32_t g_rx_tail;
static uint32_t g_tx_tail;

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

static struct e1000_rx_desc* g_rx_descs;
static struct e1000_tx_desc* g_tx_descs;
static uint8_t* g_rx_buffers[E1000_RX_DESC_COUNT];
static uint8_t* g_tx_buffers[E1000_TX_DESC_COUNT];
static uint32_t g_rx_head;

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
    status = mmio_read32(E1000_REG_STATUS);
    driver_update_pci_status(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "mmio ready: registers mapped");
    serial_print("e1000: STATUS=0x");
    serial_print_hex32(status);
    serial_print(" mmio ready\n");
}

static void* alloc_zero_page(void) {
    void* page = pmm_alloc_block();
    if (page) {
        local_memset(page, 0, 4096);
    }
    return page;
}

static int e1000_init_rx_ring(void) {
    uint64_t desc_phys;

    g_rx_descs = (struct e1000_rx_desc*)alloc_zero_page();
    if (!g_rx_descs) return -1;

    for (uint32_t i = 0; i < E1000_RX_DESC_COUNT; i++) {
        g_rx_buffers[i] = (uint8_t*)alloc_zero_page();
        if (!g_rx_buffers[i]) return -1;
        g_rx_descs[i].addr = (uint64_t)g_rx_buffers[i];
        g_rx_descs[i].status = 0;
    }

    desc_phys = (uint64_t)g_rx_descs;
    mmio_write32(E1000_REG_RDBAL, (uint32_t)(desc_phys & 0xFFFFFFFFU));
    mmio_write32(E1000_REG_RDBAH, (uint32_t)(desc_phys >> 32));
    mmio_write32(E1000_REG_RDLEN, E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
    mmio_write32(E1000_REG_RDH, 0);
    g_rx_head = 0;
    g_rx_tail = E1000_RX_DESC_COUNT - 1;
    mmio_write32(E1000_REG_RDT, g_rx_tail);
    return 0;
}

static int e1000_init_tx_ring(void) {
    uint64_t desc_phys;

    g_tx_descs = (struct e1000_tx_desc*)alloc_zero_page();
    if (!g_tx_descs) return -1;

    for (uint32_t i = 0; i < E1000_TX_DESC_COUNT; i++) {
        g_tx_buffers[i] = (uint8_t*)alloc_zero_page();
        if (!g_tx_buffers[i]) return -1;
        g_tx_descs[i].addr = (uint64_t)g_tx_buffers[i];
        g_tx_descs[i].status = E1000_TX_STATUS_DD;
    }

    desc_phys = (uint64_t)g_tx_descs;
    mmio_write32(E1000_REG_TDBAL, (uint32_t)(desc_phys & 0xFFFFFFFFU));
    mmio_write32(E1000_REG_TDBAH, (uint32_t)(desc_phys >> 32));
    mmio_write32(E1000_REG_TDLEN, E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
    mmio_write32(E1000_REG_TDH, 0);
    g_tx_tail = 0;
    mmio_write32(E1000_REG_TDT, g_tx_tail);
    return 0;
}

static void e1000_enable_rx_tx(void) {
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
}

static uint16_t e1000_read_eeprom_word(uint8_t address) {
    uint32_t value;

    mmio_write32(E1000_REG_EERD,
                 E1000_EERD_START | ((uint32_t)address << E1000_EERD_ADDR_SHIFT));

    for (int tries = 0; tries < 10000; tries++) {
        value = mmio_read32(E1000_REG_EERD);
        if (value & E1000_EERD_DONE) {
            return (uint16_t)(value >> E1000_EERD_DATA_SHIFT);
        }
    }

    return 0;
}

static void e1000_read_mac(uint8_t mac[6]) {
    uint16_t w0 = e1000_read_eeprom_word(0);
    uint16_t w1 = e1000_read_eeprom_word(1);
    uint16_t w2 = e1000_read_eeprom_word(2);

    mac[0] = (uint8_t)(w0 & 0xFF);
    mac[1] = (uint8_t)(w0 >> 8);
    mac[2] = (uint8_t)(w1 & 0xFF);
    mac[3] = (uint8_t)(w1 >> 8);
    mac[4] = (uint8_t)(w2 & 0xFF);
    mac[5] = (uint8_t)(w2 >> 8);
}

static void e1000_program_receive_address(const uint8_t mac[6]) {
    uint32_t ral;
    uint32_t rah;

    ral = ((uint32_t)mac[0]) |
          ((uint32_t)mac[1] << 8) |
          ((uint32_t)mac[2] << 16) |
          ((uint32_t)mac[3] << 24);
    rah = ((uint32_t)mac[4]) |
          ((uint32_t)mac[5] << 8) |
          E1000_RAH_AV;

    mmio_write32(E1000_REG_RAL0, ral);
    mmio_write32(E1000_REG_RAH0, rah);
}

static int e1000_send_frame(const uint8_t* frame, uint16_t length) {
    uint32_t index;
    volatile struct e1000_tx_desc* desc;

    if (!g_tx_descs || !frame || length == 0 || length > E1000_PACKET_BUFFER_SIZE) {
        return -1;
    }

    index = g_tx_tail;
    desc = (volatile struct e1000_tx_desc*)&g_tx_descs[index];
    if ((desc->status & E1000_TX_STATUS_DD) == 0) {
        return -2;
    }

    local_memcpy(g_tx_buffers[index], frame, length);
    desc->length = length;
    desc->cso = 0;
    desc->cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
    desc->status = 0;
    desc->css = 0;
    desc->special = 0;

    g_tx_tail = (index + 1) % E1000_TX_DESC_COUNT;
    mmio_write32(E1000_REG_TDT, g_tx_tail);

    for (int tries = 0; tries < 100000; tries++) {
        if (desc->status & E1000_TX_STATUS_DD) {
            return length;
        }
    }

    return -3;
}

static int e1000_recv_frame(uint8_t* frame, uint16_t max_length) {
    volatile struct e1000_rx_desc* desc;
    uint16_t length;

    if (!g_rx_descs || !frame || max_length == 0) {
        return -1;
    }

    desc = (volatile struct e1000_rx_desc*)&g_rx_descs[g_rx_head];
    if ((desc->status & E1000_RX_STATUS_DD) == 0) {
        return 0;
    }

    length = desc->length;
    if (length > max_length) {
        length = max_length;
    }
    local_memcpy(frame, g_rx_buffers[g_rx_head], length);
    desc->status = 0;

    g_rx_tail = g_rx_head;
    mmio_write32(E1000_REG_RDT, g_rx_tail);
    g_rx_head = (g_rx_head + 1) % E1000_RX_DESC_COUNT;
    return length;
}

void e1000_register_driver(void) {
    const struct pci_device* dev = find_e1000();
    uint32_t raw_bar0;
    uint64_t phys_base;
    uint64_t page_base;
    uint64_t page_offset;
    uint8_t mac[6];
    uint8_t link_up;
    int net_index;
    const uint8_t qemu_ip[4] = {10, 0, 2, 15};
    const uint8_t qemu_gateway[4] = {10, 0, 2, 2};
    const uint8_t qemu_dns[4] = {10, 0, 2, 3};
    int count = driver_claim_pci(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "aos-e1000", "claimed: pci stub");

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
    if (e1000_init_rx_ring() != 0 || e1000_init_tx_ring() != 0) {
        driver_update_pci_status(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "error: tx/rx ring allocation failed");
        serial_print("e1000: failed to allocate descriptor rings\n");
        return;
    }
    e1000_read_mac(mac);
    e1000_program_receive_address(mac);
    e1000_enable_rx_tx();
    mmio_write32(E1000_REG_IMS, 0x0000001FU);
    driver_update_pci_status(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM, "ready: tx/rx rings online");
    link_up = (mmio_read32(E1000_REG_STATUS) & E1000_STATUS_LU) ? 1 : 0;
    net_index = netdev_register_ethernet("eth0", "aos-e1000", mac, link_up,
                                         dev->bus, dev->slot, dev->function,
                                         link_up ? "link up: tx/rx rings ready" : "link down: tx/rx rings ready",
                                         e1000_send_frame, e1000_recv_frame);
    if (net_index >= 0) {
        netdev_configure_ipv4((size_t)net_index, qemu_ip, 24, qemu_gateway, qemu_dns);
        netdev_configure_ipv6_link_local((size_t)net_index);
    }
    serial_print("e1000: MMIO BAR0=0x");
    serial_print_hex32((uint32_t)phys_base);
    serial_print(" bus mastering enabled\n");
}
