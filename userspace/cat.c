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

static int write_all(const char* buf, uint64_t len) {
    uint64_t written = 0;
    while (written < len) {
        long rc = syscall3(SYS_WRITE, 1, (long)(buf + written), (long)(len - written));
        if (rc <= 0) return -1;
        written += (uint64_t)rc;
    }
    return 0;
}

static int cat_fd(int fd) {
    for (;;) {
        long rc = syscall3(SYS_READ, fd, (long)io_buf, sizeof(io_buf));
        if (rc < 0) {
            write_cstr("cat: read failed\n");
            return -1;
        }
        if (rc == 0) return 0;
        if (write_all(io_buf, (uint64_t)rc) != 0) {
            write_cstr("cat: write failed\n");
            return -1;
        }
    }
}

static int cat_file(const char* path) {
    long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
    int rc;

    if (fd < 0) {
        write_cstr("cat: open failed: ");
        write_cstr(path);
        write_cstr("\n");
        return -1;
    }

    rc = cat_fd((int)fd);
    syscall1(SYS_CLOSE, fd);
    return rc;
}

void aos_main(uint64_t argc, char** argv) {
    int rc = 0;

    if (argc > 1 && streq(argv[1], "--help")) {
        write_cstr("usage: cat [FILE...]\n");
        exit_code(0);
    }

    if (argc == 1) {
        exit_code(cat_fd(0) == 0 ? 0 : 1);
    }

    for (uint64_t i = 1; i < argc; i++) {
        if (cat_file(argv[i]) != 0) rc = 1;
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
