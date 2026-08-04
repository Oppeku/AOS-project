/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <driver.h>
#include <bluetooth.h>
#include <gfx.h>
#include <input.h>
#include <pci.h>
#include <pmm.h>
#include <stdint.h>
#include <vmm.h>
#include <xhci.h>

extern void serial_print(const char* s);
extern uint64_t p4_table[];

#define PCI_COMMAND_REG 0x04
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_BUS_MASTER 0x4

#define XHCI_CLASS_SERIAL_BUS 0x0c
#define XHCI_SUBCLASS_USB 0x03
#define XHCI_PROGIF_XHCI 0x30

#define XHCI_MMIO_SIZE 0x10000ULL
#define XHCI_MMIO_VIRT_BASE 0xFFFF800000300000ULL
#define PAGE_WRITE_THROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define XHCI_PORT_REGS_BASE 0x400U
#define XHCI_PORT_REG_STRIDE 0x10U
#define XHCI_PORTSC_CCS (1U << 0)
#define XHCI_PORTSC_PED (1U << 1)
#define XHCI_PORTSC_PR (1U << 4)
#define XHCI_PORTSC_SPEED_SHIFT 10
#define XHCI_PORTSC_SPEED_MASK 0xFU
#define XHCI_TRB_COUNT 256
#define XHCI_EVENT_TRB_COUNT 256
#define XHCI_USBCMD_RS (1U << 0)
#define XHCI_USBCMD_HCRST (1U << 1)
#define XHCI_USBSTS_HCH (1U << 0)
#define XHCI_USBSTS_CNR (1U << 11)
#define XHCI_TRB_TYPE_LINK 6U
#define XHCI_TRB_TYPE_SETUP_STAGE 2U
#define XHCI_TRB_TYPE_DATA_STAGE 3U
#define XHCI_TRB_TYPE_STATUS_STAGE 4U
#define XHCI_TRB_TYPE_ENABLE_SLOT 9U
#define XHCI_TRB_TYPE_ADDRESS_DEVICE 11U
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT 12U
#define XHCI_TRB_TYPE_NORMAL 1U
#define XHCI_TRB_TYPE_TRANSFER_EVENT 32U
#define XHCI_TRB_TYPE_COMMAND_COMPLETION 33U
#define XHCI_TRB_CYCLE (1U << 0)
#define XHCI_TRB_IOC (1U << 5)
#define XHCI_TRB_IDT (1U << 6)
#define XHCI_TRB_DATA_DIR_IN (1U << 16)
#define XHCI_TRB_STATUS_DIR_IN (1U << 16)
#define XHCI_TRB_LINK_TOGGLE_CYCLE (1U << 1)
#define XHCI_TRB_TYPE_SHIFT 10
#define XHCI_TRB_SETUP_TRT_NO_DATA (0U << 16)
#define XHCI_TRB_SETUP_TRT_OUT_DATA (2U << 16)
#define XHCI_TRB_SETUP_TRT_IN_DATA (3U << 16)
#define XHCI_COMPLETION_SUCCESS 1U
#define XHCI_COMPLETION_SHORT_PACKET 13U
#define XHCI_HCCPARAMS1_CSZ (1U << 2)
#define XHCI_SLOT_CTX_ENTRIES_SHIFT 27
#define XHCI_SLOT_CTX_SPEED_SHIFT 20
#define XHCI_SLOT_CTX_ROOT_PORT_SHIFT 16
#define XHCI_EP_TYPE_CONTROL 4U
#define XHCI_EP_TYPE_INTERRUPT_IN 7U
#define XHCI_EP_CTX_CERR_SHIFT 1
#define XHCI_EP_CTX_TYPE_SHIFT 3
#define XHCI_EP_CTX_INTERVAL_SHIFT 16
#define XHCI_EP_CTX_MAX_PACKET_SHIFT 16
#define XHCI_ADDRESS_DEVICE_BSR (1U << 9)
#define XHCI_INTERRUPT_OWNER_NONE 0
#define XHCI_INTERRUPT_OWNER_KEYBOARD 1
#define XHCI_INTERRUPT_OWNER_BLUETOOTH 2
#define XHCI_INTERRUPT_OWNER_MOUSE 3
static volatile uint8_t* g_xhci_mmio;

struct xhci_trb {
    uint32_t parameter_lo;
    uint32_t parameter_hi;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

struct xhci_erst_entry {
    uint64_t ring_segment_base;
    uint32_t ring_segment_size;
    uint32_t reserved;
} __attribute__((packed));

static uint32_t g_xhci_op_base;
static uint32_t g_xhci_db_base;
static uint32_t g_xhci_rt_base;
static struct xhci_trb* g_command_ring;
static struct xhci_trb* g_event_ring;
static struct xhci_erst_entry* g_erst;
static uint64_t* g_dcbaa;
static uint32_t g_command_enqueue;
static uint32_t g_event_dequeue;
static uint8_t g_command_cycle;
static uint8_t g_event_cycle;
static struct xhci_trb* g_ep0_ring;
static uint8_t* g_device_descriptor;
static uint8_t* g_config_descriptor;
static uint32_t g_ep0_enqueue;
static uint8_t g_ep0_cycle;
static uint32_t g_context_size;
static uint8_t g_first_connected_port;
static uint8_t g_first_connected_speed;
static uint8_t g_interrupt_in_endpoint;
static uint16_t g_interrupt_in_max_packet;
static uint8_t g_interrupt_in_interval;
static uint8_t g_interrupt_in_owner;
static struct xhci_trb* g_interrupt_in_ring;
static uint8_t g_interrupt_in_cycle;
static uint32_t g_interrupt_in_enqueue;
static uint32_t g_interrupt_in_dci;
static uint32_t g_keyboard_slot_id;
static uint8_t* g_keyboard_report;
static uint64_t g_keyboard_pending_trb;
static uint8_t g_keyboard_last_keys[6];
static uint32_t g_mouse_slot_id;
static uint8_t* g_mouse_report;
static uint64_t g_mouse_pending_trb;
static int32_t g_mouse_x = 512;
static int32_t g_mouse_y = 384;
static uint32_t g_mouse_buttons;
static uint32_t g_bluetooth_slot_id;
static uint8_t g_bluetooth_interface_number;
static uint8_t* g_bluetooth_command_buffer;
static uint8_t* g_bluetooth_event_buffer;
static uint16_t g_bluetooth_event_buffer_len;
static uint64_t g_bluetooth_pending_trb;

struct xhci_command_result {
    uint32_t completion_code;
    uint32_t slot_id;
    uint64_t command_phys;
};

struct xhci_transfer_result {
    uint32_t completion_code;
    uint32_t transfer_length;
    uint32_t endpoint_id;
    uint64_t trb_phys;
};

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

static void serial_print_hex32(uint32_t value) {
    serial_print_hex8((uint8_t)(value >> 24));
    serial_print_hex8((uint8_t)(value >> 16));
    serial_print_hex8((uint8_t)(value >> 8));
    serial_print_hex8((uint8_t)value);
}

static uint32_t mmio_read32(uint32_t offset) {
    return *(volatile uint32_t*)(g_xhci_mmio + offset);
}

static void mmio_write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_xhci_mmio + offset) = value;
}

static void mmio_write64(uint32_t offset, uint64_t value) {
    mmio_write32(offset, (uint32_t)(value & 0xFFFFFFFFU));
    mmio_write32(offset + 4, (uint32_t)(value >> 32));
}

static void local_memset(void* dst, int value, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
}

static void local_memcpy(void* dst, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
}

static void map_mmio_window(uint64_t phys_base, uint64_t virt_base, uint64_t size) {
    uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE | PAGE_WRITE_THROUGH | PAGE_CACHE_DISABLE;
    for (uint64_t off = 0; off < size; off += 4096ULL) {
        vmm_map_page(p4_table, virt_base + off, phys_base + off, flags);
    }
}

static void pci_enable_xhci(const struct pci_device* dev) {
    uint32_t command = pci_config_read32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG);
    command |= (PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER);
    pci_config_write32(dev->bus, dev->slot, dev->function, PCI_COMMAND_REG, command);
}

static const struct pci_device* find_xhci(void) {
    size_t count = pci_count();

    for (size_t i = 0; i < count; i++) {
        const struct pci_device* dev = pci_get(i);
        if (!dev) continue;
        if (dev->class_code == XHCI_CLASS_SERIAL_BUS &&
            dev->subclass == XHCI_SUBCLASS_USB &&
            dev->prog_if == XHCI_PROGIF_XHCI) {
            return dev;
        }
    }

    return 0;
}

static void* alloc_zero_page(void) {
    void* page = pmm_alloc_block();
    if (page) {
        local_memset(page, 0, 4096);
    }
    return page;
}

static void xhci_print_speed(uint32_t speed) {
    switch (speed) {
        case 1:
            serial_print("full");
            break;
        case 2:
            serial_print("low");
            break;
        case 3:
            serial_print("high");
            break;
        case 4:
            serial_print("super");
            break;
        default:
            serial_print("unknown");
            break;
    }
}

static uint32_t xhci_scan_ports(uint8_t cap_length, uint8_t max_ports) {
    uint32_t connected = 0;
    uint32_t op_base = cap_length;

    serial_print("xhci: root hub ports\n");
    for (uint8_t port = 0; port < max_ports; port++) {
        uint32_t portsc = mmio_read32(op_base + XHCI_PORT_REGS_BASE + ((uint32_t)port * XHCI_PORT_REG_STRIDE));
        uint32_t speed = (portsc >> XHCI_PORTSC_SPEED_SHIFT) & XHCI_PORTSC_SPEED_MASK;

        serial_print("xhci: port ");
        serial_print_hex8((uint8_t)(port + 1));
        serial_print(" status=0x");
        serial_print_hex32(portsc);
        serial_print(" connected=");
        if (portsc & XHCI_PORTSC_CCS) {
            connected++;
            if (g_first_connected_port == 0) {
                g_first_connected_port = (uint8_t)(port + 1);
                g_first_connected_speed = (uint8_t)speed;
            }
            serial_print("yes");
        } else {
            serial_print("no");
        }
        serial_print(" enabled=");
        serial_print((portsc & XHCI_PORTSC_PED) ? "yes" : "no");
        serial_print(" reset=");
        serial_print((portsc & XHCI_PORTSC_PR) ? "yes" : "no");
        serial_print(" speed=");
        xhci_print_speed(speed);
        serial_print("\n");
    }

    return connected;
}

static int xhci_wait_bits(uint32_t offset, uint32_t mask, uint32_t value) {
    for (uint32_t tries = 0; tries < 100000; tries++) {
        if ((mmio_read32(offset) & mask) == value) {
            return 0;
        }
    }
    return -1;
}

static void xhci_advance_event(void) {
    g_event_dequeue++;
    if (g_event_dequeue >= XHCI_EVENT_TRB_COUNT) {
        g_event_dequeue = 0;
        g_event_cycle ^= 1;
    }
    mmio_write64(g_xhci_rt_base + 0x20U + 0x18U, ((uint64_t)&g_event_ring[g_event_dequeue]) | 0x8U);
}

