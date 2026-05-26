/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <ata.h>
#include <blkdev.h>
#include <stdint.h>
#include <stddef.h>

#define ATA_IO_BASE 0x1F0
#define ATA_CTRL_BASE 0x3F6

#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_SECCOUNT0 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_COMMAND 7
#define ATA_REG_STATUS 7

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

struct ata_device {
    uint16_t io;
    uint16_t ctrl;
    uint32_t sectors;
};

static struct ata_device g_primary_master;

extern void serial_print(const char* s);
extern uint8_t aos_boot_verbose;

static inline void outb_local(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb_local(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t inw_local(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw_local(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void ata_io_wait(const struct ata_device* dev) {
    (void)inb_local(dev->ctrl);
    (void)inb_local(dev->ctrl);
    (void)inb_local(dev->ctrl);
    (void)inb_local(dev->ctrl);
}

static int ata_wait_not_busy(const struct ata_device* dev) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t status = inb_local(dev->io + ATA_REG_STATUS);
        if ((status & ATA_SR_BSY) == 0) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(const struct ata_device* dev) {
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t status = inb_local(dev->io + ATA_REG_STATUS);
        if (status & (ATA_SR_ERR | ATA_SR_DF)) {
            return -1;
        }
        if ((status & ATA_SR_BSY) == 0 && (status & ATA_SR_DRQ)) {
            return 0;
        }
    }
    return -1;
}

static int ata_identify(struct ata_device* dev) {
    uint16_t identify[256];

    outb_local(dev->io + ATA_REG_HDDEVSEL, 0xA0);
    ata_io_wait(dev);
    outb_local(dev->io + ATA_REG_SECCOUNT0, 0);
    outb_local(dev->io + ATA_REG_LBA0, 0);
    outb_local(dev->io + ATA_REG_LBA1, 0);
    outb_local(dev->io + ATA_REG_LBA2, 0);
    outb_local(dev->io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait(dev);

    if (inb_local(dev->io + ATA_REG_STATUS) == 0) {
        return -1;
    }
    if (ata_wait_not_busy(dev) != 0) {
        return -1;
    }
    if (inb_local(dev->io + ATA_REG_LBA1) != 0 || inb_local(dev->io + ATA_REG_LBA2) != 0) {
        return -1;
    }
    if (ata_wait_drq(dev) != 0) {
        return -1;
    }

    for (size_t i = 0; i < 256; i++) {
        identify[i] = inw_local(dev->io + ATA_REG_DATA);
    }

    dev->sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
    return dev->sectors ? 0 : -1;
}

static int ata_pio_read_one(const struct ata_device* dev, uint32_t lba, void* buffer) {
    uint16_t* out = (uint16_t*)buffer;

    if (ata_wait_not_busy(dev) != 0) {
        return -1;
    }
    outb_local(dev->io + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb_local(dev->io + ATA_REG_SECCOUNT0, 1);
    outb_local(dev->io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb_local(dev->io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb_local(dev->io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb_local(dev->io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    if (ata_wait_drq(dev) != 0) {
        return -1;
    }
    for (size_t i = 0; i < 256; i++) {
        out[i] = inw_local(dev->io + ATA_REG_DATA);
    }
    return 0;
}

static int ata_pio_write_one(const struct ata_device* dev, uint32_t lba, const void* buffer) {
    const uint16_t* in = (const uint16_t*)buffer;

    if (ata_wait_not_busy(dev) != 0) {
        return -1;
    }
    outb_local(dev->io + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb_local(dev->io + ATA_REG_SECCOUNT0, 1);
    outb_local(dev->io + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb_local(dev->io + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb_local(dev->io + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb_local(dev->io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    if (ata_wait_drq(dev) != 0) {
        return -1;
    }
    for (size_t i = 0; i < 256; i++) {
        outw_local(dev->io + ATA_REG_DATA, in[i]);
    }
    outb_local(dev->io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return ata_wait_not_busy(dev);
}

static int ata_read_blocks(void* ctx, uint64_t lba, uint32_t block_count, void* buffer) {
    const struct ata_device* dev = (const struct ata_device*)ctx;
    uint8_t* out = (uint8_t*)buffer;

    if (!dev || !buffer || lba > dev->sectors || block_count > dev->sectors - lba) {
        return -1;
    }
    for (uint32_t i = 0; i < block_count; i++) {
        if (ata_pio_read_one(dev, (uint32_t)(lba + i), out + ((uint64_t)i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ata_write_blocks(void* ctx, uint64_t lba, uint32_t block_count, const void* buffer) {
    const struct ata_device* dev = (const struct ata_device*)ctx;
    const uint8_t* in = (const uint8_t*)buffer;

    if (!dev || !buffer || lba > dev->sectors || block_count > dev->sectors - lba) {
        return -1;
    }
    for (uint32_t i = 0; i < block_count; i++) {
        if (ata_pio_write_one(dev, (uint32_t)(lba + i), in + ((uint64_t)i * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

uint32_t ata_init_primary_master(void) {
    g_primary_master.io = ATA_IO_BASE;
    g_primary_master.ctrl = ATA_CTRL_BASE;
    g_primary_master.sectors = 0;

    if (ata_identify(&g_primary_master) != 0) {
        serial_print("ATA: primary master not found\n");
        return BLKDEV_INVALID_ID;
    }

    if (aos_boot_verbose) {
        serial_print("ATA: primary master ready\n");
    }
    return blkdev_register_ops("ata0",
                               (uint64_t)g_primary_master.sectors * 512ULL,
                               512,
                               0,
                               &g_primary_master,
                               ata_read_blocks,
                               ata_write_blocks);
}
