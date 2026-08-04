/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_CREDENTIALS_H
#define AOS_CREDENTIALS_H

#include <stddef.h>
#include <stdint.h>

#define AOS_PASSWORD_ITERATIONS 12000U
#define AOS_PASSWORD_SALT_SIZE 16U
#define AOS_PASSWORD_HASH_SIZE 32U
#define AOS_PASSWORD_ENCODED_MAX 160U

void credentials_generate_salt(uint8_t salt[AOS_PASSWORD_SALT_SIZE],
                               const void* context, size_t context_size);
int credentials_format_password_hash(
    const char* password,
    const uint8_t salt[AOS_PASSWORD_SALT_SIZE],
    uint32_t iterations,
    char* out,
    size_t out_size);
int credentials_verify_password(const char* username,
                                const char* password,
                                const char* encoded);
void credentials_wipe(void* data, size_t size);

#endif