static int xhci_setup_rings(uint8_t max_slots) {
    uint64_t command_phys;
    uint64_t event_phys;
    uint64_t erst_phys;
    uint64_t dcbaa_phys;
    uint32_t interrupter0 = g_xhci_rt_base + 0x20U;

    g_command_ring = (struct xhci_trb*)alloc_zero_page();
    g_event_ring = (struct xhci_trb*)alloc_zero_page();
    g_erst = (struct xhci_erst_entry*)alloc_zero_page();
    g_dcbaa = (uint64_t*)alloc_zero_page();
    if (!g_command_ring || !g_event_ring || !g_erst || !g_dcbaa) {
        serial_print("xhci: ring allocation failed\n");
        return -1;
    }

    command_phys = (uint64_t)g_command_ring;
    event_phys = (uint64_t)g_event_ring;
    erst_phys = (uint64_t)g_erst;
    dcbaa_phys = (uint64_t)g_dcbaa;

    g_command_enqueue = 0;
    g_event_dequeue = 0;
    g_command_cycle = 1;
    g_event_cycle = 1;

    g_command_ring[XHCI_TRB_COUNT - 1].parameter_lo = (uint32_t)(command_phys & 0xFFFFFFFFU);
    g_command_ring[XHCI_TRB_COUNT - 1].parameter_hi = (uint32_t)(command_phys >> 32);
    g_command_ring[XHCI_TRB_COUNT - 1].status = 0;
    g_command_ring[XHCI_TRB_COUNT - 1].control =
        XHCI_TRB_CYCLE |
        XHCI_TRB_LINK_TOGGLE_CYCLE |
        (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT);

    g_erst[0].ring_segment_base = event_phys;
    g_erst[0].ring_segment_size = XHCI_EVENT_TRB_COUNT;
    g_erst[0].reserved = 0;

    mmio_write32(g_xhci_op_base + 0x38U, max_slots);
    mmio_write64(g_xhci_op_base + 0x30U, dcbaa_phys);
    mmio_write64(g_xhci_op_base + 0x18U, command_phys | XHCI_TRB_CYCLE);

    mmio_write32(interrupter0 + 0x08U, 1);
    mmio_write64(interrupter0 + 0x10U, erst_phys);
    mmio_write64(interrupter0 + 0x18U, event_phys);

    serial_print("xhci: rings ready cr=0x");
    serial_print_hex32((uint32_t)command_phys);
    serial_print(" er=0x");
    serial_print_hex32((uint32_t)event_phys);
    serial_print("\n");
    return 0;
}

static int xhci_start_controller(uint8_t max_slots) {
    uint32_t usbcmd;

    usbcmd = mmio_read32(g_xhci_op_base + 0x00U);
    mmio_write32(g_xhci_op_base + 0x00U, usbcmd & ~XHCI_USBCMD_RS);
    if (xhci_wait_bits(g_xhci_op_base + 0x04U, XHCI_USBSTS_HCH, XHCI_USBSTS_HCH) < 0) {
        serial_print("xhci: halt timeout\n");
        return -1;
    }

    usbcmd = mmio_read32(g_xhci_op_base + 0x00U);
    mmio_write32(g_xhci_op_base + 0x00U, usbcmd | XHCI_USBCMD_HCRST);
    if (xhci_wait_bits(g_xhci_op_base + 0x00U, XHCI_USBCMD_HCRST, 0) < 0) {
        serial_print("xhci: reset timeout\n");
        return -1;
    }
    if (xhci_wait_bits(g_xhci_op_base + 0x04U, XHCI_USBSTS_CNR, 0) < 0) {
        serial_print("xhci: controller not ready timeout\n");
        return -1;
    }

    if (xhci_setup_rings(max_slots) < 0) {
        return -1;
    }

    mmio_write32(g_xhci_op_base + 0x04U, mmio_read32(g_xhci_op_base + 0x04U));
    mmio_write32(g_xhci_op_base + 0x00U, XHCI_USBCMD_RS);
    if (xhci_wait_bits(g_xhci_op_base + 0x04U, XHCI_USBSTS_HCH, 0) < 0) {
        serial_print("xhci: run timeout\n");
        return -1;
    }

    serial_print("xhci: controller running\n");
    return 0;
}

static void xhci_ring_doorbell(uint32_t target, uint32_t value) {
    mmio_write32(g_xhci_db_base + (target * 4U), value);
}

static int xhci_wait_command_completion(uint64_t expected_cmd_phys, struct xhci_command_result* out) {
    for (uint32_t tries = 0; tries < 1000000; tries++) {
        struct xhci_trb* event = &g_event_ring[g_event_dequeue];
        uint32_t type;

        if ((event->control & XHCI_TRB_CYCLE) != g_event_cycle) {
            continue;
        }

        type = (event->control >> XHCI_TRB_TYPE_SHIFT) & 0x3FU;
        if (type == XHCI_TRB_TYPE_COMMAND_COMPLETION) {
            uint64_t event_cmd_phys = ((uint64_t)event->parameter_hi << 32) | event->parameter_lo;
            uint32_t completion = (event->status >> 24) & 0xFFU;
            uint32_t slot_id = (event->control >> 24) & 0xFFU;

            xhci_advance_event();

            serial_print("xhci: command completion cc=0x");
            serial_print_hex8((uint8_t)completion);
            serial_print(" slot=0x");
            serial_print_hex8((uint8_t)slot_id);
            serial_print(" cmd=0x");
            serial_print_hex32((uint32_t)event_cmd_phys);
            serial_print("\n");

            if (out) {
                out->completion_code = completion;
                out->slot_id = slot_id;
                out->command_phys = event_cmd_phys;
            }

            if (event_cmd_phys != expected_cmd_phys) {
                return -1;
            }
            return 0;
        }

        xhci_advance_event();
    }

    return -1;
}

static struct xhci_trb* xhci_next_command(uint32_t type) {
    struct xhci_trb* cmd = &g_command_ring[g_command_enqueue];
    cmd->parameter_lo = 0;
    cmd->parameter_hi = 0;
    cmd->status = 0;
    cmd->control = XHCI_TRB_CYCLE | (type << XHCI_TRB_TYPE_SHIFT);
    g_command_enqueue++;
    if (g_command_enqueue >= (XHCI_TRB_COUNT - 1)) {
        g_command_enqueue = 0;
        g_command_cycle ^= 1;
    }
    return cmd;
}

static int xhci_enable_slot(void) {
    struct xhci_command_result result;
    struct xhci_trb* cmd = xhci_next_command(XHCI_TRB_TYPE_ENABLE_SLOT);
    uint64_t cmd_phys = (uint64_t)cmd;

    serial_print("xhci: command Enable Slot\n");
    xhci_ring_doorbell(0, 0);

    if (xhci_wait_command_completion(cmd_phys, &result) < 0) {
        serial_print("xhci: enable slot timeout\n");
        return -1;
    }
    if (result.completion_code == XHCI_COMPLETION_SUCCESS) {
        return (int)result.slot_id;
    }
    return -1;
}

static uint32_t xhci_default_ep0_packet_size(uint8_t speed) {
    if (speed == 4) {
        return 512;
    }
    if (speed == 3) {
        return 64;
    }
    return 8;
}

static uint32_t* xhci_context_at(void* base, uint32_t index) {
    return (uint32_t*)((uint8_t*)base + ((uint64_t)index * g_context_size));
}

static struct xhci_trb* xhci_next_ep0_trb(void) {
    struct xhci_trb* trb = &g_ep0_ring[g_ep0_enqueue];
    g_ep0_enqueue++;
    if (g_ep0_enqueue >= (XHCI_TRB_COUNT - 1)) {
        g_ep0_enqueue = 0;
        g_ep0_cycle ^= 1;
    }
    return trb;
}

static int xhci_wait_transfer_event(uint64_t expected_trb_phys, struct xhci_transfer_result* out) {
    for (uint32_t tries = 0; tries < 1000000; tries++) {
        struct xhci_trb* event = &g_event_ring[g_event_dequeue];
        uint32_t type;

        if ((event->control & XHCI_TRB_CYCLE) != g_event_cycle) {
            continue;
        }

        type = (event->control >> XHCI_TRB_TYPE_SHIFT) & 0x3FU;
        if (type == XHCI_TRB_TYPE_TRANSFER_EVENT) {
            uint64_t event_trb_phys = ((uint64_t)event->parameter_hi << 32) | event->parameter_lo;
            uint32_t completion = (event->status >> 24) & 0xFFU;
            uint32_t transfer_length = event->status & 0x00FFFFFFU;
            uint32_t endpoint_id = (event->control >> 16) & 0x1FU;

            xhci_advance_event();

            serial_print("xhci: transfer event cc=0x");
            serial_print_hex8((uint8_t)completion);
            serial_print(" ep=0x");
            serial_print_hex8((uint8_t)endpoint_id);
            serial_print(" trb=0x");
            serial_print_hex32((uint32_t)event_trb_phys);
            serial_print(" residue=0x");
            serial_print_hex32(transfer_length);
            serial_print("\n");

            if (out) {
                out->completion_code = completion;
                out->transfer_length = transfer_length;
                out->endpoint_id = endpoint_id;
                out->trb_phys = event_trb_phys;
            }

            if (event_trb_phys != expected_trb_phys) {
                return -1;
            }
            return 0;
        }

        xhci_advance_event();
    }

    return -1;
}

