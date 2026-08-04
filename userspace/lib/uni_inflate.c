/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "uni_inflate.h"

#define DEFLATE_MAX_BITS 15
#define DEFLATE_MAX_SYMBOLS 320

struct bit_stream {
    const uint8_t* input;
    size_t size;
    size_t pos;
    uint32_t bits;
    unsigned bit_count;
};

struct huffman {
    uint16_t count[DEFLATE_MAX_BITS + 1];
    uint16_t symbol[DEFLATE_MAX_SYMBOLS];
};

static const uint16_t length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};

static const uint8_t length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

static const uint16_t distance_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385, 24577,
};

static const uint8_t distance_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

static int bits_get(struct bit_stream* stream, unsigned count, uint32_t* out) {
    uint32_t mask;

    if (!stream || !out || count > 24) return -1;
    while (stream->bit_count < count) {
        if (stream->pos >= stream->size) return -1;
        stream->bits |= (uint32_t)stream->input[stream->pos++] << stream->bit_count;
        stream->bit_count += 8;
    }
    mask = count == 0 ? 0U : ((1U << count) - 1U);
    *out = stream->bits & mask;
    stream->bits >>= count;
    stream->bit_count -= count;
    return 0;
}

static int bits_align_byte(struct bit_stream* stream) {
    uint32_t ignored;
    unsigned discard;

    if (!stream) return -1;
    discard = stream->bit_count & 7U;
    return discard ? bits_get(stream, discard, &ignored) : 0;
}

static int huffman_construct(struct huffman* table,
                             const uint8_t* lengths,
                             unsigned symbol_count) {
    uint16_t offsets[DEFLATE_MAX_BITS + 1];
    int left = 1;

    if (!table || !lengths || symbol_count > DEFLATE_MAX_SYMBOLS) return -1;
    for (unsigned i = 0; i <= DEFLATE_MAX_BITS; i++) table->count[i] = 0;
    for (unsigned i = 0; i < symbol_count; i++) {
        if (lengths[i] > DEFLATE_MAX_BITS) return -1;
        table->count[lengths[i]]++;
    }
    if (table->count[0] == symbol_count) return 0;

    for (unsigned bits = 1; bits <= DEFLATE_MAX_BITS; bits++) {
        left = (left << 1) - table->count[bits];
        if (left < 0) return -1;
    }

    offsets[1] = 0;
    for (unsigned bits = 1; bits < DEFLATE_MAX_BITS; bits++) {
        offsets[bits + 1] = offsets[bits] + table->count[bits];
    }
    for (unsigned symbol = 0; symbol < symbol_count; symbol++) {
        uint8_t len = lengths[symbol];
        if (len != 0) table->symbol[offsets[len]++] = (uint16_t)symbol;
    }
    return left;
}

