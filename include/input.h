/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_INPUT_KERNEL_H
#define AOS_INPUT_KERNEL_H

#include <stdint.h>

#define AOS_INPUT_SOURCE_PS2 1U
#define AOS_INPUT_SOURCE_USB 2U

#define AOS_INPUT_MOD_CTRL  (1U << 0)
#define AOS_INPUT_MOD_SHIFT (1U << 1)
#define AOS_INPUT_MOD_ALT   (1U << 2)

#define AOS_KEY_NONE 0x00U
#define AOS_KEY_HISTORY_PREV 0x11U
#define AOS_KEY_HISTORY_NEXT 0x12U
#define AOS_KEY_LEFT 0x13U
#define AOS_KEY_RIGHT 0x14U

struct aos_input_event {
    uint16_t key;
    char ascii;
    uint8_t pressed;
    uint8_t modifiers;
    uint8_t source;
};

void input_init(void);
void input_push_key(uint8_t source, uint16_t key, char ascii, uint8_t pressed, uint8_t modifiers);
void input_push_char(uint8_t source, char ascii);
int input_pop_event(struct aos_input_event* out);
void input_clear_events(void);
char input_read_tty_byte(void);
uint32_t input_tty_pending(void);

#endif