static int xhci_address_device(uint32_t slot_id) {
    void* input_ctx = alloc_zero_page();
    void* output_ctx = alloc_zero_page();
    uint32_t* input_control;
    uint32_t* slot_ctx;
    uint32_t* ep0_ctx;
    struct xhci_trb* cmd;
    struct xhci_command_result result;
    uint64_t input_phys;
    uint64_t output_phys;
    uint64_t ep0_phys;
    uint32_t max_packet;
    uint64_t cmd_phys;

    g_ep0_ring = (struct xhci_trb*)alloc_zero_page();
    g_device_descriptor = (uint8_t*)alloc_zero_page();
    g_config_descriptor = (uint8_t*)alloc_zero_page();
    if (!input_ctx || !output_ctx || !g_ep0_ring || !g_device_descriptor || !g_config_descriptor || slot_id == 0 || slot_id > 255) {
        serial_print("xhci: address device allocation failed\n");
        return -1;
    }

    input_phys = (uint64_t)input_ctx;
    output_phys = (uint64_t)output_ctx;
    ep0_phys = (uint64_t)g_ep0_ring;
    max_packet = xhci_default_ep0_packet_size(g_first_connected_speed);

    g_ep0_enqueue = 0;
    g_ep0_cycle = 1;
    g_ep0_ring[XHCI_TRB_COUNT - 1].parameter_lo = (uint32_t)(ep0_phys & 0xFFFFFFFFU);
    g_ep0_ring[XHCI_TRB_COUNT - 1].parameter_hi = (uint32_t)(ep0_phys >> 32);
    g_ep0_ring[XHCI_TRB_COUNT - 1].status = 0;
    g_ep0_ring[XHCI_TRB_COUNT - 1].control =
        XHCI_TRB_CYCLE |
        XHCI_TRB_LINK_TOGGLE_CYCLE |
        (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT);

    g_dcbaa[slot_id] = output_phys;

    input_control = xhci_context_at(input_ctx, 0);
    slot_ctx = xhci_context_at(input_ctx, 1);
    ep0_ctx = xhci_context_at(input_ctx, 2);

    input_control[1] = (1U << 0) | (1U << 1);
    slot_ctx[0] =
        ((uint32_t)g_first_connected_speed << XHCI_SLOT_CTX_SPEED_SHIFT) |
        (1U << XHCI_SLOT_CTX_ENTRIES_SHIFT);
    slot_ctx[1] = ((uint32_t)g_first_connected_port << XHCI_SLOT_CTX_ROOT_PORT_SHIFT);
    ep0_ctx[1] = (XHCI_EP_TYPE_CONTROL << XHCI_EP_CTX_TYPE_SHIFT) |
                 (max_packet << XHCI_EP_CTX_MAX_PACKET_SHIFT);
    ep0_ctx[2] = (uint32_t)((ep0_phys | XHCI_TRB_CYCLE) & 0xFFFFFFFFU);
    ep0_ctx[3] = (uint32_t)((ep0_phys | XHCI_TRB_CYCLE) >> 32);
    ep0_ctx[4] = 8;

    cmd = xhci_next_command(XHCI_TRB_TYPE_ADDRESS_DEVICE);
    cmd_phys = (uint64_t)cmd;
    cmd->parameter_lo = (uint32_t)(input_phys & 0xFFFFFFFFU);
    cmd->parameter_hi = (uint32_t)(input_phys >> 32);
    cmd->control |= (slot_id << 24);

    serial_print("xhci: command Address Device slot=0x");
    serial_print_hex8((uint8_t)slot_id);
    serial_print(" port=0x");
    serial_print_hex8(g_first_connected_port);
    serial_print(" mps=0x");
    serial_print_hex16((uint16_t)max_packet);
    serial_print("\n");
    xhci_ring_doorbell(0, 0);

    if (xhci_wait_command_completion(cmd_phys, &result) < 0) {
        serial_print("xhci: address device timeout\n");
        return -1;
    }
    if (result.completion_code == XHCI_COMPLETION_SUCCESS) {
        serial_print("xhci: device addressed slot=0x");
        serial_print_hex8((uint8_t)slot_id);
        serial_print("\n");
        return 0;
    }

    serial_print("xhci: address device failed cc=0x");
    serial_print_hex8((uint8_t)result.completion_code);
    serial_print("\n");
    return -1;
}

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int xhci_control_get_descriptor(uint32_t slot_id, uint8_t descriptor_type, uint8_t descriptor_index,
                                       uint8_t* buffer, uint16_t length) {
    struct xhci_trb* setup;
    struct xhci_trb* data;
    struct xhci_trb* status;
    struct xhci_transfer_result result;
    uint64_t data_phys = (uint64_t)buffer;
    uint64_t status_phys;

    if (!g_ep0_ring || !buffer || slot_id == 0 || length == 0) {
        return -1;
    }

    local_memset(buffer, 0, length);

    setup = xhci_next_ep0_trb();
    setup->parameter_lo = 0x00000680U |
        ((uint32_t)descriptor_index << 16) |
        ((uint32_t)descriptor_type << 24);
    setup->parameter_hi = ((uint32_t)length << 16);
    setup->status = 8;
    setup->control = g_ep0_cycle |
        XHCI_TRB_IDT |
        XHCI_TRB_SETUP_TRT_IN_DATA |
        (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT);

    data = xhci_next_ep0_trb();
    data->parameter_lo = (uint32_t)(data_phys & 0xFFFFFFFFU);
    data->parameter_hi = (uint32_t)(data_phys >> 32);
    data->status = length;
    data->control = g_ep0_cycle |
        XHCI_TRB_DATA_DIR_IN |
        (XHCI_TRB_TYPE_DATA_STAGE << XHCI_TRB_TYPE_SHIFT);

    status = xhci_next_ep0_trb();
    status_phys = (uint64_t)status;
    status->parameter_lo = 0;
    status->parameter_hi = 0;
    status->status = 0;
    status->control = g_ep0_cycle |
        XHCI_TRB_IOC |
        (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT);

    serial_print("xhci: control GET_DESCRIPTOR type=0x");
    serial_print_hex8(descriptor_type);
    serial_print(" len=0x");
    serial_print_hex16(length);
    serial_print(" slot=0x");
    serial_print_hex8((uint8_t)slot_id);
    serial_print("\n");
    xhci_ring_doorbell(slot_id, 1);

    if (xhci_wait_transfer_event(status_phys, &result) < 0) {
        serial_print("xhci: get descriptor timeout\n");
        return -1;
    }
    if (result.completion_code != XHCI_COMPLETION_SUCCESS) {
        serial_print("xhci: get descriptor failed cc=0x");
        serial_print_hex8((uint8_t)result.completion_code);
        serial_print("\n");
        return -1;
    }

    return 0;
}

static int xhci_control_set_configuration(uint32_t slot_id, uint8_t configuration_value) {
    struct xhci_trb* setup;
    struct xhci_trb* status;
    struct xhci_transfer_result result;
    uint64_t status_phys;

    if (!g_ep0_ring || slot_id == 0) {
        return -1;
    }

    setup = xhci_next_ep0_trb();
    setup->parameter_lo = 0x00000900U | ((uint32_t)configuration_value << 16);
    setup->parameter_hi = 0x00000000U;
    setup->status = 8;
    setup->control = g_ep0_cycle |
        XHCI_TRB_IDT |
        XHCI_TRB_SETUP_TRT_NO_DATA |
        (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT);

    status = xhci_next_ep0_trb();
    status_phys = (uint64_t)status;
    status->parameter_lo = 0;
    status->parameter_hi = 0;
    status->status = 0;
    status->control = g_ep0_cycle |
        XHCI_TRB_IOC |
        XHCI_TRB_STATUS_DIR_IN |
        (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT);

    serial_print("xhci: control SET_CONFIGURATION value=0x");
    serial_print_hex8(configuration_value);
    serial_print(" slot=0x");
    serial_print_hex8((uint8_t)slot_id);
    serial_print("\n");
    xhci_ring_doorbell(slot_id, 1);

    if (xhci_wait_transfer_event(status_phys, &result) < 0) {
        serial_print("xhci: set configuration timeout\n");
        return -1;
    }
    if (result.completion_code != XHCI_COMPLETION_SUCCESS) {
        serial_print("xhci: set configuration failed cc=0x");
        serial_print_hex8((uint8_t)result.completion_code);
        serial_print("\n");
        return -1;
    }

    serial_print("xhci: configuration set\n");
    return 0;
}

static int xhci_control_class_out(uint32_t slot_id, uint8_t request, uint16_t value,
                                  uint16_t index, const uint8_t* payload, uint16_t length) {
    struct xhci_trb* setup;
    struct xhci_trb* data;
    struct xhci_trb* status;
    struct xhci_transfer_result result;
    uint64_t status_phys;
    uint64_t data_phys = (uint64_t)payload;

    if (!g_ep0_ring || slot_id == 0 || !payload || length == 0) {
        return -1;
    }

    setup = xhci_next_ep0_trb();
    setup->parameter_lo = 0x00000020U |
        ((uint32_t)request << 8) |
        ((uint32_t)value << 16);
    setup->parameter_hi = ((uint32_t)index) | ((uint32_t)length << 16);
    setup->status = 8;
    setup->control = g_ep0_cycle |
        XHCI_TRB_IDT |
        XHCI_TRB_SETUP_TRT_OUT_DATA |
        (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT);

    data = xhci_next_ep0_trb();
    data->parameter_lo = (uint32_t)(data_phys & 0xFFFFFFFFU);
    data->parameter_hi = (uint32_t)(data_phys >> 32);
    data->status = length;
    data->control = g_ep0_cycle |
        (XHCI_TRB_TYPE_DATA_STAGE << XHCI_TRB_TYPE_SHIFT);

    status = xhci_next_ep0_trb();
    status_phys = (uint64_t)status;
    status->parameter_lo = 0;
    status->parameter_hi = 0;
    status->status = 0;
    status->control = g_ep0_cycle |
        XHCI_TRB_IOC |
        XHCI_TRB_STATUS_DIR_IN |
        (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT);

    serial_print("xhci: control CLASS OUT request=0x");
    serial_print_hex8(request);
    serial_print(" index=0x");
    serial_print_hex16(index);
    serial_print(" len=0x");
    serial_print_hex16(length);
    serial_print(" slot=0x");
    serial_print_hex8((uint8_t)slot_id);
    serial_print("\n");
    xhci_ring_doorbell(slot_id, 1);

    if (xhci_wait_transfer_event(status_phys, &result) < 0) {
        serial_print("xhci: class out timeout\n");
        return -1;
    }
    if (result.completion_code != XHCI_COMPLETION_SUCCESS) {
        serial_print("xhci: class out failed cc=0x");
        serial_print_hex8((uint8_t)result.completion_code);
        serial_print("\n");
        return -1;
    }

    return 0;
}

static int xhci_get_device_descriptor(uint32_t slot_id) {
    if (xhci_control_get_descriptor(slot_id, 1, 0, g_device_descriptor, 18) < 0) {
        return -1;
    }

    serial_print("xhci: device descriptor len=0x");
    serial_print_hex8(g_device_descriptor[0]);
    serial_print(" type=0x");
    serial_print_hex8(g_device_descriptor[1]);
    serial_print(" usb=0x");
    serial_print_hex16(read_le16(&g_device_descriptor[2]));
    serial_print(" class=0x");
    serial_print_hex8(g_device_descriptor[4]);
    serial_print(" subclass=0x");
    serial_print_hex8(g_device_descriptor[5]);
    serial_print(" protocol=0x");
    serial_print_hex8(g_device_descriptor[6]);
    serial_print(" mps=0x");
    serial_print_hex8(g_device_descriptor[7]);
    serial_print(" vendor=0x");
    serial_print_hex16(read_le16(&g_device_descriptor[8]));
    serial_print(" product=0x");
    serial_print_hex16(read_le16(&g_device_descriptor[10]));
    serial_print("\n");
    return 0;
}

