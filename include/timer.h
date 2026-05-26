/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void init_timer(uint32_t frequency);
void timer_tick(void);
uint64_t timer_get_ticks(void);
uint32_t timer_get_frequency(void);

#endif
