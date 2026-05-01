/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>

void aos_panic(const char* title, const char* detail);
void aos_panic_hex(const char* title, const char* detail_prefix, uint64_t value);

#endif
