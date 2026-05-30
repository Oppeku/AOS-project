/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef TTY_H
#define TTY_H

#include <stdint.h>

void tty_init(void);
void tty_keyboard_input(char c);
char tty_read_byte(void);
uint64_t tty_read(char* buf, uint64_t len);
uint64_t tty_write(const char* buf, uint64_t len);
uint32_t tty_pending(void);
void tty_get_winsize(uint16_t* rows, uint16_t* cols);

#endif
