/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_INPUT_H
#define AOS_INPUT_H

#include <stdint.h>

#define AOS_KEY_NONE 0
#define AOS_KEY_UP 0x11
#define AOS_KEY_DOWN 0x12
#define AOS_KEY_LEFT 0x13
#define AOS_KEY_RIGHT 0x14

#define AOS_INPUT_SOURCE_PS2 1
#define AOS_INPUT_SOURCE_USB 2
#define AOS_INPUT_FLAG_PRESSED 1
#define AOS_INPUT_FLAG_CTRL  (1 << 8)
#define AOS_INPUT_FLAG_SHIFT (1 << 9)
#define AOS_INPUT_FLAG_ALT   (1 << 10)

struct aos_input_event {
    uint32_t key;
    uint32_t flags;
    uint32_t ascii;
    uint32_t source;
};

int64_t aos_input_poll(struct aos_input_event* out);

#endif
