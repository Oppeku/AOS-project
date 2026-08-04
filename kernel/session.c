/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <credentials.h>
#include <process.h>
#include <session.h>
#include <syscall.h>
#include <timer.h>
#include <vfs.h>

#define SESSION_UID 1000U
#define SESSION_GID 1000U
#define LOGIN_UID 65534U
#define SESSION_CONFIG_MAX 4096U
#define SESSION_SHADOW_MAX 1024U

static uint32_t failed_attempts;
static uint64_t retry_after_tick;

extern void syscall_terminate_uid_processes(uint32_t uid, uint32_t except_pid);

static size_t text_length(const char* value) {
    size_t size = 0;
    while (value && value[size]) size++;
    return size;
}

static int text_equal(const char* left, const char* right) {
    size_t at = 0;
    if (!left || !right) return 0;
    while (left[at] && right[at] && left[at] == right[at]) at++;
    return left[at] == 0 && right[at] == 0;
}

static void copy_text(char* out, size_t out_size, const char* value) {
    size_t at = 0;
    if (!out || out_size == 0) return;
    if (!value) value = "";
    while (value[at] && at + 1U < out_size) {
        out[at] = value[at];
        at++;
    }
    out[at] = 0;
}

static void clear_bytes(void* data, size_t size) {
    uint8_t* bytes = (uint8_t*)data;
    while (size--) *bytes++ = 0;
}

static int read_text_file(const char* path, char* out, size_t out_size) {
    struct vfs_node node;
    uint64_t size;
    if (!path || !out || out_size < 2U) return -1;
    if (vfs_lookup(path, &node) != 0 || node.type != VFS_NODE_TYPE_REGULAR) return -1;
    size = node.size;
    if (size >= out_size) size = out_size - 1U;
    if (vfs_read_node(&node, 0, (uint8_t*)out, size) != 0) return -1;
    out[size] = 0;
    return 0;
}

static int config_value(const char* key, char* out, size_t out_size) {
    char config[SESSION_CONFIG_MAX];
    size_t key_size = text_length(key);
    size_t at = 0;

    if (!key || !key_size || !out || out_size == 0 ||
        read_text_file("etc/aos-system.conf", config, sizeof(config)) != 0) {
        return -1;
    }
    while (config[at]) {
        size_t line = at;
        size_t i = 0;
        while (i < key_size && config[line + i] == key[i]) i++;
        if (i == key_size && config[line + i] == '=') {
            size_t value = line + i + 1U;
            size_t used = 0;
            while (config[value] && config[value] != '\n' && config[value] != '\r') {
                if (used + 1U < out_size) out[used++] = config[value];
                value++;
            }
            out[used] = 0;
            clear_bytes(config, sizeof(config));
            return 0;
        }
        while (config[at] && config[at] != '\n') at++;
        if (config[at] == '\n') at++;
    }
    clear_bytes(config, sizeof(config));
    out[0] = 0;
    return -1;
}

static int installed_marker_present(void) {
    struct vfs_node node;
    return vfs_lookup("etc/aos-install.conf", &node) == 0;
}

static void configured_username(char out[AOS_SESSION_USERNAME_MAX]) {
    if (config_value("username", out, AOS_SESSION_USERNAME_MAX) != 0 || !out[0]) {
        copy_text(out, AOS_SESSION_USERNAME_MAX, "explorer");
    }
}

static int config_flag(const char* key) {
    char value[32];
    if (config_value(key, value, sizeof(value)) != 0) return 0;
    return text_equal(value, "yes") || text_equal(value, "true") ||
           text_equal(value, "administrator");
}

int session_is_live_mode(void) {
    return !installed_marker_present();
}

int session_user_is_administrator(const char* username) {
    char configured[AOS_SESSION_USERNAME_MAX];
    char account[32];
    if (!installed_marker_present() || !username) return 0;
    configured_username(configured);
    if (!text_equal(configured, username)) return 0;
    return config_value("account", account, sizeof(account)) == 0 &&
           text_equal(account, "administrator");
}

static int shadow_hash_for(const char* username, char* out, size_t out_size) {
    char shadow[SESSION_SHADOW_MAX];
    size_t at = 0;
    if (!username || !out || out_size == 0 ||
        read_text_file("etc/shadow", shadow, sizeof(shadow)) != 0) return -1;
    while (shadow[at]) {
        size_t name_start = at;
        size_t name_size;
        size_t hash_start;
        size_t hash_size;
        while (shadow[at] && shadow[at] != ':' && shadow[at] != '\n') at++;
        name_size = at - name_start;
        if (shadow[at] != ':') {
            while (shadow[at] && shadow[at] != '\n') at++;
            if (shadow[at]) at++;
            continue;
        }
        at++;
        hash_start = at;
        while (shadow[at] && shadow[at] != ':' && shadow[at] != '\n') at++;
        hash_size = at - hash_start;
        if (text_length(username) == name_size) {
            size_t i = 0;
            while (i < name_size && username[i] == shadow[name_start + i]) i++;
            if (i == name_size) {
                if (hash_size + 1U > out_size) {
                    clear_bytes(shadow, sizeof(shadow));
                    return -1;
                }
                for (i = 0; i < hash_size; i++) out[i] = shadow[hash_start + i];
                out[hash_size] = 0;
                clear_bytes(shadow, sizeof(shadow));
                return hash_size ? 0 : -1;
            }
        }
        while (shadow[at] && shadow[at] != '\n') at++;
        if (shadow[at]) at++;
    }
    clear_bytes(shadow, sizeof(shadow));
    return -1;
}