static void xhci_parse_config_descriptor(uint8_t* buffer, uint16_t total_length) {
    uint16_t off = 0;
    uint8_t interfaces;
    uint8_t current_interface_number = 0;
    uint8_t current_interface_class = 0;
    uint8_t current_interface_subclass = 0;
    uint8_t current_interface_protocol = 0;

    if (!buffer || total_length < 9) {
        return;
    }

    interfaces = buffer[4];
    serial_print("xhci: configuration total=0x");
    serial_print_hex16(total_length);
    serial_print(" interfaces=0x");
    serial_print_hex8(interfaces);
    serial_print(" config=0x");
    serial_print_hex8(buffer[5]);
    serial_print("\n");

    while (off + 2 <= total_length && off < 256) {
        uint8_t len = buffer[off];
        uint8_t type = buffer[off + 1];

        if (len == 0 || off + len > total_length) {
            break;
        }

        if (type == 4 && len >= 9) {
            uint8_t interface_number = buffer[off + 2];
            uint8_t endpoint_count = buffer[off + 4];
            uint8_t interface_class = buffer[off + 5];
            uint8_t interface_subclass = buffer[off + 6];
            uint8_t interface_protocol = buffer[off + 7];

            current_interface_number = interface_number;
            current_interface_class = interface_class;
            current_interface_subclass = interface_subclass;
            current_interface_protocol = interface_protocol;

            serial_print("xhci: interface ");
            serial_print_hex8(interface_number);
            serial_print(" alt=0x");
            serial_print_hex8(buffer[off + 3]);
            serial_print(" eps=0x");
            serial_print_hex8(endpoint_count);
            serial_print(" class=0x");
            serial_print_hex8(interface_class);
            serial_print(" subclass=0x");
            serial_print_hex8(interface_subclass);
            serial_print(" protocol=0x");
            serial_print_hex8(interface_protocol);
            serial_print("\n");
            bluetooth_usb_interface_seen(interface_number, interface_class,
                                         interface_subclass, interface_protocol,
                                         endpoint_count);
        } else if (type == 5 && len >= 7) {
            uint8_t endpoint_addr = buffer[off + 2];
            uint8_t attrs = buffer[off + 3];
            uint16_t max_packet = read_le16(&buffer[off + 4]) & 0x07FFU;
            uint8_t interval = buffer[off + 6];

            serial_print("xhci: endpoint addr=0x");
            serial_print_hex8(endpoint_addr);
            serial_print(" attrs=0x");
            serial_print_hex8(attrs);
            serial_print(" maxpkt=0x");
            serial_print_hex16(max_packet);
            serial_print(" interval=0x");
            serial_print_hex8(interval);
            serial_print("\n");

            if ((endpoint_addr & 0x80U) && ((attrs & 0x03U) == 0x03U) && g_interrupt_in_endpoint == 0) {
                if (current_interface_class == 0x03U &&
                    current_interface_subclass == 0x01U &&
                    current_interface_protocol == 0x01U) {
                    g_interrupt_in_endpoint = endpoint_addr;
                    g_interrupt_in_max_packet = max_packet;
                    g_interrupt_in_interval = interval;
                    g_interrupt_in_owner = XHCI_INTERRUPT_OWNER_KEYBOARD;
                } else if (current_interface_class == 0x03U &&
                           current_interface_subclass == 0x01U &&
                           current_interface_protocol == 0x02U) {
                    g_interrupt_in_endpoint = endpoint_addr;
                    g_interrupt_in_max_packet = max_packet;
                    g_interrupt_in_interval = interval;
                    g_interrupt_in_owner = XHCI_INTERRUPT_OWNER_MOUSE;
                } else if (current_interface_class == 0x03U) {
                    g_interrupt_in_endpoint = endpoint_addr;
                    g_interrupt_in_max_packet = max_packet;
                    g_interrupt_in_interval = interval;
                    g_interrupt_in_owner = XHCI_INTERRUPT_OWNER_MOUSE;
                } else if (current_interface_class == 0xe0U &&
                           current_interface_subclass == 0x01U &&
                           current_interface_protocol == 0x01U) {
                    g_interrupt_in_endpoint = endpoint_addr;
                    g_interrupt_in_max_packet = max_packet;
                    g_interrupt_in_interval = interval;
                    g_interrupt_in_owner = XHCI_INTERRUPT_OWNER_BLUETOOTH;
                    g_bluetooth_interface_number = current_interface_number;
                    bluetooth_usb_event_endpoint_seen(endpoint_addr, max_packet, interval);
                }
            }
        }

        off = (uint16_t)(off + len);
    }
}

static int xhci_get_config_descriptor(uint32_t slot_id) {
    uint16_t total_length;

    g_interrupt_in_endpoint = 0;
    g_interrupt_in_max_packet = 0;
    g_interrupt_in_interval = 0;
    g_interrupt_in_owner = XHCI_INTERRUPT_OWNER_NONE;
    g_bluetooth_interface_number = 0;

    if (xhci_control_get_descriptor(slot_id, 2, 0, g_config_descriptor, 9) < 0) {
        return -1;
    }
    if (g_config_descriptor[0] < 9 || g_config_descriptor[1] != 2) {
        serial_print("xhci: invalid configuration descriptor header\n");
        return -1;
    }

    total_length = read_le16(&g_config_descriptor[2]);
    if (total_length > 256) {
        total_length = 256;
    }
    if (total_length < 9) {
        return -1;
    }

    if (xhci_control_get_descriptor(slot_id, 2, 0, g_config_descriptor, total_length) < 0) {
        return -1;
    }
    xhci_parse_config_descriptor(g_config_descriptor, total_length);
    return 0;
}

static uint32_t xhci_endpoint_dci(uint8_t endpoint_address) {
    uint32_t endpoint_number = endpoint_address & 0x0FU;
    if (endpoint_number == 0) {
        return 1;
    }
    return (endpoint_number * 2U) + ((endpoint_address & 0x80U) ? 1U : 0U);
}

static struct xhci_trb* xhci_next_interrupt_in_trb(void) {
    struct xhci_trb* trb = &g_interrupt_in_ring[g_interrupt_in_enqueue];
    g_interrupt_in_enqueue++;
    if (g_interrupt_in_enqueue >= (XHCI_TRB_COUNT - 1)) {
        g_interrupt_in_ring[XHCI_TRB_COUNT - 1].control =
            g_interrupt_in_cycle |
            XHCI_TRB_LINK_TOGGLE_CYCLE |
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT);
        g_interrupt_in_enqueue = 0;
        g_interrupt_in_cycle ^= 1;
    }
    return trb;
}

static int xhci_submit_bluetooth_event_transfer(void) {
    struct xhci_trb* trb;
    uint64_t event_phys;
    uint32_t len;
    uint8_t cycle;

    if (!g_interrupt_in_ring || !g_bluetooth_event_buffer ||
        g_bluetooth_slot_id == 0 || g_interrupt_in_dci == 0 ||
        g_interrupt_in_owner != XHCI_INTERRUPT_OWNER_BLUETOOTH) {
        return -1;
    }
    if (g_bluetooth_pending_trb != 0) {
        return 0;
    }

    len = g_bluetooth_event_buffer_len;
    if (len == 0) {
        len = 16;
    }

    local_memset(g_bluetooth_event_buffer, 0, len);
    event_phys = (uint64_t)g_bluetooth_event_buffer;
    cycle = g_interrupt_in_cycle;
    trb = xhci_next_interrupt_in_trb();
    g_bluetooth_pending_trb = (uint64_t)trb;

    trb->parameter_lo = (uint32_t)(event_phys & 0xFFFFFFFFU);
    trb->parameter_hi = (uint32_t)(event_phys >> 32);
    trb->status = len;
    trb->control =
        cycle |
        XHCI_TRB_IOC |
        (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT);

    xhci_ring_doorbell(g_bluetooth_slot_id, g_interrupt_in_dci);
    return 0;
}

static int xhci_wait_bluetooth_command_complete(uint16_t expected_opcode, uint32_t* out_len) {
    for (uint32_t tries = 0; tries < 1000000; tries++) {
        struct xhci_trb* event = &g_event_ring[g_event_dequeue];
        uint32_t type;
        uint64_t event_trb_phys;
        uint32_t completion;
        uint32_t residue;
        uint32_t endpoint_id;
        uint32_t actual_len;
        uint16_t opcode;

        if ((event->control & XHCI_TRB_CYCLE) != g_event_cycle) {
            continue;
        }

        type = (event->control >> XHCI_TRB_TYPE_SHIFT) & 0x3FU;
        if (type != XHCI_TRB_TYPE_TRANSFER_EVENT) {
            xhci_advance_event();
            continue;
        }

        event_trb_phys = ((uint64_t)event->parameter_hi << 32) | event->parameter_lo;
        completion = (event->status >> 24) & 0xFFU;
        residue = event->status & 0x00FFFFFFU;
        endpoint_id = (event->control >> 16) & 0x1FU;
        xhci_advance_event();

        if (event_trb_phys != g_bluetooth_pending_trb || endpoint_id != g_interrupt_in_dci) {
            continue;
        }

        g_bluetooth_pending_trb = 0;
        if (completion != XHCI_COMPLETION_SUCCESS) {
            serial_print("xhci: bluetooth event transfer failed cc=0x");
            serial_print_hex8((uint8_t)completion);
            serial_print("\n");
            return -1;
        }

        actual_len = g_bluetooth_event_buffer_len;
        if (residue < actual_len) {
            actual_len -= residue;
        }
        if (out_len) {
            *out_len = actual_len;
        }
        if (actual_len < 6 ||
            g_bluetooth_event_buffer[0] != 0x0eU ||
            g_bluetooth_event_buffer[1] < 4U) {
            serial_print("xhci: bluetooth unexpected HCI event\n");
            return -1;
        }

        opcode = (uint16_t)g_bluetooth_event_buffer[3] |
                 ((uint16_t)g_bluetooth_event_buffer[4] << 8);
        if (opcode != expected_opcode) {
            serial_print("xhci: bluetooth unexpected opcode=0x");
            serial_print_hex16(opcode);
            serial_print("\n");
            return -1;
        }
        return 0;
    }

    serial_print("xhci: bluetooth command complete timeout\n");
    return -1;
}

static int xhci_wait_bluetooth_event(uint32_t* out_len) {
    for (uint32_t tries = 0; tries < 1000000; tries++) {
        struct xhci_trb* event = &g_event_ring[g_event_dequeue];
        uint32_t type;
        uint64_t event_trb_phys;
        uint32_t completion;
        uint32_t residue;
        uint32_t endpoint_id;
        uint32_t actual_len;

        if ((event->control & XHCI_TRB_CYCLE) != g_event_cycle) {
            continue;
        }

        type = (event->control >> XHCI_TRB_TYPE_SHIFT) & 0x3FU;
        if (type != XHCI_TRB_TYPE_TRANSFER_EVENT) {
            xhci_advance_event();
            continue;
        }

        event_trb_phys = ((uint64_t)event->parameter_hi << 32) | event->parameter_lo;
        completion = (event->status >> 24) & 0xFFU;
        residue = event->status & 0x00FFFFFFU;
        endpoint_id = (event->control >> 16) & 0x1FU;
        xhci_advance_event();

        if (event_trb_phys != g_bluetooth_pending_trb || endpoint_id != g_interrupt_in_dci) {
            continue;
        }

        g_bluetooth_pending_trb = 0;
        if (completion != XHCI_COMPLETION_SUCCESS) {
            serial_print("xhci: bluetooth event transfer failed cc=0x");
            serial_print_hex8((uint8_t)completion);
            serial_print("\n");
            return -1;
        }

        actual_len = g_bluetooth_event_buffer_len;
        if (residue < actual_len) {
            actual_len -= residue;
        }
        if (out_len) {
            *out_len = actual_len;
        }
        return 0;
    }

    return -1;
}

static int xhci_wait_bluetooth_command_status(uint16_t expected_opcode) {
    uint32_t event_len = 0;
    uint16_t opcode;
    uint8_t status;

    if (xhci_wait_bluetooth_event(&event_len) < 0) {
        serial_print("xhci: bluetooth command status timeout\n");
        return -1;
    }
    if (event_len < 6 || g_bluetooth_event_buffer[0] != 0x0fU || g_bluetooth_event_buffer[1] < 4U) {
        serial_print("xhci: bluetooth expected command status event\n");
        return -1;
    }

    status = g_bluetooth_event_buffer[2];
    opcode = (uint16_t)g_bluetooth_event_buffer[4] |
             ((uint16_t)g_bluetooth_event_buffer[5] << 8);
    if (opcode != expected_opcode) {
        serial_print("xhci: bluetooth unexpected status opcode=0x");
        serial_print_hex16(opcode);
        serial_print("\n");
        return -1;
    }
    if (status != 0) {
        serial_print("xhci: bluetooth command status failed=0x");
        serial_print_hex8(status);
        serial_print("\n");
        return -1;
    }
    return 0;
}

