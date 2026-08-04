/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define SYS_NEWFSTATAT 262

#define AT_FDCWD -100

#define S_IFMT 0170000U
#define S_IFIFO 0010000U
#define S_IFCHR 0020000U
#define S_IFDIR 0040000U
#define S_IFREG 0100000U

struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t __unused[3];
};

static char num_buf[32];

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

static void write_u64(uint64_t v) {
    char* p = &num_buf[31];
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

static void write_octal(uint64_t v) {
    char* p = &num_buf[31];
    *p = 0;
    if (v == 0) {
        *--p = '0';
    } else {
        while (v) {
            *--p = (char)('0' + (v & 7));
            v >>= 3;
        }
    }
    write_cstr(p);
}

static const char* type_name(uint32_t mode) {
    switch (mode & S_IFMT) {
        case S_IFREG:
            return "regular";
        case S_IFDIR:
            return "directory";
        case S_IFCHR:
            return "character";
        case S_IFIFO:
            return "fifo";
        default:
            return "unknown";
    }
}

void aos_main(uint64_t argc, char** argv) {
    struct linux_stat st;

    if (argc != 2) {
        write_cstr("usage: stat FILE\n");
        exit_code(1);
    }

    if (syscall4(SYS_NEWFSTATAT, AT_FDCWD, (long)argv[1], (long)&st, 0) < 0) {
        write_cstr("stat: failed\n");
        exit_code(1);
    }

    write_cstr("File: ");
    write_cstr(argv[1]);
    write_cstr("\nType: ");
    write_cstr(type_name(st.st_mode));
    write_cstr("\nSize: ");
    write_u64((uint64_t)st.st_size);
    write_cstr("\nMode: 0");
    write_octal(st.st_mode);
    write_cstr("\nInode: ");
    write_u64(st.st_ino);
    write_cstr("\n");
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
