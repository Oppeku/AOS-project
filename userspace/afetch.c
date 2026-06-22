/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_UNAME 63
#define SYS_EXIT 60
#define SYS_OPENAT 257
#define AOS_SYS_MEM_INFO 509
#define AOS_SYS_UPTIME_INFO 510

#define AT_FDCWD -100
#define O_RDONLY 0

#define UTS_FIELD_SIZE 65
#define UTS_SYSNAME_OFF 0
#define UTS_RELEASE_OFF 130
#define UTS_MACHINE_OFF 260
#define UTS_SIZE 390

#define MEM_TOTAL_OFF 0
#define MEM_FREE_OFF 8
#define MEM_USED_OFF 16

#define UPTIME_TICKS_OFF 0
#define UPTIME_SECONDS_OFF 8

static char uts_buf[UTS_SIZE];
static uint8_t mem_info[24];
static uint8_t uptime_info[24];
static char read_buf[512];
static char num_buf[21];

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

static uint64_t load_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (uint64_t i = 0; i < 8; i++) {
        v |= ((uint64_t)p[i]) << (i * 8);
    }
    return v;
}

static void write_two_digits(uint64_t v) {
    char buf[3];
    if (v > 99) {
        write_u64(v);
        return;
    }
    buf[0] = (char)('0' + (v / 10));
    buf[1] = (char)('0' + (v % 10));
    buf[2] = 0;
    write_cstr(buf);
}

static void write_uptime(uint64_t seconds) {
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds / 60) % 60;
    uint64_t secs = seconds % 60;

    write_two_digits(hours);
    write_cstr(":");
    write_two_digits(minutes);
    write_cstr(":");
    write_two_digits(secs);
}

static void write_mib(uint64_t bytes) {
    write_u64((bytes + 1048575ULL) / 1048576ULL);
    write_cstr(" MiB");
}

static uint64_t count_installed_packages(void) {
    int fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"/tmp/acur-installed.txt", O_RDONLY, 0);
    uint64_t count = 0;
    int saw_text = 0;

    if (fd < 0) return 0;
    for (;;) {
        long rc = syscall3(SYS_READ, fd, (long)read_buf, sizeof(read_buf));
        if (rc <= 0) break;
        for (long i = 0; i < rc; i++) {
            if (read_buf[i] == '\n') {
                count++;
                saw_text = 0;
            } else if (read_buf[i] != '\r' && read_buf[i] != ' ' && read_buf[i] != '\t') {
                saw_text = 1;
            }
        }
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    if (saw_text) count++;
    return count;
}

static void print_line(const char* logo, const char* key) {
    write_cstr(logo);
    write_cstr("  ");
    write_cstr(key);
}

void aos_main(void) {
    uint64_t total;
    uint64_t used;
    uint64_t seconds;
    uint64_t package_count;

    if (syscall3(SYS_UNAME, (long)uts_buf, 0, 0) < 0) {
        uts_buf[UTS_SYSNAME_OFF] = 'A';
        uts_buf[UTS_SYSNAME_OFF + 1] = 'O';
        uts_buf[UTS_SYSNAME_OFF + 2] = 'S';
        uts_buf[UTS_SYSNAME_OFF + 3] = 0;
        uts_buf[UTS_RELEASE_OFF] = '0';
        uts_buf[UTS_RELEASE_OFF + 1] = '.';
        uts_buf[UTS_RELEASE_OFF + 2] = '1';
        uts_buf[UTS_RELEASE_OFF + 3] = '-';
        uts_buf[UTS_RELEASE_OFF + 4] = 'd';
        uts_buf[UTS_RELEASE_OFF + 5] = 'e';
        uts_buf[UTS_RELEASE_OFF + 6] = 'v';
        uts_buf[UTS_RELEASE_OFF + 7] = 0;
        uts_buf[UTS_MACHINE_OFF] = 'x';
        uts_buf[UTS_MACHINE_OFF + 1] = '8';
        uts_buf[UTS_MACHINE_OFF + 2] = '6';
        uts_buf[UTS_MACHINE_OFF + 3] = '_';
        uts_buf[UTS_MACHINE_OFF + 4] = '6';
        uts_buf[UTS_MACHINE_OFF + 5] = '4';
        uts_buf[UTS_MACHINE_OFF + 6] = 0;
    }
    if (syscall3(AOS_SYS_MEM_INFO, (long)mem_info, 0, 0) < 0) {
        for (uint64_t i = 0; i < sizeof(mem_info); i++) mem_info[i] = 0;
    }
    if (syscall3(AOS_SYS_UPTIME_INFO, (long)uptime_info, 0, 0) < 0) {
        for (uint64_t i = 0; i < sizeof(uptime_info); i++) uptime_info[i] = 0;
    }

    total = load_u64(mem_info + MEM_TOTAL_OFF);
    used = load_u64(mem_info + MEM_USED_OFF);
    seconds = load_u64(uptime_info + UPTIME_SECONDS_OFF);
    package_count = count_installed_packages();

    write_cstr("\n");
    print_line("       /\\       ", "OS: ");
    write_cstr(uts_buf + UTS_SYSNAME_OFF);
    write_cstr("\n");

    print_line("      /  \\      ", "Kernel: ");
    write_cstr(uts_buf + UTS_RELEASE_OFF);
    write_cstr("\n");

    print_line("     / /\\ \\     ", "Uptime: ");
    write_uptime(seconds);
    write_cstr("\n");

    print_line("    / ____ \\    ", "CPU: ");
    write_cstr(uts_buf + UTS_MACHINE_OFF);
    write_cstr("\n");

    print_line("   /_/    \\_\\   ", "Memory: ");
    write_mib(used);
    write_cstr(" / ");
    write_mib(total);
    write_cstr("\n");

    print_line("                ", "Shell: AOS Interactive Shell");
    write_cstr("\n");

    print_line("                ", "Packages: ");
    write_u64(package_count);
    write_cstr(" live");
    write_cstr("\n\n");

    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile("call aos_main\n");
}