static int huffman_decode(struct bit_stream* stream,
                          const struct huffman* table,
                          uint16_t* symbol_out) {
    uint32_t code = 0;
    uint32_t first = 0;
    uint32_t index = 0;

    if (!stream || !table || !symbol_out) return -1;
    for (unsigned len = 1; len <= DEFLATE_MAX_BITS; len++) {
        uint32_t bit;
        uint32_t count;

        if (bits_get(stream, 1, &bit) != 0) return -1;
        code |= bit;
        count = table->count[len];
        if (code >= first && code - first < count) {
            *symbol_out = table->symbol[index + (code - first)];
            return 0;
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

static int fixed_tables(struct huffman* literal, struct huffman* distance) {
    uint8_t lengths[288];
    uint8_t distances[32];

    for (unsigned i = 0; i <= 143; i++) lengths[i] = 8;
    for (unsigned i = 144; i <= 255; i++) lengths[i] = 9;
    for (unsigned i = 256; i <= 279; i++) lengths[i] = 7;
    for (unsigned i = 280; i <= 287; i++) lengths[i] = 8;
    for (unsigned i = 0; i < 32; i++) distances[i] = 5;
    return huffman_construct(literal, lengths, 288) < 0 ||
           huffman_construct(distance, distances, 32) < 0 ? -1 : 0;
}

static int dynamic_tables(struct bit_stream* stream,
                          struct huffman* literal,
                          struct huffman* distance) {
    static const uint8_t order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
    };
    struct huffman code_table;
    uint8_t code_lengths[19];
    uint8_t lengths[DEFLATE_MAX_SYMBOLS];
    uint32_t value;
    unsigned literal_count;
    unsigned distance_count;
    unsigned code_count;
    unsigned total;
    unsigned at = 0;

    for (unsigned i = 0; i < 19; i++) code_lengths[i] = 0;
    if (bits_get(stream, 5, &value) != 0) return -1;
    literal_count = (unsigned)value + 257;
    if (bits_get(stream, 5, &value) != 0) return -1;
    distance_count = (unsigned)value + 1;
    if (bits_get(stream, 4, &value) != 0) return -1;
    code_count = (unsigned)value + 4;
    total = literal_count + distance_count;
    if (literal_count > 286 || distance_count > 32 ||
        total > DEFLATE_MAX_SYMBOLS) return -1;

    for (unsigned i = 0; i < code_count; i++) {
        if (bits_get(stream, 3, &value) != 0) return -1;
        code_lengths[order[i]] = (uint8_t)value;
    }
    if (huffman_construct(&code_table, code_lengths, 19) < 0) return -1;

    while (at < total) {
        uint16_t symbol;
        unsigned repeat;
        uint8_t repeated;

        if (huffman_decode(stream, &code_table, &symbol) != 0) return -1;
        if (symbol <= 15) {
            lengths[at++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 16) {
            if (at == 0 || bits_get(stream, 2, &value) != 0) return -1;
            repeat = (unsigned)value + 3;
            repeated = lengths[at - 1];
        } else if (symbol == 17) {
            if (bits_get(stream, 3, &value) != 0) return -1;
            repeat = (unsigned)value + 3;
            repeated = 0;
        } else if (symbol == 18) {
            if (bits_get(stream, 7, &value) != 0) return -1;
            repeat = (unsigned)value + 11;
            repeated = 0;
        } else {
            return -1;
        }
        if (repeat > total - at) return -1;
        while (repeat--) lengths[at++] = repeated;
    }

    if (lengths[256] == 0 ||
        huffman_construct(literal, lengths, literal_count) < 0 ||
        huffman_construct(distance, lengths + literal_count, distance_count) < 0) {
        return -1;
    }
    return 0;
}

static int inflate_codes(struct bit_stream* stream,
                         const struct huffman* literal,
                         const struct huffman* distance,
                         uint8_t* output,
                         size_t output_capacity,
                         size_t* output_pos) {
    for (;;) {
        uint16_t symbol;

        if (huffman_decode(stream, literal, &symbol) != 0) return -1;
        if (symbol < 256) {
            if (*output_pos >= output_capacity) return -1;
            output[(*output_pos)++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 256) return 0;
        if (symbol < 257 || symbol > 285) return -1;

        {
            unsigned length_index = symbol - 257;
            uint32_t extra = 0;
            uint16_t distance_symbol;
            uint32_t length = length_base[length_index];
            uint32_t distance_value;

            if (length_extra[length_index] &&
                bits_get(stream, length_extra[length_index], &extra) != 0) {
                return -1;
            }
            length += extra;
            if (huffman_decode(stream, distance, &distance_symbol) != 0 ||
                distance_symbol >= 30) {
                return -1;
            }
            extra = 0;
            if (distance_extra[distance_symbol] &&
                bits_get(stream, distance_extra[distance_symbol], &extra) != 0) {
                return -1;
            }
            distance_value = distance_base[distance_symbol] + extra;
            if (distance_value == 0 || distance_value > *output_pos ||
                length > output_capacity - *output_pos) {
                return -1;
            }
            while (length--) {
                output[*output_pos] = output[*output_pos - distance_value];
                (*output_pos)++;
            }
        }
    }
}

static int inflate_deflate(const uint8_t* input,
                           size_t input_size,
                           uint8_t* output,
                           size_t output_capacity,
                           size_t* output_size) {
    struct bit_stream stream;
    size_t output_pos = 0;
    uint32_t final = 0;

    stream.input = input;
    stream.size = input_size;
    stream.pos = 0;
    stream.bits = 0;
    stream.bit_count = 0;

    while (!final) {
        uint32_t type;
        struct huffman literal;
        struct huffman distance;

        if (bits_get(&stream, 1, &final) != 0 ||
            bits_get(&stream, 2, &type) != 0) return -1;
        if (type == 0) {
            uint32_t len;
            uint32_t inverse;

            if (bits_align_byte(&stream) != 0 ||
                bits_get(&stream, 16, &len) != 0 ||
                bits_get(&stream, 16, &inverse) != 0 ||
                (len ^ 0xffffU) != inverse ||
                len > output_capacity - output_pos) {
                return -1;
            }
            for (uint32_t i = 0; i < len; i++) {
                uint32_t byte;
                if (bits_get(&stream, 8, &byte) != 0) return -1;
                output[output_pos++] = (uint8_t)byte;
            }
        } else if (type == 1) {
            if (fixed_tables(&literal, &distance) != 0 ||
                inflate_codes(&stream, &literal, &distance,
                              output, output_capacity, &output_pos) != 0) {
                return -1;
            }
        } else if (type == 2) {
            if (dynamic_tables(&stream, &literal, &distance) != 0 ||
                inflate_codes(&stream, &literal, &distance,
                              output, output_capacity, &output_pos) != 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    *output_size = output_pos;
    return 0;
}

static uint32_t read_le32(const uint8_t* value) {
    return (uint32_t)value[0] |
           ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) |
           ((uint32_t)value[3] << 24);
}

static uint32_t gzip_crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xffffffffU;

    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

int uni_inflate_gzip(const uint8_t* input,
                     size_t input_size,
                     uint8_t* output,
                     size_t output_capacity,
                     size_t* output_size) {
    size_t pos = 10;
    size_t deflate_end;
    uint8_t flags;
    size_t produced = 0;
    uint32_t expected_crc;
    uint32_t expected_size;

    if (!input || !output || !output_size || input_size < 18 ||
        input[0] != 0x1f || input[1] != 0x8b || input[2] != 8) {
        return -1;
    }
    flags = input[3];
    if (flags & 0xe0U) return -1;
    deflate_end = input_size - 8;

    if (flags & 0x04U) {
        uint16_t extra_len;
        if (pos + 2 > deflate_end) return -1;
        extra_len = (uint16_t)input[pos] | ((uint16_t)input[pos + 1] << 8);
        pos += 2;
        if (extra_len > deflate_end - pos) return -1;
        pos += extra_len;
    }
    if (flags & 0x08U) {
        while (pos < deflate_end && input[pos] != 0) pos++;
        if (pos >= deflate_end) return -1;
        pos++;
    }
    if (flags & 0x10U) {
        while (pos < deflate_end && input[pos] != 0) pos++;
        if (pos >= deflate_end) return -1;
        pos++;
    }
    if (flags & 0x02U) {
        if (pos + 2 > deflate_end) return -1;
        pos += 2;
    }
    if (pos >= deflate_end ||
        inflate_deflate(input + pos, deflate_end - pos,
                        output, output_capacity, &produced) != 0) {
        return -1;
    }

    expected_crc = read_le32(input + input_size - 8);
    expected_size = read_le32(input + input_size - 4);
    if ((uint32_t)produced != expected_size ||
        gzip_crc32(output, produced) != expected_crc) {
        return -1;
    }
    *output_size = produced;
    return 0;
}