static int xhci_bluetooth_send_hci_command(uint16_t opcode, const uint8_t* command,
                                           uint16_t command_len, uint32_t* event_len) {
    if (!g_bluetooth_command_buffer || !command || command_len == 0 || command_len > 256) {
        return -1;
    }

    local_memset(g_bluetooth_command_buffer, 0, 256);
    local_memcpy(g_bluetooth_command_buffer, command, command_len);
    if (xhci_submit_bluetooth_event_transfer() < 0) {
        return -1;
    }
    if (xhci_control_class_out(g_bluetooth_slot_id, 0, 0,
                               g_bluetooth_interface_number,
                               g_bluetooth_command_buffer, command_len) < 0) {
        g_bluetooth_pending_trb = 0;
        return -1;
    }
    return xhci_wait_bluetooth_command_complete(opcode, event_len);
}

static int xhci_bluetooth_send_hci_command_status(uint16_t opcode, const uint8_t* command,
                                                  uint16_t command_len) {
    if (!g_bluetooth_command_buffer || !command || command_len == 0 || command_len > 256) {
        return -1;
    }

    local_memset(g_bluetooth_command_buffer, 0, 256);
    local_memcpy(g_bluetooth_command_buffer, command, command_len);
    if (xhci_submit_bluetooth_event_transfer() < 0) {
        return -1;
    }
    if (xhci_control_class_out(g_bluetooth_slot_id, 0, 0,
                               g_bluetooth_interface_number,
                               g_bluetooth_command_buffer, command_len) < 0) {
        g_bluetooth_pending_trb = 0;
        return -1;
    }
    return xhci_wait_bluetooth_command_status(opcode);
}

static int xhci_bluetooth_hci_init(void) {
    static const uint8_t hci_reset[] = {0x03, 0x0c, 0x00};
    static const uint8_t hci_write_event_mask[] = {
        0x01, 0x0c, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f
    };
    static const uint8_t hci_write_simple_pairing_mode[] = {0x56, 0x0c, 0x01, 0x01};
    static const uint8_t hci_read_local_version[] = {0x01, 0x10, 0x00};
    uint32_t event_len = 0;
    uint8_t status;
    uint8_t hci_version;
    uint16_t hci_revision;
    uint8_t lmp_version;
    uint16_t manufacturer;
    uint16_t lmp_subversion;

    if (g_interrupt_in_owner != XHCI_INTERRUPT_OWNER_BLUETOOTH || g_bluetooth_slot_id == 0) {
        return -1;
    }

    serial_print("xhci: bluetooth HCI Reset\n");
    if (xhci_bluetooth_send_hci_command(0x0c03U, hci_reset, sizeof(hci_reset), &event_len) < 0) {
        bluetooth_hci_transport_error("warn: HCI reset transport failed");
        return -1;
    }
    status = g_bluetooth_event_buffer[5];
    bluetooth_hci_reset_complete(status);
    if (status != 0) {
        return -1;
    }

    serial_print("xhci: bluetooth Write Event Mask\n");
    if (xhci_bluetooth_send_hci_command(0x0c01U, hci_write_event_mask,
                                        sizeof(hci_write_event_mask),
                                        &event_len) == 0 &&
        event_len >= 6 && g_bluetooth_event_buffer[5] == 0) {
        serial_print("xhci: bluetooth pairing events enabled\n");
    } else {
        serial_print("xhci: bluetooth event mask setup skipped\n");
    }

    serial_print("xhci: bluetooth Write Simple Pairing Mode\n");
    if (xhci_bluetooth_send_hci_command(0x0c56U, hci_write_simple_pairing_mode,
                                        sizeof(hci_write_simple_pairing_mode),
                                        &event_len) == 0 &&
        event_len >= 6 && g_bluetooth_event_buffer[5] == 0) {
        serial_print("xhci: bluetooth simple pairing enabled\n");
    } else {
        serial_print("xhci: bluetooth simple pairing unavailable\n");
    }

    serial_print("xhci: bluetooth Read Local Version Information\n");
    if (xhci_bluetooth_send_hci_command(0x1001U, hci_read_local_version,
                                        sizeof(hci_read_local_version),
                                        &event_len) < 0) {
        bluetooth_hci_transport_error("warn: HCI version transport failed");
        return -1;
    }
    if (event_len < 14) {
        bluetooth_hci_transport_error("warn: short HCI version event");
        return -1;
    }

    status = g_bluetooth_event_buffer[5];
    hci_version = g_bluetooth_event_buffer[6];
    hci_revision = read_le16(&g_bluetooth_event_buffer[7]);
    lmp_version = g_bluetooth_event_buffer[9];
    manufacturer = read_le16(&g_bluetooth_event_buffer[10]);
    lmp_subversion = read_le16(&g_bluetooth_event_buffer[12]);
    bluetooth_hci_version_complete(status, hci_version, hci_revision,
                                   lmp_version, manufacturer, lmp_subversion);
    return status == 0 ? 0 : -1;
}

static void xhci_bluetooth_parse_inquiry_result(uint8_t event_code, uint32_t event_len) {
    uint8_t count;
    uint32_t off;
    uint32_t stride;

    if (event_len < 3) {
        return;
    }

    count = g_bluetooth_event_buffer[2];
    off = 3;
    if (event_code == 0x02U) {
        stride = 14;
    } else if (event_code == 0x22U) {
        stride = 15;
    } else {
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t* record;
        uint8_t has_rssi = 0;
        int8_t rssi = 0;

        if (off + stride > event_len) {
            return;
        }

        record = &g_bluetooth_event_buffer[off];
        if (event_code == 0x22U) {
            has_rssi = 1;
            rssi = (int8_t)record[14];
        }
        bluetooth_discovery_add(record, record[6], has_rssi, rssi);
        off += stride;
    }
}

static void xhci_bluetooth_parse_extended_inquiry_result(uint32_t event_len) {
    const uint8_t* record;

    if (event_len < 17 || g_bluetooth_event_buffer[2] == 0) {
        return;
    }

    record = &g_bluetooth_event_buffer[3];
    bluetooth_discovery_add(record, record[6], 1, (int8_t)record[13]);
}

static int xhci_bluetooth_address_match(const uint8_t* event_address,
                                        const uint8_t address[6]) {
    if (!event_address || !address) {
        return 0;
    }
    return event_address[0] == address[0] &&
           event_address[1] == address[1] &&
           event_address[2] == address[2] &&
           event_address[3] == address[3] &&
           event_address[4] == address[4] &&
           event_address[5] == address[5];
}

static int xhci_bluetooth_command_complete_ok(uint16_t opcode,
                                              const uint8_t* command,
                                              uint16_t command_len) {
    uint32_t event_len = 0;

    if (xhci_bluetooth_send_hci_command(opcode, command, command_len, &event_len) < 0) {
        return -1;
    }
    if (event_len < 6 || g_bluetooth_event_buffer[5] != 0) {
        return -1;
    }
    return 0;
}

static int xhci_bluetooth_reply_link_key(const uint8_t address[6]) {
    uint8_t command[25];
    uint8_t link_key[BLUETOOTH_LINK_KEY_SIZE];

    if (bluetooth_find_link_key(address, link_key, 0) == 0) {
        command[0] = 0x0b;
        command[1] = 0x04;
        command[2] = 0x16;
        for (uint32_t i = 0; i < 6; i++) {
            command[3 + i] = address[i];
        }
        for (uint32_t i = 0; i < BLUETOOTH_LINK_KEY_SIZE; i++) {
            command[9 + i] = link_key[i];
        }
        bluetooth_auth_event(address, "pair: stored link key accepted");
        return xhci_bluetooth_command_complete_ok(0x040bU, command, sizeof(command));
    }

    command[0] = 0x0c;
    command[1] = 0x04;
    command[2] = 0x06;
    for (uint32_t i = 0; i < 6; i++) {
        command[3 + i] = address[i];
    }
    bluetooth_auth_event(address, "pair: no stored link key, starting pairing");
    return xhci_bluetooth_command_complete_ok(0x040cU, command, 9);
}

static int xhci_bluetooth_reply_pin_code(const uint8_t address[6]) {
    uint8_t command[26];

    local_memset(command, 0, sizeof(command));
    command[0] = 0x0d;
    command[1] = 0x04;
    command[2] = 0x17;
    for (uint32_t i = 0; i < 6; i++) {
        command[3 + i] = address[i];
    }
    command[9] = 4;
    command[10] = '0';
    command[11] = '0';
    command[12] = '0';
    command[13] = '0';
    bluetooth_auth_event(address, "pair: PIN fallback replied with 0000");
    return xhci_bluetooth_command_complete_ok(0x040dU, command, sizeof(command));
}

static int xhci_bluetooth_reply_io_capability(const uint8_t address[6]) {
    uint8_t command[12];

    command[0] = 0x2b;
    command[1] = 0x04;
    command[2] = 0x09;
    for (uint32_t i = 0; i < 6; i++) {
        command[3 + i] = address[i];
    }
    command[9] = 0x03;
    command[10] = 0x00;
    command[11] = 0x04;
    bluetooth_auth_event(address, "pair: SSP IO capability replied");
    return xhci_bluetooth_command_complete_ok(0x042bU, command, sizeof(command));
}

static int xhci_bluetooth_reply_user_confirmation(const uint8_t address[6]) {
    uint8_t command[9];

    command[0] = 0x2c;
    command[1] = 0x04;
    command[2] = 0x06;
    for (uint32_t i = 0; i < 6; i++) {
        command[3 + i] = address[i];
    }
    bluetooth_auth_event(address, "pair: SSP user confirmation accepted");
    return xhci_bluetooth_command_complete_ok(0x042cU, command, sizeof(command));
}

static int xhci_bluetooth_handle_pairing_event(uint8_t event_code,
                                               uint32_t event_len,
                                               const uint8_t target_address[6]) {
    const uint8_t* address;

    if (event_code == 0x18U) {
        if (event_len < 25) {
            return 1;
        }
        address = &g_bluetooth_event_buffer[2];
        bluetooth_link_key_store(address, &g_bluetooth_event_buffer[8],
                                 g_bluetooth_event_buffer[24]);
        return 1;
    }

    if (event_code == 0x36U) {
        if (event_len < 9) {
            return 1;
        }
        address = &g_bluetooth_event_buffer[3];
        if (!target_address || xhci_bluetooth_address_match(address, target_address)) {
            if (g_bluetooth_event_buffer[2] == 0) {
                bluetooth_auth_event(address, "pair: simple pairing complete");
            } else {
                bluetooth_auth_event(address, "warn: simple pairing failed");
            }
        }
        return 1;
    }

    if (event_len < 8) {
        return 0;
    }
    address = &g_bluetooth_event_buffer[2];
    if (target_address && !xhci_bluetooth_address_match(address, target_address)) {
        return 1;
    }

    if (event_code == 0x17U) {
        return xhci_bluetooth_reply_link_key(address) == 0 ? 1 : -1;
    }
    if (event_code == 0x16U) {
        return xhci_bluetooth_reply_pin_code(address) == 0 ? 1 : -1;
    }
    if (event_code == 0x31U) {
        return xhci_bluetooth_reply_io_capability(address) == 0 ? 1 : -1;
    }
    if (event_code == 0x33U) {
        return xhci_bluetooth_reply_user_confirmation(address) == 0 ? 1 : -1;
    }

    return 0;
}

