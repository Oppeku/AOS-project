/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <credentials.h>
#include <timer.h>

struct sha256_ctx {
    uint32_t state[8];
    uint64_t total_len;
    uint8_t block[64];
    uint32_t block_len;
};

static const uint32_t sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint64_t salt_counter;

static size_t local_strlen(const char* value) {
    size_t size = 0;
    while (value && value[size]) size++;
    return size;
}

static void local_memset(void* dst, uint8_t value, size_t size) {
    uint8_t* bytes = (uint8_t*)dst;
    while (size--) *bytes++ = value;
}

static void local_memcpy(void* dst, const void* src, size_t size) {
    uint8_t* out = (uint8_t*)dst;
    const uint8_t* in = (const uint8_t*)src;
    while (size--) *out++ = *in++;
}

void credentials_wipe(void* data, size_t size) {
    volatile uint8_t* bytes = (volatile uint8_t*)data;
    while (size--) *bytes++ = 0;
}

static uint32_t load_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void store_be32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t rotate_right(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32U - count));
}

static void sha256_transform(struct sha256_ctx* ctx,
                             const uint8_t block[64]) {
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;

    for (uint32_t i = 0; i < 16; i++) words[i] = load_be32(block + i * 4U);
    for (uint32_t i = 16; i < 64; i++) {
        uint32_t s0 = rotate_right(words[i - 15], 7) ^
                      rotate_right(words[i - 15], 18) ^
                      (words[i - 15] >> 3);
        uint32_t s1 = rotate_right(words[i - 2], 17) ^
                      rotate_right(words[i - 2], 19) ^
                      (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (uint32_t i = 0; i < 64; i++) {
        uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
                        rotate_right(e, 25);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + sum1 + choose + sha256_k[i] + words[i];
        uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
                        rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
    credentials_wipe(words, sizeof(words));
}

static void sha256_init(struct sha256_ctx* ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };
    local_memcpy(ctx->state, initial, sizeof(initial));
    ctx->total_len = 0;
    ctx->block_len = 0;
}

static void sha256_update(struct sha256_ctx* ctx, const void* input,
                          size_t size) {
    const uint8_t* data = (const uint8_t*)input;
    ctx->total_len += size;
    while (size--) {
        ctx->block[ctx->block_len++] = *data++;
        if (ctx->block_len == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static void sha256_final(struct sha256_ctx* ctx,
                         uint8_t digest[AOS_PASSWORD_HASH_SIZE]) {
    uint64_t bit_len = ctx->total_len * 8U;
    ctx->block[ctx->block_len++] = 0x80U;
    if (ctx->block_len > 56U) {
        while (ctx->block_len < 64U) ctx->block[ctx->block_len++] = 0;
        sha256_transform(ctx, ctx->block);
        ctx->block_len = 0;
    }
    while (ctx->block_len < 56U) ctx->block[ctx->block_len++] = 0;
    for (uint32_t i = 0; i < 8; i++) {
        ctx->block[56U + i] = (uint8_t)(bit_len >> (56U - i * 8U));
    }
    sha256_transform(ctx, ctx->block);
    for (uint32_t i = 0; i < 8; i++) store_be32(digest + i * 4U, ctx->state[i]);
    credentials_wipe(ctx, sizeof(*ctx));
}

static void hmac_sha256(const uint8_t* key, size_t key_size,
                        const void* first, size_t first_size,
                        const void* second, size_t second_size,
                        uint8_t out[AOS_PASSWORD_HASH_SIZE]) {
    struct sha256_ctx ctx;
    uint8_t key_block[64];
    uint8_t inner[AOS_PASSWORD_HASH_SIZE];
    uint8_t pad[64];

    local_memset(key_block, 0, sizeof(key_block));
    if (key_size > sizeof(key_block)) {
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_size);
        sha256_final(&ctx, key_block);
    } else {
        local_memcpy(key_block, key, key_size);
    }
    for (size_t i = 0; i < sizeof(pad); i++) pad[i] = key_block[i] ^ 0x36U;
    sha256_init(&ctx);
    sha256_update(&ctx, pad, sizeof(pad));
    sha256_update(&ctx, first, first_size);
    if (second && second_size) sha256_update(&ctx, second, second_size);
    sha256_final(&ctx, inner);

    for (size_t i = 0; i < sizeof(pad); i++) pad[i] = key_block[i] ^ 0x5cU;
    sha256_init(&ctx);
    sha256_update(&ctx, pad, sizeof(pad));
    sha256_update(&ctx, inner, sizeof(inner));
    sha256_final(&ctx, out);
    credentials_wipe(key_block, sizeof(key_block));
    credentials_wipe(inner, sizeof(inner));
    credentials_wipe(pad, sizeof(pad));
}

static void pbkdf2_sha256(const char* password,
                          const uint8_t salt[AOS_PASSWORD_SALT_SIZE],
                          uint32_t iterations,
                          uint8_t out[AOS_PASSWORD_HASH_SIZE]) {
    uint8_t block_index[4] = {0, 0, 0, 1};
    uint8_t value[AOS_PASSWORD_HASH_SIZE];
    size_t password_size = local_strlen(password);

    hmac_sha256((const uint8_t*)password, password_size,
                salt, AOS_PASSWORD_SALT_SIZE,
                block_index, sizeof(block_index), value);
    local_memcpy(out, value, AOS_PASSWORD_HASH_SIZE);
    for (uint32_t round = 1; round < iterations; round++) {
        hmac_sha256((const uint8_t*)password, password_size,
                    value, sizeof(value), NULL, 0, value);
        for (size_t i = 0; i < AOS_PASSWORD_HASH_SIZE; i++) out[i] ^= value[i];
    }
    credentials_wipe(value, sizeof(value));
}

static char hex_digit(uint8_t value) {
    return value < 10U ? (char)('0' + value) : (char)('a' + value - 10U);
}

static int parse_hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static void bytes_to_hex(const uint8_t* bytes, size_t size, char* out) {
    for (size_t i = 0; i < size; i++) {
        out[i * 2U] = hex_digit(bytes[i] >> 4);
        out[i * 2U + 1U] = hex_digit(bytes[i] & 0x0fU);
    }
    out[size * 2U] = 0;
}

static int hex_to_bytes(const char* text, size_t chars, uint8_t* out) {
    if ((chars & 1U) != 0) return -1;
    for (size_t i = 0; i < chars / 2U; i++) {
        int high = parse_hex_digit(text[i * 2U]);
        int low = parse_hex_digit(text[i * 2U + 1U]);
        if (high < 0 || low < 0) return -1;
        out[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

static int append_text(char* out, size_t out_size, size_t* used,
                       const char* text) {
    size_t size = local_strlen(text);
    if (*used + size + 1U > out_size) return -1;
    local_memcpy(out + *used, text, size);
    *used += size;
    out[*used] = 0;
    return 0;
}

static int append_decimal(char* out, size_t out_size, size_t* used,
                          uint32_t value) {
    char digits[11];
    size_t at = sizeof(digits);
    digits[--at] = 0;
    do {
        digits[--at] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value && at);
    return append_text(out, out_size, used, digits + at);
}

int credentials_format_password_hash(
    const char* password,
    const uint8_t salt[AOS_PASSWORD_SALT_SIZE],
    uint32_t iterations,
    char* out,
    size_t out_size) {
    uint8_t digest[AOS_PASSWORD_HASH_SIZE];
    char salt_hex[AOS_PASSWORD_SALT_SIZE * 2U + 1U];
    char digest_hex[AOS_PASSWORD_HASH_SIZE * 2U + 1U];
    size_t used = 0;
    int rc = -1;

    if (!password || !salt || !out || iterations == 0 || out_size == 0) return -1;
    out[0] = 0;
    pbkdf2_sha256(password, salt, iterations, digest);
    bytes_to_hex(salt, AOS_PASSWORD_SALT_SIZE, salt_hex);
    bytes_to_hex(digest, AOS_PASSWORD_HASH_SIZE, digest_hex);
    if (append_text(out, out_size, &used, "$aos-pbkdf2-sha256$") == 0 &&
        append_decimal(out, out_size, &used, iterations) == 0 &&
        append_text(out, out_size, &used, "$") == 0 &&
        append_text(out, out_size, &used, salt_hex) == 0 &&
        append_text(out, out_size, &used, "$") == 0 &&
        append_text(out, out_size, &used, digest_hex) == 0) {
        rc = 0;
    }
    credentials_wipe(digest, sizeof(digest));
    credentials_wipe(digest_hex, sizeof(digest_hex));
    return rc;
}

static int text_starts_with(const char* value, const char* prefix) {
    size_t i = 0;
    if (!value || !prefix) return 0;
    while (prefix[i]) {
        if (value[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static int constant_time_equal(const uint8_t* left, const uint8_t* right,
                               size_t size) {
    uint8_t difference = 0;
    for (size_t i = 0; i < size; i++) difference |= left[i] ^ right[i];
    return difference == 0;
}

static int verify_pbkdf2(const char* password, const char* encoded) {
    static const char prefix[] = "$aos-pbkdf2-sha256$";
    const char* cursor = encoded + sizeof(prefix) - 1U;
    uint32_t iterations = 0;
    uint8_t salt[AOS_PASSWORD_SALT_SIZE];
    uint8_t expected[AOS_PASSWORD_HASH_SIZE];
    uint8_t actual[AOS_PASSWORD_HASH_SIZE];
    int result = 0;

    if (!text_starts_with(encoded, prefix)) return 0;
    if (*cursor < '1' || *cursor > '9') return 0;
    while (*cursor >= '0' && *cursor <= '9') {
        if (iterations > 1000000U) return 0;
        iterations = iterations * 10U + (uint32_t)(*cursor++ - '0');
    }
    if (*cursor++ != '$' || iterations == 0 || iterations > 1000000U) return 0;
    if (hex_to_bytes(cursor, AOS_PASSWORD_SALT_SIZE * 2U, salt) != 0) return 0;
    cursor += AOS_PASSWORD_SALT_SIZE * 2U;
    if (*cursor++ != '$') return 0;
    if (hex_to_bytes(cursor, AOS_PASSWORD_HASH_SIZE * 2U, expected) != 0 ||
        cursor[AOS_PASSWORD_HASH_SIZE * 2U] != 0) {
        credentials_wipe(salt, sizeof(salt));
        return 0;
    }
    pbkdf2_sha256(password, salt, iterations, actual);
    result = constant_time_equal(expected, actual, sizeof(actual));
    credentials_wipe(salt, sizeof(salt));
    credentials_wipe(expected, sizeof(expected));
    credentials_wipe(actual, sizeof(actual));
    return result;
}

static int verify_legacy(const char* username, const char* password,
                         const char* encoded) {
    static const char prefix[] = "$aos$";
    uint64_t hash = 1469598103934665603ULL;
    const char* fields[2] = {username, password};
    char expected[17];

    if (!text_starts_with(encoded, prefix) || local_strlen(encoded) != 21U) return 0;
    for (size_t field = 0; field < 2; field++) {
        for (size_t i = 0; fields[field] && fields[field][i]; i++) {
            hash ^= (uint8_t)fields[field][i];
            hash *= 1099511628211ULL;
        }
        hash ^= 0xffU;
        hash *= 1099511628211ULL;
    }
    for (int i = 15; i >= 0; i--) {
        expected[i] = hex_digit((uint8_t)(hash & 0x0fU));
        hash >>= 4;
    }
    expected[16] = 0;
    return constant_time_equal((const uint8_t*)expected,
                               (const uint8_t*)encoded + sizeof(prefix) - 1U,
                               16U);
}

int credentials_verify_password(const char* username,
                                const char* password,
                                const char* encoded) {
    if (!username || !password || !encoded || !encoded[0]) return 0;
    if (text_starts_with(encoded, "$aos-pbkdf2-sha256$")) {
        return verify_pbkdf2(password, encoded);
    }
    if (text_starts_with(encoded, "$aos$")) {
        return verify_legacy(username, password, encoded);
    }
    return 0;
}

static uint64_t read_tsc(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static int read_rdrand(uint64_t* value) {
    uint32_t eax, ebx, ecx, edx;
    unsigned char ok;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1U), "c"(0U));
    (void)eax; (void)ebx; (void)edx;
    if ((ecx & (1U << 30)) == 0) return 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok));
        if (ok) return 1;
    }
    return 0;
}

void credentials_generate_salt(uint8_t salt[AOS_PASSWORD_SALT_SIZE],
                               const void* context, size_t context_size) {
    struct sha256_ctx ctx;
    uint8_t digest[AOS_PASSWORD_HASH_SIZE];
    uint64_t entropy[5];

    salt_counter++;
    entropy[0] = read_tsc();
    entropy[1] = timer_get_ticks();
    entropy[2] = salt_counter;
    entropy[3] = (uint64_t)(uintptr_t)&ctx;
    if (!read_rdrand(&entropy[4])) {
        entropy[4] = read_tsc() ^ (salt_counter * 0x9e3779b97f4a7c15ULL);
    }
    sha256_init(&ctx);
    sha256_update(&ctx, entropy, sizeof(entropy));
    if (context && context_size) sha256_update(&ctx, context, context_size);
    sha256_final(&ctx, digest);
    local_memcpy(salt, digest, AOS_PASSWORD_SALT_SIZE);
    credentials_wipe(entropy, sizeof(entropy));
    credentials_wipe(digest, sizeof(digest));
}
