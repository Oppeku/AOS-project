/* SPDX-License-Identifier: GPL-3.0-or-later
 * XZ container integration for Uni. The embedded decoder sources in xz/
 * are public domain code by Lasse Collin and Igor Pavlov.
 */

#include "uni_xz.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UNI_XZ_ARENA_SIZE (64U * 1024U)
#define XZ_STREAM_HEADER_SIZE 12U
#define XZ_STREAM_FOOTER_SIZE 12U

static uint64_t xz_arena[UNI_XZ_ARENA_SIZE / sizeof(uint64_t)];
static uint8_t* xz_allocator_base = (uint8_t*)xz_arena;
static size_t xz_allocator_size = UNI_XZ_ARENA_SIZE;
static size_t xz_arena_used;
static uint32_t xz_crc32_table[256];
static uint64_t xz_crc64_table[256];
static bool xz_crc_tables_ready;

static void* uni_xz_alloc(size_t size) {
    size_t aligned = (size + 15U) & ~(size_t)15U;
    void* result;

    if (aligned < size || xz_arena_used > xz_allocator_size ||
        aligned > xz_allocator_size - xz_arena_used) {
        return NULL;
    }
    result = xz_allocator_base + xz_arena_used;
    xz_arena_used += aligned;
    return result;
}

static void uni_xz_free(void* pointer) {
    (void)pointer;
}

static void* uni_xz_memcpy(void* destination,
                           const void* source, size_t size) {
    uint8_t* out = (uint8_t*)destination;
    const uint8_t* in = (const uint8_t*)source;
    while (size--) *out++ = *in++;
    return destination;
}

static void* uni_xz_memmove(void* destination,
                            const void* source, size_t size) {
    uint8_t* out = (uint8_t*)destination;
    const uint8_t* in = (const uint8_t*)source;

    if (out < in) {
        for (size_t i = 0; i < size; i++) out[i] = in[i];
    } else if (out > in) {
        while (size) {
            size--;
            out[size] = in[size];
        }
    }
    return destination;
}

static void* uni_xz_memset(void* destination, int value, size_t size) {
    uint8_t* out = (uint8_t*)destination;
    while (size--) *out++ = (uint8_t)value;
    return destination;
}

static int uni_xz_memcmp(const void* left, const void* right, size_t size) {
    const uint8_t* a = (const uint8_t*)left;
    const uint8_t* b = (const uint8_t*)right;
    while (size--) {
        if (*a != *b) return *a < *b ? -1 : 1;
        a++;
        b++;
    }
    return 0;
}

static void uni_xz_use_allocator(void* workspace, size_t workspace_size) {
    xz_allocator_base = workspace ? (uint8_t*)workspace : (uint8_t*)xz_arena;
    xz_allocator_size = workspace ? workspace_size : UNI_XZ_ARENA_SIZE;
    xz_arena_used = 0;
}

static void uni_xz_crc_init(void) {
    if (xz_crc_tables_ready) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc32 = i;
        uint64_t crc64 = i;
        for (unsigned bit = 0; bit < 8; bit++) {
            crc32 = (crc32 >> 1) ^
                    (0xedb88320U & (0U - (crc32 & 1U)));
            crc64 = (crc64 >> 1) ^
                    (0xc96c5795d7870f42ULL &
                     (0ULL - (crc64 & 1ULL)));
        }
        xz_crc32_table[i] = crc32;
        xz_crc64_table[i] = crc64;
    }
    xz_crc_tables_ready = true;
}

static uint32_t xz_crc32(const uint8_t* data,
                         size_t size, uint32_t crc) {
    crc = ~crc;
    while (size--) {
        crc = xz_crc32_table[(crc ^ *data++) & 0xffU] ^ (crc >> 8);
    }
    return ~crc;
}

