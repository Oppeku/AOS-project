/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef INSTALLER_H
#define INSTALLER_H

#include <stddef.h>
#include <stdint.h>
#include <aos_install.h>

enum installer_payload_kind {
    INSTALLER_PAYLOAD_BOOT = 1,
    INSTALLER_PAYLOAD_BIOS = 2,
};

void installer_init(void);
int installer_set_payload(uint32_t kind, uint32_t start, uint32_t end);
int installer_get_status(struct aos_install_status* out);
int installer_begin(const struct aos_install_config* config);
int installer_advance(void);
int installer_cancel(void);

#endif
