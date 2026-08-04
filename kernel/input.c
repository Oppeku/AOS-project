/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <input.h>
#include <stdint.h>

#define INPUT_EVENT_QUEUE_SIZE 128
#define INPUT_TTY_QUEUE_SIZE 1024

static struct aos_input_event event_queue[INPUT_EVENT_QUEUE_SIZE];
static uint32_t event_head;
static uint32_t event_tail;
static char tty_queue[INPUT_TTY_QUEUE_SIZE];
static uint32_t tty_head;
static uint32_t tty_tail;

void input_init(void) {
    event_head = 0;
    event_tail = 0;
    tty_head = 0;
    tty_tail = 0;
}

static void input_queue_event(const struct aos_input_event* event) {
    uint32_t next = (event_head + 1) % INPUT_EVENT_QUEUE_SIZE;

    if (next != event_tail) {
        event_queue[event_head] = *event;
        event_head = next;
    }
}

static int input_coalesce_pointer(const struct aos_input_event* event) {
    uint32_t previous_index;
    struct aos_input_event* previous;

    if (event_head == event_tail) {
        return 0;
    }

    previous_index = (event_head + INPUT_EVENT_QUEUE_SIZE - 1) % INPUT_EVENT_QUEUE_SIZE;
    previous = &event_queue[previous_index];
    if (previous->key != AOS_KEY_POINTER ||
        previous->source != event->source ||
        previous->pointer_buttons != event->pointer_buttons) {
        return 0;
    }

    previous->pointer_dx += event->pointer_dx;
    previous->pointer_dy += event->pointer_dy;
    previous->pointer_x = event->pointer_x;
    previous->pointer_y = event->pointer_y;
    return 1;
}

static void input_queue_tty_byte(char ascii) {
    uint32_t next = (tty_head + 1) % INPUT_TTY_QUEUE_SIZE;

    if (next != tty_tail) {
        tty_queue[tty_head] = ascii;
        tty_head = next;
    }
}

void input_push_key(uint8_t source, uint16_t key, char ascii, uint8_t pressed, uint8_t modifiers) {
    struct aos_input_event event;

    event.key = key;
    event.ascii = ascii;
    event.pressed = pressed ? 1U : 0U;
    event.modifiers = modifiers;
    event.source = source;
    event.pointer_x = 0;
    event.pointer_y = 0;
    event.pointer_dx = 0;
    event.pointer_dy = 0;
    event.pointer_buttons = 0;
    input_queue_event(&event);

    if (event.pressed && ascii != 0) {
        input_queue_tty_byte(ascii);
    }
}

void input_push_char(uint8_t source, char ascii) {
    input_push_key(source, (uint16_t)(uint8_t)ascii, ascii, 1, 0);
}

void input_push_pointer(uint8_t source, int32_t x, int32_t y, int32_t dx, int32_t dy, uint32_t buttons) {
    struct aos_input_event event;

    event.key = AOS_KEY_POINTER;
    event.ascii = 0;
    event.pressed = buttons ? 1U : 0U;
    event.modifiers = 0;
    event.source = source;
    event.pointer_x = x;
    event.pointer_y = y;
    event.pointer_dx = dx;
    event.pointer_dy = dy;
    event.pointer_buttons = buttons;
    if (!input_coalesce_pointer(&event)) {
        input_queue_event(&event);
    }
}

int input_pop_event(struct aos_input_event* out) {
    if (event_head == event_tail || !out) {
        return 0;
    }

    *out = event_queue[event_tail];
    event_tail = (event_tail + 1) % INPUT_EVENT_QUEUE_SIZE;
    return 1;
}

void input_clear_events(void) {
    event_tail = event_head;
}

char input_read_tty_byte(void) {
    char c;

    if (tty_head == tty_tail) {
        return 0;
    }

    c = tty_queue[tty_tail];
    tty_tail = (tty_tail + 1) % INPUT_TTY_QUEUE_SIZE;
    return c;
}

uint32_t input_tty_pending(void) {
    if (tty_head >= tty_tail) {
        return tty_head - tty_tail;
    }
    return INPUT_TTY_QUEUE_SIZE - tty_tail + tty_head;
}