int session_verify_user_password(const char* username, const char* password) {
    char encoded[AOS_PASSWORD_ENCODED_MAX];
    int matches;
    if (!username || !password || shadow_hash_for(username, encoded, sizeof(encoded)) != 0) {
        return 0;
    }
    matches = credentials_verify_password(username, password, encoded);
    credentials_wipe(encoded, sizeof(encoded));
    return matches;
}

static uint32_t retry_seconds(void) {
    uint64_t now = timer_get_ticks();
    uint32_t frequency = timer_get_frequency();
    uint64_t ticks;
    if (now >= retry_after_tick) return 0;
    if (frequency == 0) frequency = 100;
    ticks = retry_after_tick - now;
    return (uint32_t)((ticks + frequency - 1U) / frequency);
}

static void fill_status(struct aos_session_status* out, const char* message) {
    char username[AOS_SESSION_USERNAME_MAX];
    int installed;
    if (!out) return;
    clear_bytes(out, sizeof(*out));
    installed = installed_marker_present();
    configured_username(username);
    out->version = AOS_SESSION_ABI_VERSION;
    out->installed = installed ? 1U : 0U;
    out->live_mode = installed ? 0U : 1U;
    out->uid = process_get_uid();
    out->gid = process_get_gid();
    out->autologin = installed && config_flag("autologin") ? 1U : 0U;
    out->administrator = installed &&
        session_user_is_administrator(username) ? 1U : 0U;
    out->authenticated = !installed ||
        (process_get_uid() == SESSION_UID &&
         text_equal(process_get_username(), username));
    out->login_required = installed && !out->authenticated;
    out->failed_attempts = failed_attempts;
    out->retry_after_seconds = retry_seconds();
    copy_text(out->username, sizeof(out->username), username);
    copy_text(out->home, sizeof(out->home), "/main");
    copy_text(out->message, sizeof(out->message), message ? message :
              (out->authenticated ? "Session ready" : "Enter your password"));
}

void session_prepare_boot_identity(void) {
    failed_attempts = 0;
    retry_after_tick = 0;
    if (installed_marker_present()) {
        process_set_identity(LOGIN_UID, LOGIN_UID, "login", "");
        process_set_cwd("");
    }
}

int session_query(struct aos_session_status* out) {
    if (!out) return -(int)LINUX_EFAULT;
    fill_status(out, NULL);
    return 0;
}

static int session_caller_is_manager(void) {
    process_t* process = get_current_process();
    return process && process->pid == 1U;
}

static int begin_user_session(struct aos_session_status* out,
                              const char* message) {
    char username[AOS_SESSION_USERNAME_MAX];
    configured_username(username);
    process_set_identity(SESSION_UID, SESSION_GID, username, "main");
    process_set_cwd("main");
    failed_attempts = 0;
    retry_after_tick = 0;
    fill_status(out, message);
    return 0;
}

int session_login(const struct aos_session_request* request,
                  struct aos_session_status* out) {
    char username[AOS_SESSION_USERNAME_MAX];
    uint32_t frequency;
    uint32_t delay_seconds;
    if (!request || !out) return -(int)LINUX_EFAULT;
    if (!installed_marker_present() || !session_caller_is_manager() ||
        process_get_uid() != LOGIN_UID) return -(int)LINUX_EPERM;
    if (request->version != AOS_SESSION_ABI_VERSION) return -(int)LINUX_EINVAL;
    if (retry_seconds()) {
        fill_status(out, "Please wait before trying again");
        return -(int)LINUX_EAGAIN;
    }
    configured_username(username);
    if (!text_equal(request->username, username) ||
        !session_verify_user_password(username, request->password)) {
        failed_attempts++;
        if (failed_attempts >= 3U) {
            frequency = timer_get_frequency();
            if (frequency == 0) frequency = 100;
            delay_seconds = failed_attempts - 1U;
            if (delay_seconds > 10U) delay_seconds = 10U;
            retry_after_tick = timer_get_ticks() +
                               (uint64_t)frequency * delay_seconds;
        }
        fill_status(out, "Password was not accepted");
        return -(int)LINUX_EACCES;
    }
    return begin_user_session(out, "Welcome to AOS");
}

int session_autologin(struct aos_session_status* out) {
    if (!out) return -(int)LINUX_EFAULT;
    if (!installed_marker_present() || !session_caller_is_manager() ||
        process_get_uid() != LOGIN_UID || !config_flag("autologin")) {
        fill_status(out, "Automatic login is disabled");
        return -(int)LINUX_EPERM;
    }
    return begin_user_session(out, "Automatic login complete");
}

int session_logout(struct aos_session_status* out) {
    process_t* current = get_current_process();
    uint32_t old_uid;
    if (!out) return -(int)LINUX_EFAULT;
    if (!installed_marker_present() || !session_caller_is_manager() ||
        process_get_uid() != SESSION_UID) return -(int)LINUX_EPERM;
    old_uid = process_get_uid();
    syscall_terminate_uid_processes(old_uid, current->pid);
    process_set_identity(LOGIN_UID, LOGIN_UID, "login", "");
    process_set_cwd("");
    failed_attempts = 0;
    retry_after_tick = 0;
    fill_status(out, "Signed out");
    return 0;
}
