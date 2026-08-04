/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_NANOSLEEP 35
#define SYS_FORK 57
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61
#define AOS_SYS_GFX_INFO 520

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct aos_gfx_info {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t ready;
};

static long syscall1(long n, long a) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long n, long a, long b) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b)
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

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void write_buf(const char* s, uint64_t n) {
    uint64_t off = 0;
    while (off < n) {
        long rc = syscall3(SYS_WRITE, 1, (long)(s + off), (long)(n - off));
        if (rc <= 0) return;
        off += (uint64_t)rc;
    }
}

static void write_cstr(const char* s) {
    write_buf(s, cstrlen(s));
}

static void exit_code(int code) {
    syscall1(SYS_EXIT, code);
    for (;;) {}
}

static void sleep_ms(uint64_t ms) {
    struct timespec ts;
    ts.tv_sec = (int64_t)(ms / 1000);
    ts.tv_nsec = (int64_t)((ms % 1000) * 1000000ULL);
    syscall2(SYS_NANOSLEEP, (long)&ts, 0);
}

static long exec_first_available(const char* const* paths, const char* const* argv) {
    for (uint64_t i = 0; paths[i]; i++) {
        long rc = syscall3(SYS_EXECVE, (long)paths[i], (long)argv, 0);
        if (rc >= 0) return rc;
    }
    return -1;
}

static void exec_shell(void) {
    static const char* const shell_paths[] = { "/shell.elf", "/shell", "shell.elf", 0 };
    static const char* const shell_argv[] = { "shell.elf", 0 };

    write_cstr("AOS Desktop Manager: starting shell fallback\n");
    exec_first_available(shell_paths, shell_argv);
    write_cstr("AOS Desktop Manager: shell fallback failed\n");
    exit_code(1);
}

static int framebuffer_ready(void) {
    struct aos_gfx_info gfx;
    for (uint32_t i = 0; i < 20; i++) {
        if (syscall3(AOS_SYS_GFX_INFO, (long)&gfx, 0, 0) >= 0 && gfx.ready) {
            return 1;
        }
        sleep_ms(50);
    }
    return 0;
}

void aos_main(void) {
    static const char* const mui_paths[] = { "/mui", "/MUI", "/mui.elf", "mui.elf", 0 };
    static const char* const mui_argv[] = { "mui", 0 };

    write_cstr("AOS Desktop Manager: starting MUI session\n");

    if (!framebuffer_ready()) {
        write_cstr("AOS Desktop Manager: framebuffer unavailable\n");
        exec_shell();
    }

    exec_first_available(mui_paths, mui_argv);
    write_cstr("AOS Desktop Manager: MUI launch failed\n");
    exec_shell();
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile("call aos_main\n");
}