static uint64_t uni_xz_crc64(const uint8_t* data,
                             size_t size, uint64_t crc) {
    crc = ~crc;
    while (size--) {
        crc = xz_crc64_table[(crc ^ *data++) & 0xffU] ^ (crc >> 8);
    }
    return ~crc;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include "xz/xz_dec_bcj.c"
#include "xz/xz_dec_lzma2.c"
#include "xz/xz_dec_stream.c"
#pragma GCC diagnostic pop

static uint32_t read_le32(const uint8_t* data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static int decode_vli(const uint8_t* input, size_t limit,
                      size_t* position, uint64_t* value) {
    uint64_t result = 0;
    unsigned shift = 0;

    while (*position < limit && shift <= 56) {
        uint8_t byte = input[(*position)++];
        if (shift == 56 && (byte & 0xfeU) != 0) return -1;
        result |= (uint64_t)(byte & 0x7fU) << shift;
        if ((byte & 0x80U) == 0) {
            if (shift != 0 && byte == 0) return -1;
            *value = result;
            return 0;
        }
        shift += 7;
    }
    return -1;
}

int uni_xz_dictionary_size(const uint8_t* input,
                           size_t input_size,
                           uint32_t* dictionary_size) {
    size_t header_size;
    size_t limit;
    size_t position;
    uint8_t flags;
    unsigned filter_count;

    if (!input || !dictionary_size || input_size < 24 ||
        input[0] != 0xfd || input[1] != '7' || input[2] != 'z' ||
        input[3] != 'X' || input[4] != 'Z' || input[5] != 0) {
        return -1;
    }
    header_size = ((size_t)input[12] + 1U) * 4U;
    if (header_size < 8 || header_size > input_size - 12) return -1;
    uni_xz_crc_init();
    if (xz_crc32(input + 12, header_size - 4, 0) !=
        read_le32(input + 12 + header_size - 4)) {
        return -1;
    }

    flags = input[13];
    if ((flags & 0x3cU) != 0) return -1;
    filter_count = (flags & 0x03U) + 1U;
    limit = 12 + header_size - 4;
    position = 14;
    if (flags & 0x40U) {
        uint64_t ignored;
        if (decode_vli(input, limit, &position, &ignored) != 0) return -1;
    }
    if (flags & 0x80U) {
        uint64_t ignored;
        if (decode_vli(input, limit, &position, &ignored) != 0) return -1;
    }

    for (unsigned filter = 0; filter < filter_count; filter++) {
        uint64_t filter_id;
        uint64_t properties_size;
        if (decode_vli(input, limit, &position, &filter_id) != 0 ||
            decode_vli(input, limit, &position, &properties_size) != 0 ||
            properties_size > limit - position) {
            return -1;
        }
        if (filter_id == 0x21U) {
            uint8_t property;
            uint64_t size;
            if (properties_size != 1) return -1;
            property = input[position];
            if (property > 39) return -1;
            size = (uint64_t)(2U + (property & 1U));
            size <<= (property >> 1) + 11U;
            if (size == 0 || size > UINT32_MAX) return -1;
            *dictionary_size = (uint32_t)size;
            return 0;
        }
        position += (size_t)properties_size;
    }
    return -1;
}

static int stream_end_without_padding(const uint8_t* input,
                                      size_t input_size,
                                      size_t* stream_end) {
    size_t end = input_size;

    while (end >= 4 && input[end - 1] == 0 && input[end - 2] == 0 &&
           input[end - 3] == 0 && input[end - 4] == 0) {
        end -= 4;
    }
    if (end < XZ_STREAM_HEADER_SIZE + XZ_STREAM_FOOTER_SIZE) return -1;
    *stream_end = end;
    return 0;
}

int uni_xz_output_size(const uint8_t* input,
                       size_t input_size,
                       size_t* output_size) {
    size_t stream_end;
    size_t footer;
    size_t index_start;
    size_t index_crc;
    size_t position;
    uint64_t backward_size;
    uint64_t record_count;
    uint64_t total = 0;

    if (!input || !output_size ||
        stream_end_without_padding(input, input_size, &stream_end) != 0 ||
        input[0] != 0xfd || input[1] != '7' || input[2] != 'z' ||
        input[3] != 'X' || input[4] != 'Z' || input[5] != 0) {
        return -1;
    }

    footer = stream_end - XZ_STREAM_FOOTER_SIZE;
    if (input[footer + 10] != 'Y' || input[footer + 11] != 'Z' ||
        input[6] != input[footer + 8] ||
        input[7] != input[footer + 9]) {
        return -1;
    }

    uni_xz_crc_init();
    if (xz_crc32(input + 6, 2, 0) != read_le32(input + 8) ||
        xz_crc32(input + footer + 4, 6, 0) != read_le32(input + footer)) {
        return -1;
    }

    backward_size = ((uint64_t)read_le32(input + footer + 4) + 1ULL) * 4ULL;
    if (backward_size > footer ||
        backward_size < 8 ||
        backward_size > (uint64_t)SIZE_MAX) {
        return -1;
    }
    index_start = footer - (size_t)backward_size;
    index_crc = footer - 4;
    if (index_start < XZ_STREAM_HEADER_SIZE ||
        index_start >= index_crc ||
        input[index_start] != 0 ||
        xz_crc32(input + index_start, index_crc - index_start, 0) !=
            read_le32(input + index_crc)) {
        return -1;
    }

    position = index_start + 1;
    if (decode_vli(input, index_crc, &position, &record_count) != 0 ||
        record_count > (uint64_t)(index_crc - position) / 2ULL) {
        return -1;
    }

    for (uint64_t record = 0; record < record_count; record++) {
        uint64_t unpadded;
        uint64_t uncompressed;
        if (decode_vli(input, index_crc, &position, &unpadded) != 0 ||
            decode_vli(input, index_crc, &position, &uncompressed) != 0 ||
            unpadded == 0 || uncompressed > (uint64_t)SIZE_MAX - total) {
            return -1;
        }
        total += uncompressed;
    }
    while (position < index_crc) {
        if (input[position++] != 0) return -1;
    }
    if (total == 0 || total > (uint64_t)SIZE_MAX) return -1;
    *output_size = (size_t)total;
    return 0;
}

int uni_xz_decode(const uint8_t* input,
                  size_t input_size,
                  uint8_t* output,
                  size_t output_capacity,
                  size_t* output_size) {
    struct xz_dec* decoder;
    struct xz_buf buffer;
    enum xz_ret result;
    size_t expected_size;

    if (!input || !output || !output_size ||
        uni_xz_output_size(input, input_size, &expected_size) != 0 ||
        expected_size > output_capacity) {
        return -1;
    }

    uni_xz_use_allocator(NULL, 0);
    decoder = xz_dec_init(XZ_SINGLE, 0);
    if (!decoder) return -1;

    buffer.in = input;
    buffer.in_pos = 0;
    buffer.in_size = input_size;
    buffer.out = output;
    buffer.out_pos = 0;
    buffer.out_size = output_capacity;

    result = xz_dec_run(decoder, &buffer);
    xz_dec_end(decoder);
    if (result != XZ_STREAM_END || buffer.out_pos != expected_size ||
        (input_size - buffer.in_pos) % 4 != 0) {
        return -1;
    }
    for (size_t i = buffer.in_pos; i < input_size; i++) {
        if (input[i] != 0) return -1;
    }

    *output_size = buffer.out_pos;
    return 0;
}

int uni_xz_stream_init(struct uni_xz_stream* stream,
                       void* workspace,
                       size_t workspace_size,
                       uint32_t dictionary_limit) {
    if (!stream || !workspace || workspace_size < 4096 ||
        dictionary_limit == 0 ||
        workspace_size <= (size_t)dictionary_limit) {
        return -1;
    }

    stream->decoder = NULL;
    stream->finished = 0;
    uni_xz_use_allocator(workspace, workspace_size);
    stream->decoder = xz_dec_init(XZ_DYNALLOC, dictionary_limit);
    return stream->decoder ? 0 : -1;
}

int uni_xz_stream_decode(struct uni_xz_stream* stream,
                         const uint8_t* input,
                         size_t input_size,
                         size_t* input_used,
                         uint8_t* output,
                         size_t output_size,
                         size_t* output_used) {
    struct xz_buf buffer;
    enum xz_ret result;

    if (!stream || !stream->decoder || stream->finished ||
        (!input && input_size != 0) || !input_used ||
        (!output && output_size != 0) || !output_used) {
        return -1;
    }

    buffer.in = input;
    buffer.in_pos = 0;
    buffer.in_size = input_size;
    buffer.out = output;
    buffer.out_pos = 0;
    buffer.out_size = output_size;
    result = xz_dec_run((struct xz_dec*)stream->decoder, &buffer);
    *input_used = buffer.in_pos;
    *output_used = buffer.out_pos;

    if (result == XZ_STREAM_END) {
        stream->finished = 1;
        return 0;
    }
    return result == XZ_OK ? 0 : -1;
}

void uni_xz_stream_end(struct uni_xz_stream* stream) {
    if (!stream) return;
    if (stream->decoder) {
        xz_dec_end((struct xz_dec*)stream->decoder);
    }
    stream->decoder = NULL;
    stream->finished = 0;
    uni_xz_use_allocator(NULL, 0);
}