static int xhci_bluetooth_wait_remote_name_complete(const uint8_t address[6]) {
    for (uint32_t events = 0; events < 32; events++) {
        uint32_t event_len = 0;
        uint8_t event_code;
        uint8_t status;
        const char* name;
        uint32_t name_len;

        if (xhci_submit_bluetooth_event_transfer() < 0 ||
            xhci_wait_bluetooth_event(&event_len) < 0) {
            return -1;
        }
        if (event_len < 2) {
            continue;
        }

        event_code = g_bluetooth_event_buffer[0];
        if (event_code == 0x02U || event_code == 0x22U) {
            xhci_bluetooth_parse_inquiry_result(event_code, event_len);
            continue;
        }
        if (event_code == 0x2fU) {
            xhci_bluetooth_parse_extended_inquiry_result(event_len);
            continue;
        }
        if (event_code != 0x07U || event_len < 9) {
            continue;
        }

        status = g_bluetooth_event_buffer[2];
        if (!address || !((g_bluetooth_event_buffer[3] == address[0]) &&
                          (g_bluetooth_event_buffer[4] == address[1]) &&
                          (g_bluetooth_event_buffer[5] == address[2]) &&
                          (g_bluetooth_event_buffer[6] == address[3]) &&
                          (g_bluetooth_event_buffer[7] == address[4]) &&
                          (g_bluetooth_event_buffer[8] == address[5]))) {
            continue;
        }

        name = (const char*)&g_bluetooth_event_buffer[9];
        name_len = event_len > 9 ? event_len - 9 : 0;
        for (uint32_t i = 0; i < name_len; i++) {
            if (name[i] == '\0') {
                name_len = i;
                break;
            }
        }
        bluetooth_discovery_set_name(address, status, name, name_len);
        return status == 0 ? 0 : -1;
    }

    return -1;
}

static void xhci_bluetooth_resolve_discovered_names(void) {
    for (uint32_t i = 0; i < BLUETOOTH_MAX_DISCOVERED; i++) {
        struct bluetooth_discovered_device dev;
        uint8_t command[13];

        if (bluetooth_get_discovered(i, &dev) != 0) {
            break;
        }
        if (!dev.valid || dev.has_name) {
            continue;
        }

        command[0] = 0x19;
        command[1] = 0x04;
        command[2] = 0x0a;
        for (uint32_t j = 0; j < 6; j++) {
            command[3 + j] = dev.address[j];
        }
        command[9] = dev.page_scan_repetition_mode;
        command[10] = 0;
        command[11] = 0;
        command[12] = 0;

        serial_print("xhci: bluetooth Remote Name Request\n");
        if (xhci_bluetooth_send_hci_command_status(0x0419U, command, sizeof(command)) < 0) {
            bluetooth_discovery_set_name(dev.address, 1, 0, 0);
            continue;
        }
        if (xhci_bluetooth_wait_remote_name_complete(dev.address) != 0) {
            bluetooth_discovery_set_name(dev.address, 1, 0, 0);
        }
    }
}

int xhci_bluetooth_scan(void) {
    static const uint8_t hci_write_inquiry_mode[] = {0x45, 0x0c, 0x01, 0x01};
    static const uint8_t hci_inquiry[] = {0x01, 0x04, 0x05, 0x33, 0x8b, 0x9e, 0x04, 0x08};
    uint32_t event_len = 0;
    uint8_t status;

    if (g_interrupt_in_owner != XHCI_INTERRUPT_OWNER_BLUETOOTH ||
        g_bluetooth_slot_id == 0 || !g_bluetooth_event_buffer) {
        bluetooth_hci_transport_error("scan: Bluetooth HCI transport not ready");
        return -1;
    }

    serial_print("xhci: bluetooth Write Inquiry Mode RSSI\n");
    if (xhci_bluetooth_send_hci_command(0x0c45U, hci_write_inquiry_mode,
                                        sizeof(hci_write_inquiry_mode), &event_len) < 0) {
        bluetooth_hci_transport_error("scan: write inquiry mode failed");
        return -1;
    }
    if (event_len < 6 || g_bluetooth_event_buffer[5] != 0) {
        bluetooth_hci_transport_error("scan: inquiry mode rejected");
        return -1;
    }

    serial_print("xhci: bluetooth Inquiry\n");
    if (xhci_bluetooth_send_hci_command_status(0x0401U, hci_inquiry, sizeof(hci_inquiry)) < 0) {
        bluetooth_hci_transport_error("scan: inquiry command failed");
        return -1;
    }

    for (uint32_t events = 0; events < 64; events++) {
        uint8_t event_code;

        if (xhci_submit_bluetooth_event_transfer() < 0 ||
            xhci_wait_bluetooth_event(&event_len) < 0) {
            bluetooth_hci_transport_error("scan: inquiry event timeout");
            return -1;
        }
        if (event_len < 2) {
            continue;
        }

        event_code = g_bluetooth_event_buffer[0];
        if (event_code == 0x01U) {
            status = event_len >= 3 ? g_bluetooth_event_buffer[2] : 1U;
            if (status == 0) {
                xhci_bluetooth_resolve_discovered_names();
            }
            bluetooth_scan_complete(status);
            return status == 0 ? 0 : -1;
        }
        if (event_code == 0x02U || event_code == 0x22U) {
            xhci_bluetooth_parse_inquiry_result(event_code, event_len);
        } else if (event_code == 0x2fU) {
            xhci_bluetooth_parse_extended_inquiry_result(event_len);
        }
    }

    bluetooth_hci_transport_error("scan: inquiry did not complete");
    return -1;
}

static int xhci_bluetooth_wait_connection_complete(const uint8_t address[6]) {
    for (uint32_t events = 0; events < 64; events++) {
        uint32_t event_len = 0;
        uint8_t event_code;
        uint8_t status;
        uint16_t handle;
        uint8_t link_type;
        uint8_t encryption_enabled;

        if (xhci_submit_bluetooth_event_transfer() < 0 ||
            xhci_wait_bluetooth_event(&event_len) < 0) {
            return -1;
        }
        if (event_len < 2) {
            continue;
        }

        event_code = g_bluetooth_event_buffer[0];
        if (event_code == 0x02U || event_code == 0x22U) {
            xhci_bluetooth_parse_inquiry_result(event_code, event_len);
            continue;
        }
        if (event_code == 0x2fU) {
            xhci_bluetooth_parse_extended_inquiry_result(event_len);
            continue;
        }
        {
            int auth_rc = xhci_bluetooth_handle_pairing_event(event_code, event_len, address);
            if (auth_rc > 0) {
                continue;
            }
            if (auth_rc < 0) {
                return -1;
            }
        }
        if (event_code != 0x03U || event_len < 13) {
            continue;
        }

        status = g_bluetooth_event_buffer[2];
        if (!address || !((g_bluetooth_event_buffer[5] == address[0]) &&
                          (g_bluetooth_event_buffer[6] == address[1]) &&
                          (g_bluetooth_event_buffer[7] == address[2]) &&
                          (g_bluetooth_event_buffer[8] == address[3]) &&
                          (g_bluetooth_event_buffer[9] == address[4]) &&
                          (g_bluetooth_event_buffer[10] == address[5]))) {
            continue;
        }

        handle = (uint16_t)g_bluetooth_event_buffer[3] |
                 ((uint16_t)g_bluetooth_event_buffer[4] << 8);
        link_type = g_bluetooth_event_buffer[11];
        encryption_enabled = g_bluetooth_event_buffer[12];
        bluetooth_pair_complete(address, status, handle, link_type, encryption_enabled);
        return status == 0 ? 0 : 1;
    }

    return -1;
}

int xhci_bluetooth_pair(size_t index) {
    struct bluetooth_discovered_device dev;
    uint8_t command[16];

    if (g_interrupt_in_owner != XHCI_INTERRUPT_OWNER_BLUETOOTH ||
        g_bluetooth_slot_id == 0 || !g_bluetooth_event_buffer) {
        bluetooth_hci_transport_error("pair: Bluetooth HCI transport not ready");
        return -1;
    }
    if (bluetooth_get_discovered(index, &dev) != 0 || !dev.valid) {
        bluetooth_hci_transport_error("pair: device index missing");
        return -1;
    }

    command[0] = 0x05;
    command[1] = 0x04;
    command[2] = 0x0d;
    for (uint32_t i = 0; i < 6; i++) {
        command[3 + i] = dev.address[i];
    }
    command[9] = 0x18;
    command[10] = 0xcc;
    command[11] = dev.page_scan_repetition_mode;
    command[12] = 0;
    command[13] = 0;
    command[14] = 0;
    command[15] = 1;

    serial_print("xhci: bluetooth Create Connection\n");
    if (xhci_bluetooth_send_hci_command_status(0x0405U, command, sizeof(command)) < 0) {
        bluetooth_pair_complete(dev.address, 1, 0, 0, 0);
        return -1;
    }
    int wait_status = xhci_bluetooth_wait_connection_complete(dev.address);
    if (wait_status < 0) {
        bluetooth_pair_complete(dev.address, 1, 0, 0, 0);
        return -1;
    }
    return wait_status == 0 ? 0 : -1;
}

static int xhci_submit_keyboard_transfer(void) {
    struct xhci_trb* trb;
    uint64_t report_phys;
    uint32_t len;
    uint8_t cycle;

    if (!g_interrupt_in_ring || !g_keyboard_report || g_keyboard_slot_id == 0 || g_interrupt_in_dci == 0) {
        return -1;
    }
    if (g_keyboard_pending_trb != 0) {
        return 0;
    }

    len = g_interrupt_in_max_packet;
    if (len == 0 || len > 8) {
        len = 8;
    }

    local_memset(g_keyboard_report, 0, 8);
    report_phys = (uint64_t)g_keyboard_report;
    cycle = g_interrupt_in_cycle;
    trb = xhci_next_interrupt_in_trb();
    g_keyboard_pending_trb = (uint64_t)trb;

    trb->parameter_lo = (uint32_t)(report_phys & 0xFFFFFFFFU);
    trb->parameter_hi = (uint32_t)(report_phys >> 32);
    trb->status = len;
    trb->control =
        cycle |
        XHCI_TRB_IOC |
        (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT);

    xhci_ring_doorbell(g_keyboard_slot_id, g_interrupt_in_dci);
    return 0;
}

