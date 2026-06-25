/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_PROCESS_INFO 544

#define MAX_PROCESSES 64

#define PROCESS_STATUS_READY 1
#define PROCESS_STATUS_RUNNING 2
#define PROCESS_STATUS_WAITING 3
#define PROCESS_STATUS_ZOMBIE 4

struct aos_process_info_user {
    uint8_t valid;
    uint8_t status;
    uint16_t reserved;
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t uid;
    uint32_t euid;
    int32_t exit_status;
    uint64_t brk_current;
    uint64_t mmap_next;
    char username[32];
    char command[64];
    char cwd[256];
} __attribute__((packed));

static struct aos_process_info_user proc;
static char num_buf[21];

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static void exit_code(int code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void write_cstr(const char* s) {
    syscall3(SYS_WRITE, 1, (long)s, (long)cstrlen(s));
}

static void write_spaces(uint64_t count) {
    while (count--) write_cstr(" ");
}

static uint64_t decimal_len(uint64_t v) {
    uint64_t len = 1;
    while (v >= 10) {
        v /= 10;
        len++;
    }
    return len;
}

static void write_u64(uint64_t v) {
    char* p = &num_buf[20];
    *p = 0;
    if (v == 0) {
        *--p = '0';
    } else {
        while (v) {
            *--p = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    write_cstr(p);
}

static void write_u64_width(uint64_t v, uint64_t width) {
    uint64_t len = decimal_len(v);
    if (len < width) write_spaces(width - len);
    write_u64(v);
}

static const char* status_name(uint8_t status) {
    switch (status) {
        case PROCESS_STATUS_READY:
            return "ready";
        case PROCESS_STATUS_RUNNING:
            return "run";
        case PROCESS_STATUS_WAITING:
            return "wait";
        case PROCESS_STATUS_ZOMBIE:
            return "zombie";
        default:
            return "unknown";
    }
}

static const char* fallback_empty(const char* s, const char* fallback) {
    return (s && s[0]) ? s : fallback;
}

void aos_main(void) {
    uint64_t shown = 0;

    write_cstr(" PID  PPID  UID  EUID  STATE   USER      COMMAND\n");

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        long rc = syscall3(AOS_SYS_PROCESS_INFO, (long)i, (long)&proc, 0);
        if (rc < 0) continue;
        if (!proc.valid) continue;

        write_u64_width(proc.pid, 4);
        write_cstr(" ");
        write_u64_width(proc.parent_pid, 5);
        write_cstr(" ");
        write_u64_width(proc.uid, 4);
        write_cstr(" ");
        write_u64_width(proc.euid, 5);
        write_cstr("  ");
        write_cstr(status_name(proc.status));
        write_spaces(proc.status == PROCESS_STATUS_ZOMBIE ? 2 : 7 - cstrlen(status_name(proc.status)));
        write_cstr(" ");
        write_cstr(fallback_empty(proc.username, "-"));
        write_cstr("      ");
        write_cstr(fallback_empty(proc.command, "-"));
        if (proc.cwd[0]) {
            write_cstr(" cwd=/");
            write_cstr(proc.cwd);
        }
        write_cstr("\n");
        shown++;
    }

    if (shown == 0) {
        write_cstr("ps: no process records visible\n");
    }
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile("call aos_main\n");
}
