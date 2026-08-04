/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_SESSION_ABI_H
#define AOS_SESSION_ABI_H

#include <stdint.h>

#define AOS_SESSION_ABI_VERSION 1U
#define AOS_SESSION_USERNAME_MAX 32U
#define AOS_SESSION_PASSWORD_MAX 64U
#define AOS_SESSION_HOME_MAX 64U

enum aos_session_action {
    AOS_SESSION_QUERY = 0,
    AOS_SESSION_LOGIN = 1,
    AOS_SESSION_AUTOLOGIN = 2,
    AOS_SESSION_LOGOUT = 3,
};

struct aos_session_request {
    uint32_t version;
    uint32_t reserved;
    char username[AOS_SESSION_USERNAME_MAX];
    char password[AOS_SESSION_PASSWORD_MAX];
};

struct aos_session_status {
    uint32_t version;
    uint32_t installed;
    uint32_t live_mode;
    uint32_t authenticated;
    uint32_t login_required;
    uint32_t autologin;
    uint32_t administrator;
    uint32_t uid;
    uint32_t gid;
    uint32_t failed_attempts;
    uint32_t retry_after_seconds;
    uint32_t reserved;
    char username[AOS_SESSION_USERNAME_MAX];
    char home[AOS_SESSION_HOME_MAX];
    char message[96];
};

#endif
