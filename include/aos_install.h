/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_INSTALL_H
#define AOS_INSTALL_H

#include <stdint.h>

#define AOS_INSTALL_ABI_VERSION 1U
#define AOS_INSTALL_BOOT_OFFSET (1ULL * 1024ULL * 1024ULL)
#define AOS_INSTALL_BOOT_SIZE (16ULL * 1024ULL * 1024ULL)
#define AOS_INSTALL_ROOT_OFFSET (AOS_INSTALL_BOOT_OFFSET + AOS_INSTALL_BOOT_SIZE)
#define AOS_INSTALL_MIN_DISK_SIZE (32ULL * 1024ULL * 1024ULL)

enum aos_install_action {
    AOS_INSTALL_QUERY = 0,
    AOS_INSTALL_BEGIN = 1,
    AOS_INSTALL_ADVANCE = 2,
    AOS_INSTALL_CANCEL = 3,
};

enum aos_install_partition_mode {
    AOS_INSTALL_PARTITION_AUTO = 0,
    AOS_INSTALL_PARTITION_MANUAL = 1,
};

enum aos_install_step {
    AOS_INSTALL_STEP_IDLE = 0,
    AOS_INSTALL_STEP_PREPARING = 1,
    AOS_INSTALL_STEP_CHECKING_DISK = 2,
    AOS_INSTALL_STEP_PARTITIONING = 3,
    AOS_INSTALL_STEP_FORMATTING = 4,
    AOS_INSTALL_STEP_COPYING_KERNEL = 5,
    AOS_INSTALL_STEP_COPYING_INITRD = 6,
    AOS_INSTALL_STEP_COPYING_USERSPACE = 7,
    AOS_INSTALL_STEP_WRITING_CONFIG = 8,
    AOS_INSTALL_STEP_INSTALLING_BOOTLOADER = 9,
    AOS_INSTALL_STEP_CREATING_LOG = 10,
    AOS_INSTALL_STEP_FINISHING = 11,
    AOS_INSTALL_STEP_COMPLETE = 12,
    AOS_INSTALL_STEP_ERROR = 13,
};

enum aos_install_error {
    AOS_INSTALL_ERROR_NONE = 0,
    AOS_INSTALL_ERROR_NOT_LIVE = 1,
    AOS_INSTALL_ERROR_PAYLOAD_MISSING = 2,
    AOS_INSTALL_ERROR_BAD_CONFIG = 3,
    AOS_INSTALL_ERROR_BAD_CONFIRMATION = 4,
    AOS_INSTALL_ERROR_NO_DISK = 5,
    AOS_INSTALL_ERROR_DISK_TOO_SMALL = 6,
    AOS_INSTALL_ERROR_READ_ONLY = 7,
    AOS_INSTALL_ERROR_MANUAL_LAYOUT = 8,
    AOS_INSTALL_ERROR_IO = 9,
    AOS_INSTALL_ERROR_FILESYSTEM = 10,
    AOS_INSTALL_ERROR_VERIFY = 11,
    AOS_INSTALL_ERROR_BUSY = 12,
    AOS_INSTALL_ERROR_CANCELLED = 13,
};

enum aos_install_app_profile {
    AOS_INSTALL_APPS_MINIMAL = 0,
    AOS_INSTALL_APPS_BASIC = 1,
    AOS_INSTALL_APPS_FULL = 2,
    AOS_INSTALL_APPS_CUSTOM = 3,
};

enum aos_install_driver_mode {
    AOS_INSTALL_DRIVERS_STABLE = 0,
    AOS_INSTALL_DRIVERS_EXPERIMENTAL = 1,
    AOS_INSTALL_DRIVERS_ASK = 2,
    AOS_INSTALL_DRIVERS_SKIP = 3,
};

struct aos_install_config {
    uint32_t version;
    uint32_t target_blkdev_id;
    uint32_t app_mask;
    uint32_t driver_scan_mask;
    uint8_t partition_mode;
    uint8_t account_admin;
    uint8_t auto_login;
    uint8_t wifi_enabled;
    uint8_t wifi_static;
    uint8_t driver_mode;
    uint8_t app_profile;
    uint8_t reserved0;
    char username[32];
    char password[64];
    char password_confirm[64];
    char hostname[64];
    char language[32];
    char region[32];
    char timezone[40];
    char keyboard[32];
    char wifi_ssid[33];
    char wifi_password[64];
    char wifi_ip[40];
    char wifi_gateway[40];
    char wifi_dns[40];
    char confirmation[4];
};

struct aos_install_status {
    uint32_t version;
    uint32_t live_mode;
    uint32_t payload_ready;
    uint32_t running;
    uint32_t step;
    uint32_t progress_percent;
    uint32_t error;
    uint32_t target_blkdev_id;
    uint64_t target_size;
    uint64_t bytes_done;
    uint64_t bytes_total;
    char target_name[16];
    char message[96];
};

#endif
