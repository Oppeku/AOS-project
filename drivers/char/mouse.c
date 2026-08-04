/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <gfx.h>
#include <input.h>
#include <mouse.h>
#include <stdint.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL  0x02
#define PS2_STATUS_AUX_DATA    0x20

#define PS2_MOUSE_ACK    0xFA
#define PS2_MOUSE_RESEND 0xFE

static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static uint8_t packet[3];
static uint8_t packet_index;
static uint8_t mouse_ready;

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void outb_local(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int mouse_wait_input(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
            return 1;
        }
    }
    return 0;
}

static int mouse_wait_output(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
            return 1;
        }
    }
    return 0;
}

static int mouse_read_data(uint8_t* out) {
    if (!mouse_wait_output()) {
        return 0;
    }
    *out = inb(PS2_DATA_PORT);
    return 1;
}

static void mouse_flush_output(void) {
    for (uint32_t i = 0; i < 32; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0) {
            break;
        }
        (void)inb(PS2_DATA_PORT);
    }
}

static int mouse_write_controller(uint8_t command) {
    if (mouse_wait_input()) {
        outb_local(PS2_COMMAND_PORT, command);
        return 1;
    }
    return 0;
}

static int mouse_write_data(uint8_t data) {
    if (mouse_wait_input()) {
        outb_local(PS2_DATA_PORT, data);
        return 1;
    }
    return 0;
}

static int mouse_write_device_ack(uint8_t command) {
    uint8_t reply = 0;

    for (uint32_t retry = 0; retry < 3; retry++) {
        if (!mouse_write_controller(0xD4)) {
            return 0;
        }
        if (!mouse_write_data(command)) {
            return 0;
        }
        if (!mouse_read_data(&reply)) {
            return 0;
        }
        if (reply == PS2_MOUSE_ACK) {
            return 1;
        }
        if (reply != PS2_MOUSE_RESEND) {
            return 0;
        }
    }
    return 0;
}

static int32_t clamp_i32(int32_t value, int32_t min, int32_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void mouse_init(void) {
    uint8_t config = 0;

    mouse_ready = 0;
    packet_index = 0;

    if (gfx_is_ready()) {
        mouse_x = (int32_t)(gfx_width() / 2);
        mouse_y = (int32_t)(gfx_height() / 2);
    }

    mouse_flush_output();
    if (!mouse_write_controller(0xA8)) {
        return;
    }
    if (!mouse_write_controller(0x20)) {
        return;
    }
    if (!mouse_read_data(&config)) {
        return;
    }
    config |= 0x02;
    config &= (uint8_t)~0x20;
    if (!mouse_write_controller(0x60)) {
        return;
    }
    if (!mouse_write_data(config)) {
        return;
    }

    mouse_flush_output();
    if (!mouse_write_device_ack(0xF6)) return;
    (void)mouse_write_device_ack(0xE6);
    if (mouse_write_device_ack(0xF3)) {
        (void)mouse_write_device_ack(100);
    }
    if (mouse_write_device_ack(0xE8)) {
        (void)mouse_write_device_ack(3);
    }
    if (!mouse_write_device_ack(0xF4)) return;

    mouse_ready = 1;
    input_push_pointer(AOS_INPUT_SOURCE_MOUSE, mouse_x, mouse_y, 0, 0, 0);
}

void mouse_handler_main(void) {
    uint8_t status = inb(PS2_STATUS_PORT);
    uint8_t data;

    if (!mouse_ready) {
        return;
    }

    if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
        return;
    }

    data = inb(PS2_DATA_PORT);
    if ((status & PS2_STATUS_AUX_DATA) == 0) {
        return;
    }

    if (packet_index == 0 && (data & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) {
        return;
    }
    packet_index = 0;

    int32_t dx = (int8_t)packet[1];
    int32_t dy = (int8_t)packet[2];
    int32_t max_x = gfx_is_ready() && gfx_width() > 0 ? (int32_t)gfx_width() - 1 : 1023;
    int32_t max_y = gfx_is_ready() && gfx_height() > 0 ? (int32_t)gfx_height() - 1 : 767;
    uint32_t buttons = 0;

    if (packet[0] & 0x40) {
        dx = (packet[0] & 0x10) ? -127 : 127;
    }
    if (packet[0] & 0x80) {
        dy = (packet[0] & 0x20) ? -127 : 127;
    }

    mouse_x = clamp_i32(mouse_x + dx, 0, max_x);
    mouse_y = clamp_i32(mouse_y - dy, 0, max_y);
    if (packet[0] & 0x01) buttons |= AOS_POINTER_LEFT;
    if (packet[0] & 0x02) buttons |= AOS_POINTER_RIGHT;
    if (packet[0] & 0x04) buttons |= AOS_POINTER_MIDDLE;

    input_push_pointer(AOS_INPUT_SOURCE_MOUSE, mouse_x, mouse_y, dx, -dy, buttons);
}
