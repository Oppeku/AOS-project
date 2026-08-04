/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_SESSION_H
#define AOS_SESSION_H

#include <aos_session.h>

void session_prepare_boot_identity(void);
int session_is_live_mode(void);
int session_query(struct aos_session_status* out);
int session_login(const struct aos_session_request* request,
                  struct aos_session_status* out);
int session_autologin(struct aos_session_status* out);
int session_logout(struct aos_session_status* out);
int session_user_is_administrator(const char* username);
int session_verify_user_password(const char* username, const char* password);

#endif