static int xhci_submit_mouse_transfer(void) {
    struct xhci_trb* trb;
    uint64_t report_phys;
    uint32_t len;
    uint8_t cycle;

    if (!g_interrupt_in_ring || !g_mouse_report || g_mouse_slot_id == 0 || g_interrupt_in_dci == 0) {
        return -1;
    }
    if (g_mouse_pending_trb != 0) {
        return 0;
    }

    len = g_interrupt_in_max_packet;
    if (len == 0 || len > 8) {
        len = 8;
    }

    local_memset(g_mouse_report, 0, 8);
    report_phys = (uint64_t)g_mouse_report;
    cycle = g_interrupt_in_cycle;
    trb = xhci_next_interrupt_in_trb();
    g_mouse_pending_trb = (uint64_t)trb;

    trb->parameter_lo = (uint32_t)(report_phys & 0xFFFFFFFFU);
    trb->parameter_hi = (uint32_t)(report_phys >> 32);
    trb->status = len;
    trb->control =
        cycle |
        XHCI_TRB_IOC |
        (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT);

    xhci_ring_doorbell(g_mouse_slot_id, g_interrupt_in_dci);
    return 0;
}

static int xhci_configure_interrupt_in_endpoint(uint32_t slot_id) {
    void* input_ctx = alloc_zero_page();
    uint32_t* input_control;
    uint32_t* slot_ctx;
    uint32_t* ep_ctx;
    struct xhci_trb* cmd;
    struct xhci_command_result result;
    uint64_t input_phys;
    uint64_t ring_phys;
    uint64_t cmd_phys;
    uint32_t dci;
    uint32_t interval;
    uint32_t max_packet;

    if (slot_id == 0 || g_interrupt_in_endpoint == 0 || g_interrupt_in_max_packet == 0 ||
        g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_NONE) {
        serial_print("xhci: no interrupt IN endpoint to configure\n");
        return -1;
    }

    g_interrupt_in_ring = (struct xhci_trb*)alloc_zero_page();
    if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_KEYBOARD) {
        g_keyboard_report = (uint8_t*)alloc_zero_page();
    } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_MOUSE) {
        g_mouse_report = (uint8_t*)alloc_zero_page();
    } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_BLUETOOTH) {
        g_bluetooth_command_buffer = (uint8_t*)alloc_zero_page();
        g_bluetooth_event_buffer = (uint8_t*)alloc_zero_page();
    }
    if (!input_ctx || !g_interrupt_in_ring ||
        (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_KEYBOARD && !g_keyboard_report) ||
        (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_MOUSE && !g_mouse_report) ||
        (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_BLUETOOTH &&
         (!g_bluetooth_command_buffer || !g_bluetooth_event_buffer))) {
        serial_print("xhci: interrupt endpoint allocation failed\n");
        return -1;
    }

    input_phys = (uint64_t)input_ctx;
    ring_phys = (uint64_t)g_interrupt_in_ring;
    dci = xhci_endpoint_dci(g_interrupt_in_endpoint);
    g_interrupt_in_dci = dci;
    max_packet = g_interrupt_in_max_packet;
    interval = g_interrupt_in_interval;
    if (interval > 0) {
        interval--;
    }
    if (interval > 15) {
        interval = 15;
    }

    g_interrupt_in_cycle = 1;
    g_interrupt_in_enqueue = 0;
    g_keyboard_pending_trb = 0;
    g_mouse_pending_trb = 0;
    g_bluetooth_pending_trb = 0;
    g_keyboard_slot_id = 0;
    g_mouse_slot_id = 0;
    g_bluetooth_slot_id = 0;
    local_memset(g_keyboard_last_keys, 0, sizeof(g_keyboard_last_keys));
    if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_KEYBOARD) {
        g_keyboard_slot_id = slot_id;
    } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_MOUSE) {
        g_mouse_slot_id = slot_id;
    } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_BLUETOOTH) {
        g_bluetooth_slot_id = slot_id;
        g_bluetooth_event_buffer_len = 260;
    }
    g_interrupt_in_ring[XHCI_TRB_COUNT - 1].parameter_lo = (uint32_t)(ring_phys & 0xFFFFFFFFU);
    g_interrupt_in_ring[XHCI_TRB_COUNT - 1].parameter_hi = (uint32_t)(ring_phys >> 32);
    g_interrupt_in_ring[XHCI_TRB_COUNT - 1].status = 0;
    g_interrupt_in_ring[XHCI_TRB_COUNT - 1].control =
        XHCI_TRB_CYCLE |
        XHCI_TRB_LINK_TOGGLE_CYCLE |
        (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT);

    input_control = xhci_context_at(input_ctx, 0);
    slot_ctx = xhci_context_at(input_ctx, 1);
    ep_ctx = xhci_context_at(input_ctx, dci + 1U);

    input_control[1] = (1U << 0) | (1U << dci);
    slot_ctx[0] =
        ((uint32_t)g_first_connected_speed << XHCI_SLOT_CTX_SPEED_SHIFT) |
        (dci << XHCI_SLOT_CTX_ENTRIES_SHIFT);
    slot_ctx[1] = ((uint32_t)g_first_connected_port << XHCI_SLOT_CTX_ROOT_PORT_SHIFT);
    ep_ctx[0] = (interval << XHCI_EP_CTX_INTERVAL_SHIFT);
    ep_ctx[1] =
        (3U << XHCI_EP_CTX_CERR_SHIFT) |
        (XHCI_EP_TYPE_INTERRUPT_IN << XHCI_EP_CTX_TYPE_SHIFT) |
        (max_packet << XHCI_EP_CTX_MAX_PACKET_SHIFT);
    ep_ctx[2] = (uint32_t)((ring_phys | g_interrupt_in_cycle) & 0xFFFFFFFFU);
    ep_ctx[3] = (uint32_t)((ring_phys | g_interrupt_in_cycle) >> 32);
    ep_ctx[4] = max_packet | (max_packet << 16);

    cmd = xhci_next_command(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT);
    cmd_phys = (uint64_t)cmd;
    cmd->parameter_lo = (uint32_t)(input_phys & 0xFFFFFFFFU);
    cmd->parameter_hi = (uint32_t)(input_phys >> 32);
    cmd->control |= (slot_id << 24);

    serial_print("xhci: command Configure Endpoint slot=0x");
    serial_print_hex8((uint8_t)slot_id);
    serial_print(" ep=0x");
    serial_print_hex8(g_interrupt_in_endpoint);
    serial_print(" dci=0x");
    serial_print_hex8((uint8_t)dci);
    serial_print(" ring=0x");
    serial_print_hex32((uint32_t)ring_phys);
    serial_print("\n");
    xhci_ring_doorbell(0, 0);

    if (xhci_wait_command_completion(cmd_phys, &result) < 0) {
        serial_print("xhci: configure endpoint timeout\n");
        return -1;
    }
    if (result.completion_code != XHCI_COMPLETION_SUCCESS) {
        serial_print("xhci: configure endpoint failed cc=0x");
        serial_print_hex8((uint8_t)result.completion_code);
        serial_print("\n");
        return -1;
    }

    serial_print("xhci: interrupt IN endpoint configured\n");
    if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_KEYBOARD &&
        xhci_submit_keyboard_transfer() == 0) {
        serial_print("xhci: keyboard interrupt transfer armed\n");
    } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_MOUSE &&
               xhci_submit_mouse_transfer() == 0) {
        serial_print("xhci: mouse interrupt transfer armed\n");
    } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_BLUETOOTH) {
        serial_print("xhci: bluetooth event endpoint configured\n");
    }
    return 0;
}

static int hid_key_was_down(uint8_t key) {
    for (uint32_t i = 0; i < 6; i++) {
        if (g_keyboard_last_keys[i] == key) {
            return 1;
        }
    }
    return 0;
}

static char hid_key_to_tty(uint8_t key, uint8_t modifiers) {
    static const char normal[] = {
        0, 0, 0, 0,
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
        'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
        'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
        'y', 'z', '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '\n', 27, '\b', '\t',
        ' ', '-', '=', '[', ']', '\\', 0, ';',
        '\'', '`', ',', '.', '/'
    };
    static const char shifted[] = {
        0, 0, 0, 0,
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', '!', '@', '#', '$', '%', '^',
        '&', '*', '(', ')', '\n', 27, '\b', '\t',
        ' ', '_', '+', '{', '}', '|', 0, ':',
        '"', '~', '<', '>', '?'
    };
    uint8_t shift = (modifiers & 0x22U) ? 1U : 0U;
    uint8_t ctrl = (modifiers & 0x11U) ? 1U : 0U;
    char c;

    switch (key) {
        case 0x4F:
            return AOS_KEY_RIGHT;
        case 0x50:
            return AOS_KEY_LEFT;
        case 0x51:
            return AOS_KEY_HISTORY_NEXT;
        case 0x52:
            return AOS_KEY_HISTORY_PREV;
        default:
            break;
    }

    if (key < sizeof(normal)) {
        c = shift ? shifted[key] : normal[key];
        if (ctrl && c >= 'a' && c <= 'z') {
            return (char)(c - 'a' + 1);
        }
        if (ctrl && c >= 'A' && c <= 'Z') {
            return (char)(c - 'A' + 1);
        }
        return c;
    }
    return 0;
}

static void xhci_handle_keyboard_report(void) {
    uint8_t modifiers;
    uint8_t aos_modifiers = 0;

    if (!g_keyboard_report) {
        return;
    }

    modifiers = g_keyboard_report[0];
    if (modifiers & 0x11U) {
        aos_modifiers |= AOS_INPUT_MOD_CTRL;
    }
    if (modifiers & 0x22U) {
        aos_modifiers |= AOS_INPUT_MOD_SHIFT;
    }
    if (modifiers & 0x44U) {
        aos_modifiers |= AOS_INPUT_MOD_ALT;
    }

    for (uint32_t i = 2; i < 8; i++) {
        uint8_t key = g_keyboard_report[i];
        char c;
        if (key == 0 || hid_key_was_down(key)) {
            continue;
        }
        c = hid_key_to_tty(key, modifiers);
        if (c != 0) {
            input_push_key(AOS_INPUT_SOURCE_USB, key, c, 1, aos_modifiers);
        } else {
            input_push_key(AOS_INPUT_SOURCE_USB, key, 0, 1, aos_modifiers);
        }
    }

    for (uint32_t i = 0; i < 6; i++) {
        g_keyboard_last_keys[i] = g_keyboard_report[i + 2];
    }
}

