/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_OPENAT 257
#define SYS_EXIT 60

#define AT_FDCWD -100
#define O_RDONLY 0

static char io_buf[1024];

static long syscall1(long n, long a) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall4(long n, long a, long b, long c, long d) {
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
                     : "rcx", "r11", "memory");
    return ret;
}

static void exit_code(int code) {
    syscall1(SYS_EXIT, code);
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

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

static int parse_u64(const char* s, uint64_t* out) {
    uint64_t v = 0;
    if (!s || !s[0]) return -1;
    for (uint64_t i = 0; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (uint64_t)(s[i] - '0');
    }
    *out = v;
    return 0;
}

static int write_all(const char* buf, uint64_t len) {
    uint64_t written = 0;
    while (written < len) {
        long rc = syscall3(SYS_WRITE, 1, (long)(buf + written), (long)(len - written));
        if (rc <= 0) return -1;
        written += (uint64_t)rc;
    }
    return 0;
}

static int head_fd(int fd, uint64_t lines) {
    uint64_t seen = 0;

    if (lines == 0) return 0;
    for (;;) {
        long rc = syscall3(SYS_READ, fd, (long)io_buf, sizeof(io_buf));
        if (rc < 0) {
            write_cstr("head: read failed\n");
            return -1;
        }
        if (rc == 0) return 0;

        uint64_t take = 0;
        while (take < (uint64_t)rc) {
            if (io_buf[take] == '\n') {
                seen++;
                take++;
                if (seen >= lines) break;
                continue;
            }
            take++;
        }

        if (write_all(io_buf, take) != 0) {
            write_cstr("head: write failed\n");
            return -1;
        }
        if (seen >= lines) return 0;
    }
}

static int head_file(const char* path, uint64_t lines) {
    long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
    int rc;

    if (fd < 0) {
        write_cstr("head: open failed: ");
        write_cstr(path);
        write_cstr("\n");
        return -1;
    }

    rc = head_fd((int)fd, lines);
    syscall1(SYS_CLOSE, fd);
    return rc;
}

void aos_main(uint64_t argc, char** argv) {
    uint64_t lines = 10;
    uint64_t first_path = 1;
    int rc = 0;

    if (argc > 1 && streq(argv[1], "--help")) {
        write_cstr("usage: head [-n N] [FILE...]\n");
        exit_code(0);
    }

    if (argc > 2 && streq(argv[1], "-n")) {
        if (parse_u64(argv[2], &lines) != 0) {
            write_cstr("head: invalid line count\n");
            exit_code(1);
        }
        first_path = 3;
    } else if (argc > 1 && argv[1][0] == '-' && argv[1][1] >= '0' && argv[1][1] <= '9') {
        if (parse_u64(argv[1] + 1, &lines) != 0) {
            write_cstr("head: invalid line count\n");
            exit_code(1);
        }
        first_path = 2;
    }

    if (first_path >= argc) {
        exit_code(head_fd(0, lines) == 0 ? 0 : 1);
    }

    for (uint64_t i = first_path; i < argc; i++) {
        if (head_file(argv[i], lines) != 0) rc = 1;
    }
    exit_code(rc);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
