/*
 * AOS freestanding configuration for XZ Embedded.
 *
 * XZ Embedded and this adaptation are in the public domain.
 */

#ifndef XZ_CONFIG_H
#define XZ_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define XZ_DEC_SINGLE
#define XZ_DEC_DYNALLOC
#define XZ_DEC_X86
#define XZ_INTERNAL_CRC32 0

#define GFP_KERNEL 0
#define kmalloc(size, flags) uni_xz_alloc(size)
#define kfree(ptr) uni_xz_free(ptr)
#define vmalloc(size) uni_xz_alloc(size)
#define vfree(ptr) uni_xz_free(ptr)

#define memcpy(dst, src, size) uni_xz_memcpy(dst, src, size)
#define memmove(dst, src, size) uni_xz_memmove(dst, src, size)
#define memset(dst, value, size) uni_xz_memset(dst, value, size)
#define memcmp(left, right, size) uni_xz_memcmp(left, right, size)
#define memeq(left, right, size) (uni_xz_memcmp(left, right, size) == 0)
#define memzero(buf, size) uni_xz_memset(buf, 0, size)

#define min(x, y) ((x) < (y) ? (x) : (y))
#define min_t(type, x, y) min(x, y)

#ifndef __always_inline
#define __always_inline inline __attribute__((__always_inline__))
#endif

#ifndef noinline_for_stack
#define noinline_for_stack __attribute__((__noinline__))
#endif

#ifndef NOINLINE
#define NOINLINE __attribute__((__noinline__))
#endif

#include "xz.h"

static inline uint32_t get_unaligned_le32(const uint8_t* buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

static inline uint32_t get_unaligned_be32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

static inline void put_unaligned_le32(uint32_t value, uint8_t* buf) {
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
}

static inline void put_unaligned_be32(uint32_t value, uint8_t* buf) {
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

#define get_le32 get_unaligned_le32

#endif