static int32_t clamp_i32(int32_t value, int32_t min, int32_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int xhci_hid_transfer_ok(uint32_t completion) {
    return completion == XHCI_COMPLETION_SUCCESS ||
           completion == XHCI_COMPLETION_SHORT_PACKET;
}

static void xhci_handle_mouse_report(void) {
    uint32_t buttons = 0;
    int32_t old_x;
    int32_t old_y;
    int32_t max_x = gfx_is_ready() && gfx_width() > 0 ? (int32_t)gfx_width() - 1 : 1023;
    int32_t max_y = gfx_is_ready() && gfx_height() > 0 ? (int32_t)gfx_height() - 1 : 767;

    if (!g_mouse_report) {
        return;
    }

    old_x = g_mouse_x;
    old_y = g_mouse_y;
    if (g_mouse_report[0] & 0x01U) buttons |= AOS_POINTER_LEFT;
    if (g_mouse_report[0] & 0x02U) buttons |= AOS_POINTER_RIGHT;
    if (g_mouse_report[0] & 0x04U) buttons |= AOS_POINTER_MIDDLE;

    if (g_interrupt_in_max_packet >= 5) {
        uint32_t raw_x = (uint32_t)g_mouse_report[1] | ((uint32_t)g_mouse_report[2] << 8);
        uint32_t raw_y = (uint32_t)g_mouse_report[3] | ((uint32_t)g_mouse_report[4] << 8);
        if (raw_x > 32767U) raw_x = 32767U;
        if (raw_y > 32767U) raw_y = 32767U;
        g_mouse_x = (int32_t)((raw_x * (uint32_t)max_x) / 32767U);
        g_mouse_y = (int32_t)((raw_y * (uint32_t)max_y) / 32767U);
    } else {
        int32_t dx = (int8_t)g_mouse_report[1];
        int32_t dy = (int8_t)g_mouse_report[2];
        g_mouse_x = clamp_i32(g_mouse_x + dx, 0, max_x);
        g_mouse_y = clamp_i32(g_mouse_y - dy, 0, max_y);
    }

    g_mouse_buttons = buttons;
    input_push_pointer(AOS_INPUT_SOURCE_USB, g_mouse_x, g_mouse_y,
                       g_mouse_x - old_x, g_mouse_y - old_y, g_mouse_buttons);
}

void xhci_poll_keyboard(void) {
    struct xhci_trb* event;
    uint32_t type;
    uint64_t event_trb_phys;
    uint32_t completion;
    uint32_t endpoint_id;

    if (g_interrupt_in_owner != XHCI_INTERRUPT_OWNER_KEYBOARD &&
        g_interrupt_in_owner != XHCI_INTERRUPT_OWNER_MOUSE) {
        return;
    }
    if (!g_interrupt_in_ring) {
        return;
    }
    if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_KEYBOARD && g_keyboard_pending_trb == 0) {
        return;
    }
    if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_MOUSE && g_mouse_pending_trb == 0) {
        return;
    }

    event = &g_event_ring[g_event_dequeue];
    if ((event->control & XHCI_TRB_CYCLE) != g_event_cycle) {
        return;
    }

    type = (event->control >> XHCI_TRB_TYPE_SHIFT) & 0x3FU;
    if (type != XHCI_TRB_TYPE_TRANSFER_EVENT) {
        xhci_advance_event();
        return;
    }

    event_trb_phys = ((uint64_t)event->parameter_hi << 32) | event->parameter_lo;
    completion = (event->status >> 24) & 0xFFU;
    endpoint_id = (event->control >> 16) & 0x1FU;
    xhci_advance_event();

    if (endpoint_id != g_interrupt_in_dci) {
        return;
    }

    if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_KEYBOARD) {
        if (event_trb_phys != g_keyboard_pending_trb) {
            return;
        }
        g_keyboard_pending_trb = 0;
        if (xhci_hid_transfer_ok(completion)) {
            xhci_handle_keyboard_report();
        } else {
            serial_print("xhci: keyboard transfer failed cc=0x");
            serial_print_hex8((uint8_t)completion);
            serial_print("\n");
        }
        xhci_submit_keyboard_transfer();
    } else {
        if (event_trb_phys != g_mouse_pending_trb) {
            return;
        }
        g_mouse_pending_trb = 0;
        if (xhci_hid_transfer_ok(completion)) {
            xhci_handle_mouse_report();
        } else {
            serial_print("xhci: mouse transfer failed cc=0x");
            serial_print_hex8((uint8_t)completion);
            serial_print("\n");
        }
        xhci_submit_mouse_transfer();
    }
}

void xhci_register_driver(void) {
    const struct pci_device* dev = find_xhci();
    uint32_t raw_bar0;
    uint64_t phys_base;
    uint64_t page_base;
    uint64_t page_offset;
    uint32_t cap_reg;
    uint8_t cap_length;
    uint16_t hci_version;
    uint32_t hcsparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint8_t max_slots;
    uint16_t max_intrs;
    uint8_t max_ports;
    uint32_t connected_ports;
    char status[64];

    if (!dev) {
        driver_register_system(DRIVER_CLASS_USB, "aos-xhci", "not found: no xHCI controller");
        serial_print("xhci: no controller found\n");
        return;
    }

    driver_claim_pci(dev->vendor_id, dev->device_id, "aos-xhci", "claimed: probing mmio");
    pci_enable_xhci(dev);

    raw_bar0 = dev->bar[0];
    if ((raw_bar0 & 0x1U) != 0) {
        driver_update_pci_status(dev->vendor_id, dev->device_id, "error: xhci BAR0 is not MMIO");
        serial_print("xhci: BAR0 is not MMIO\n");
        return;
    }

    phys_base = (uint64_t)(raw_bar0 & 0xFFFFFFF0U);
    page_base = phys_base & ~0xFFFULL;
    page_offset = phys_base & 0xFFFULL;
    map_mmio_window(page_base, XHCI_MMIO_VIRT_BASE, XHCI_MMIO_SIZE + page_offset);
    g_xhci_mmio = (volatile uint8_t*)(XHCI_MMIO_VIRT_BASE + page_offset);

    cap_reg = mmio_read32(0x00);
    cap_length = (uint8_t)(cap_reg & 0xFFU);
    hci_version = (uint16_t)(cap_reg >> 16);
    hcsparams1 = mmio_read32(0x04);
    g_context_size = (mmio_read32(0x10) & XHCI_HCCPARAMS1_CSZ) ? 64U : 32U;
    dboff = mmio_read32(0x14) & ~0x3U;
    rtsoff = mmio_read32(0x18) & ~0x1FU;
    g_xhci_op_base = cap_length;
    g_xhci_db_base = dboff;
    g_xhci_rt_base = rtsoff;
    max_slots = (uint8_t)(hcsparams1 & 0xFFU);
    max_intrs = (uint16_t)((hcsparams1 >> 8) & 0x7FFU);
    max_ports = (uint8_t)((hcsparams1 >> 24) & 0xFFU);

    status[0] = 'r';
    status[1] = 'e';
    status[2] = 'a';
    status[3] = 'd';
    status[4] = 'y';
    status[5] = ':';
    status[6] = ' ';
    status[7] = 'm';
    status[8] = 'm';
    status[9] = 'i';
    status[10] = 'o';
    status[11] = ' ';
    status[12] = 'p';
    status[13] = 'r';
    status[14] = 'o';
    status[15] = 'b';
    status[16] = 'e';
    status[17] = 'd';
    status[18] = ',';
    status[19] = ' ';
    status[20] = 'e';
    status[21] = 'n';
    status[22] = 'u';
    status[23] = 'm';
    status[24] = ' ';
    status[25] = 'n';
    status[26] = 'e';
    status[27] = 'x';
    status[28] = 't';
    status[29] = '\0';
    driver_update_pci_status(dev->vendor_id, dev->device_id, status);

    serial_print("xhci: MMIO BAR0=0x");
    serial_print_hex32((uint32_t)phys_base);
    serial_print(" cap=0x");
    serial_print_hex8(cap_length);
    serial_print(" version=0x");
    serial_print_hex16(hci_version);
    serial_print(" slots=");
    serial_print_hex8(max_slots);
    serial_print(" intrs=");
    serial_print_hex16(max_intrs);
    serial_print(" ports=");
    serial_print_hex8(max_ports);
    serial_print(" dboff=0x");
    serial_print_hex32(dboff);
    serial_print(" rtsoff=0x");
    serial_print_hex32(rtsoff);
    serial_print("\n");

    connected_ports = xhci_scan_ports(cap_length, max_ports);
    if (connected_ports > 0) {
        status[0] = 'r';
        status[1] = 'e';
        status[2] = 'a';
        status[3] = 'd';
        status[4] = 'y';
        status[5] = ':';
        status[6] = ' ';
        status[7] = 'p';
        status[8] = 'o';
        status[9] = 'r';
        status[10] = 't';
        status[11] = 's';
        status[12] = ' ';
        status[13] = 's';
        status[14] = 'c';
        status[15] = 'a';
        status[16] = 'n';
        status[17] = 'n';
        status[18] = 'e';
        status[19] = 'd';
        status[20] = ',';
        status[21] = ' ';
        status[22] = 'd';
        status[23] = 'e';
        status[24] = 'v';
        status[25] = 'i';
        status[26] = 'c';
        status[27] = 'e';
        status[28] = 's';
        status[29] = ' ';
        status[30] = 'p';
        status[31] = 'r';
        status[32] = 'e';
        status[33] = 's';
        status[34] = 'e';
        status[35] = 'n';
        status[36] = 't';
        status[37] = '\0';
    } else {
        status[0] = 'r';
        status[1] = 'e';
        status[2] = 'a';
        status[3] = 'd';
        status[4] = 'y';
        status[5] = ':';
        status[6] = ' ';
        status[7] = 'p';
        status[8] = 'o';
        status[9] = 'r';
        status[10] = 't';
        status[11] = 's';
        status[12] = ' ';
        status[13] = 's';
        status[14] = 'c';
        status[15] = 'a';
        status[16] = 'n';
        status[17] = 'n';
        status[18] = 'e';
        status[19] = 'd';
        status[20] = ',';
        status[21] = ' ';
        status[22] = 'n';
        status[23] = 'o';
        status[24] = ' ';
        status[25] = 'd';
        status[26] = 'e';
        status[27] = 'v';
        status[28] = 'i';
        status[29] = 'c';
        status[30] = 'e';
        status[31] = 's';
        status[32] = '\0';
    }
    driver_update_pci_status(dev->vendor_id, dev->device_id, status);

    if (connected_ports > 0) {
        int slot_id;
        if (xhci_start_controller(max_slots) == 0) {
            slot_id = xhci_enable_slot();
            if (slot_id > 0) {
                serial_print("xhci: slot enabled id=0x");
                serial_print_hex8((uint8_t)slot_id);
                serial_print("\n");
                if (xhci_address_device((uint32_t)slot_id) == 0) {
                    if (xhci_get_device_descriptor((uint32_t)slot_id) == 0 &&
                        xhci_get_config_descriptor((uint32_t)slot_id) == 0 &&
                        xhci_control_set_configuration((uint32_t)slot_id, 1) == 0 &&
                        xhci_configure_interrupt_in_endpoint((uint32_t)slot_id) == 0) {
                        if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_BLUETOOTH) {
                            if (xhci_bluetooth_hci_init() == 0) {
                                driver_update_pci_status(dev->vendor_id, dev->device_id, "ready: bluetooth HCI checked");
                            } else {
                                driver_update_pci_status(dev->vendor_id, dev->device_id, "warn: bluetooth HCI init failed");
                            }
                        } else if (g_interrupt_in_owner == XHCI_INTERRUPT_OWNER_MOUSE) {
                            driver_update_pci_status(dev->vendor_id, dev->device_id, "ready: usb mouse endpoint configured");
                        } else {
                            driver_update_pci_status(dev->vendor_id, dev->device_id, "ready: usb keyboard endpoint configured");
                        }
                    } else {
                        driver_update_pci_status(dev->vendor_id, dev->device_id, "warn: usb setup failed");
                    }
                } else {
                    driver_update_pci_status(dev->vendor_id, dev->device_id, "warn: address device failed");
                }
            } else {
                driver_update_pci_status(dev->vendor_id, dev->device_id, "warn: enable slot failed");
            }
        } else {
            driver_update_pci_status(dev->vendor_id, dev->device_id, "warn: controller start failed");
        }
    }
}
