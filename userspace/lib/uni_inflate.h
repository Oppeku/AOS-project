/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef UNI_INFLATE_H
#define UNI_INFLATE_H

#include <stddef.h>
#include <stdint.h>

int uni_inflate_gzip(const uint8_t* input,
                     size_t input_size,
                     uint8_t* output,
                     size_t output_capacity,
                     size_t* output_size);

#endif
