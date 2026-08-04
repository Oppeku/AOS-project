/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef UNI_XZ_H
#define UNI_XZ_H

#include <stddef.h>
#include <stdint.h>

struct uni_xz_stream {
    void* decoder;
    int finished;
};

int uni_xz_output_size(const uint8_t* input,
                       size_t input_size,
                       size_t* output_size);

int uni_xz_dictionary_size(const uint8_t* input,
                           size_t input_size,
                           uint32_t* dictionary_size);

int uni_xz_decode(const uint8_t* input,
                  size_t input_size,
                  uint8_t* output,
                  size_t output_capacity,
                  size_t* output_size);

int uni_xz_stream_init(struct uni_xz_stream* stream,
                       void* workspace,
                       size_t workspace_size,
                       uint32_t dictionary_limit);

int uni_xz_stream_decode(struct uni_xz_stream* stream,
                         const uint8_t* input,
                         size_t input_size,
                         size_t* input_used,
                         uint8_t* output,
                         size_t output_size,
                         size_t* output_used);

void uni_xz_stream_end(struct uni_xz_stream* stream);

#endif
