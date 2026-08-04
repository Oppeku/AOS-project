/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_OPENAT 257
#define SYS_UNLINKAT 263
#define SYS_EXIT 60

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512

static char copy_buf[1024];

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

static int write_all(int fd, const char* buf, uint64_t len) {
    uint64_t written = 0;
    while (written < len) {
        long rc = syscall3(SYS_WRITE, fd, (long)(buf + written), (long)(len - written));
        if (rc <= 0) return -1;
        written += (uint64_t)rc;
    }
    return 0;
}

static int copy_file(const char* src, const char* dst) {
    long in_fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)src, O_RDONLY, 0);
    long out_fd;

    if (in_fd < 0) {
        write_cstr("mv: open source failed\n");
        return -1;
    }

    out_fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        syscall1(SYS_CLOSE, in_fd);
        write_cstr("mv: open destination failed\n");
        return -1;
    }

    for (;;) {
        long rc = syscall3(SYS_READ, in_fd, (long)copy_buf, sizeof(copy_buf));
        if (rc < 0) {
            write_cstr("mv: read failed\n");
            syscall1(SYS_CLOSE, in_fd);
            syscall1(SYS_CLOSE, out_fd);
            return -1;
        }
        if (rc == 0) break;
        if (write_all((int)out_fd, copy_buf, (uint64_t)rc) != 0) {
            write_cstr("mv: write failed\n");
            syscall1(SYS_CLOSE, in_fd);
            syscall1(SYS_CLOSE, out_fd);
            return -1;
        }
    }

    syscall1(SYS_CLOSE, in_fd);
    syscall1(SYS_CLOSE, out_fd);
    return 0;
}

void aos_main(uint64_t argc, char** argv) {
    if (argc != 3) {
        write_cstr("usage: mv SRC DEST\n");
        exit_code(1);
    }
    if (copy_file(argv[1], argv[2]) != 0) {
        exit_code(1);
    }
    if (syscall3(SYS_UNLINKAT, AT_FDCWD, (long)argv[1], 0) < 0) {
        write_cstr("mv: unlink source failed\n");
        exit_code(1);
    }
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
