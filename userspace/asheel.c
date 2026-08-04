/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_LSEEK 8
#define SYS_NANOSLEEP 35
#define SYS_UNAME 63
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_GETCWD 79
#define SYS_GETUID 102
#define SYS_GETGID 104
#define SYS_GETEUID 107
#define SYS_GETEGID 108
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258
#define SYS_NEWFSTATAT 262
#define SYS_UNLINKAT 263
#define SYS_GETDENTS64 217
#define AOS_SYS_MEM_INFO 509
#define AOS_SYS_UPTIME_INFO 510
#define AOS_SYS_MOUNT_INFO 506
#define AOS_SYS_TIME_INFO 515
#define AOS_SYS_USER_INFO 516
#define AOS_SYS_PROCESS_KILL 546
#define AOS_SYS_PROCESS_PAUSE 547
#define AOS_SYS_PROCESS_RESUME 548

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512
#define O_DIRECTORY 0x10000
#define AT_REMOVEDIR 0x200
#define SEEK_SET 0
#define DT_DIR 4
#define UTS_FIELD_SIZE 65

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

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct mount_info {
    uint8_t backend;
    uint8_t reserved[7];
    char path[64];
    char backend_root[64];
};

struct time_info {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
};

struct mem_info {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

struct uptime_info {
    uint64_t ticks;
    uint64_t seconds;
    uint32_t frequency;
    uint32_t reserved;
};

struct user_info {
    uint32_t uid;
    uint32_t gid;
    uint32_t euid;
    uint32_t egid;
    char username[32];
    char home[256];
};

struct utsname_user {
    char sysname[UTS_FIELD_SIZE];
    char nodename[UTS_FIELD_SIZE];
    char release[UTS_FIELD_SIZE];
    char version[UTS_FIELD_SIZE];
    char machine[UTS_FIELD_SIZE];
    char domainname[UTS_FIELD_SIZE];
};

static char io_buf[8192];
static char line_buf[2048];
static char path_buf[512];
static char path_buf2[512];
static char num_buf[32];

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

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

static int starts_with(const char* s, const char* prefix) {
    uint64_t i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static void write_buf(const char* s, uint64_t n) {
    uint64_t off = 0;
    while (off < n) {
        long rc = syscall3(SYS_WRITE, 1, (long)(s + off), (long)(n - off));
        if (rc <= 0) return;
        off += (uint64_t)rc;
    }
}

static int write_all_fd(int fd, const char* s, uint64_t n) {
    uint64_t off = 0;
    while (off < n) {
        long rc = syscall3(SYS_WRITE, fd, (long)(s + off), (long)(n - off));
        if (rc <= 0) return -1;
        off += (uint64_t)rc;
    }
    return 0;
}

static void write_cstr(const char* s) {
    write_buf(s, cstrlen(s));
}

static void write_char(char c) {
    write_buf(&c, 1);
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

static void write_two_digits(uint64_t v) {
    write_char((char)('0' + ((v / 10) % 10)));
    write_char((char)('0' + (v % 10)));
}

static int parse_i64(const char* s, int64_t* out) {
    int neg = 0;
    int64_t v = 0;
    uint64_t i = 0;
    if (!s || !s[0]) return -1;
    if (s[0] == '-') {
        neg = 1;
        i = 1;
        if (!s[i]) return -1;
    }
    for (; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    *out = neg ? -v : v;
    return 0;
}

static const char* basename(const char* s) {
    const char* b = s;
    for (uint64_t i = 0; s && s[i]; i++) {
        if (s[i] == '/') b = s + i + 1;
    }
    return b;
}

static int path_join(char* out, uint64_t out_size, const char* a, const char* b) {
    uint64_t n = 0;
    if (!out || out_size == 0) return -1;
    if (!a || !a[0]) a = ".";
    for (uint64_t i = 0; a[i]; i++) {
        if (n + 1 >= out_size) return -1;
        out[n++] = a[i];
    }
    if (n > 0 && out[n - 1] != '/') {
        if (n + 1 >= out_size) return -1;
        out[n++] = '/';
    }
    for (uint64_t i = 0; b && b[i]; i++) {
        if (n + 1 >= out_size) return -1;
        out[n++] = b[i];
    }
    out[n] = 0;
    return 0;
}

static int stat_path(const char* path, struct linux_stat* st) {
    return (int)syscall4(SYS_NEWFSTATAT, AT_FDCWD, (long)path, (long)st, 0);
}

static int is_dir_path(const char* path) {
    struct linux_stat st;
    if (stat_path(path, &st) < 0) return 0;
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

static int read_line_fd(int fd, char* out, uint64_t cap) {
    uint64_t n = 0;
    while (n + 1 < cap) {
        char c;
        long rc = syscall3(SYS_READ, fd, (long)&c, 1);
        if (rc < 0) return -1;
        if (rc == 0) break;
        out[n++] = c;
        if (c == '\n') break;
    }
    out[n] = 0;
    return n == 0 ? 0 : (int)n;
}

static int open_read(const char* path) {
    return (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
}

static int cat_fd(int fd) {
    for (;;) {
        long rc = syscall3(SYS_READ, fd, (long)io_buf, sizeof(io_buf));
        if (rc < 0) {
            write_cstr("cat: read failed\n");
            return 1;
        }
        if (rc == 0) return 0;
        write_buf(io_buf, (uint64_t)rc);
    }
}

static int cmd_cat(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc == 1) return cat_fd(0);
    for (uint64_t i = 1; i < argc; i++) {
        int fd = open_read(argv[i]);
        if (fd < 0) {
            write_cstr("cat: open failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
            continue;
        }
        if (cat_fd(fd) != 0) rc = 1;
        syscall1(SYS_CLOSE, fd);
    }
    return rc;
}

static int cmd_echo(uint64_t argc, char** argv) {
    uint64_t i = 1;
    int newline = 1;
    if (argc > 1 && streq(argv[1], "-n")) {
        newline = 0;
        i = 2;
    }
    for (; i < argc; i++) {
        if (i > (newline ? 1 : 2)) write_char(' ');
        write_cstr(argv[i]);
    }
    if (newline) write_char('\n');
    return 0;
}

static int cmd_pwd(uint64_t argc, char** argv) {
    (void)argc;
    (void)argv;
    long rc = syscall2(SYS_GETCWD, (long)path_buf, sizeof(path_buf));
    if (rc < 0) {
        write_cstr("pwd: getcwd failed\n");
        return 1;
    }
    write_cstr(path_buf);
    write_char('\n');
    return 0;
}

static int ls_one(const char* path) {
    struct linux_stat st;
    int fd;
    if (stat_path(path, &st) < 0) {
        write_cstr("ls: cannot access: ");
        write_cstr(path);
        write_char('\n');
        return 1;
    }
    if ((st.st_mode & S_IFMT) != S_IFDIR) {
        write_cstr(path);
        write_char('\n');
        return 0;
    }
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        write_cstr("ls: open failed: ");
        write_cstr(path);
        write_char('\n');
        return 1;
    }
    for (;;) {
        long got = syscall3(SYS_GETDENTS64, fd, (long)io_buf, sizeof(io_buf));
        if (got < 0) {
            syscall1(SYS_CLOSE, fd);
            write_cstr("ls: read directory failed\n");
            return 1;
        }
        if (got == 0) break;
        uint64_t off = 0;
        while (off < (uint64_t)got) {
            struct linux_dirent64* ent = (struct linux_dirent64*)(io_buf + off);
            if (ent->d_reclen == 0) break;
            if (!streq(ent->d_name, ".") && !streq(ent->d_name, "..")) {
                write_cstr(ent->d_name);
                write_char('\n');
            }
            off += ent->d_reclen;
        }
    }
    syscall1(SYS_CLOSE, fd);
    return 0;
}

static int cmd_ls(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc == 1) return ls_one(".");
    for (uint64_t i = 1; i < argc; i++) {
        if (argc > 2) {
            write_cstr(argv[i]);
            write_cstr(":\n");
        }
        if (ls_one(argv[i]) != 0) rc = 1;
        if (argc > 2 && i + 1 < argc) write_char('\n');
    }
    return rc;
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

static int cmd_stat(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc < 2) {
        write_cstr("usage: stat FILE...\n");
        return 1;
    }
    for (uint64_t i = 1; i < argc; i++) {
        struct linux_stat st;
        if (stat_path(argv[i], &st) < 0) {
            write_cstr("stat: failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
            continue;
        }
        write_cstr("File: ");
        write_cstr(argv[i]);
        write_cstr("\nType: ");
        write_cstr(type_name(st.st_mode));
        write_cstr("\nSize: ");
        write_u64((uint64_t)st.st_size);
        write_cstr("\nMode: 0");
        write_octal(st.st_mode);
        write_cstr("\nInode: ");
        write_u64(st.st_ino);
        write_char('\n');
        if (i + 1 < argc) write_char('\n');
    }
    return rc;
}

static uint64_t parse_line_count_arg(uint64_t argc, char** argv, uint64_t* first_path, const char* name) {
    int64_t lines = 10;
    *first_path = 1;
    if (argc > 2 && streq(argv[1], "-n")) {
        if (parse_i64(argv[2], &lines) != 0 || lines < 0) {
            write_cstr(name);
            write_cstr(": invalid line count\n");
            return (uint64_t)-1;
        }
        *first_path = 3;
    } else if (argc > 1 && argv[1][0] == '-' && argv[1][1] >= '0' && argv[1][1] <= '9') {
        if (parse_i64(argv[1] + 1, &lines) != 0 || lines < 0) {
            write_cstr(name);
            write_cstr(": invalid line count\n");
            return (uint64_t)-1;
        }
        *first_path = 2;
    }
    return (uint64_t)lines;
}

static int head_fd(int fd, uint64_t lines) {
    uint64_t seen = 0;
    if (lines == 0) return 0;
    for (;;) {
        long rc = syscall3(SYS_READ, fd, (long)io_buf, sizeof(io_buf));
        if (rc < 0) {
            write_cstr("head: read failed\n");
            return 1;
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
        write_buf(io_buf, take);
        if (seen >= lines) return 0;
    }
}

static int cmd_head(uint64_t argc, char** argv) {
    uint64_t first_path;
    uint64_t lines = parse_line_count_arg(argc, argv, &first_path, "head");
    int rc = 0;
    if (lines == (uint64_t)-1) return 1;
    if (first_path >= argc) return head_fd(0, lines);
    for (uint64_t i = first_path; i < argc; i++) {
        int fd = open_read(argv[i]);
        if (fd < 0) {
            write_cstr("head: open failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
            continue;
        }
        if (head_fd(fd, lines) != 0) rc = 1;
        syscall1(SYS_CLOSE, fd);
    }
    return rc;
}

static int tail_fd(int fd, uint64_t lines) {
    uint64_t len = 0;
    uint64_t end;
    uint64_t start;
    uint64_t found = 0;
    for (;;) {
        long rc = syscall3(SYS_READ, fd, (long)(io_buf + len), sizeof(io_buf) - len);
        if (rc < 0) {
            write_cstr("tail: read failed\n");
            return 1;
        }
        if (rc == 0) break;
        len += (uint64_t)rc;
        if (len == sizeof(io_buf)) {
            write_cstr("tail: input truncated to last 8 KiB buffer\n");
            break;
        }
    }
    if (lines == 0 || len == 0) return 0;
    end = len;
    if (end > 0 && io_buf[end - 1] == '\n') end--;
    start = end;
    while (start > 0 && found < lines) {
        start--;
        if (io_buf[start] == '\n') found++;
    }
    if (found >= lines && io_buf[start] == '\n') start++;
    write_buf(io_buf + start, len - start);
    return 0;
}

static int cmd_tail(uint64_t argc, char** argv) {
    uint64_t first_path;
    uint64_t lines = parse_line_count_arg(argc, argv, &first_path, "tail");
    int rc = 0;
    if (lines == (uint64_t)-1) return 1;
    if (first_path >= argc) return tail_fd(0, lines);
    for (uint64_t i = first_path; i < argc; i++) {
        int fd = open_read(argv[i]);
        if (fd < 0) {
            write_cstr("tail: open failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
            continue;
        }
        if (tail_fd(fd, lines) != 0) rc = 1;
        syscall1(SYS_CLOSE, fd);
    }
    return rc;
}

static int cmd_date(uint64_t argc, char** argv) {
    static const char* weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    struct time_info ti;
    (void)argc;
    (void)argv;
    if (syscall1(AOS_SYS_TIME_INFO, (long)&ti) < 0) {
        write_cstr("date: failed to read RTC time\n");
        return 1;
    }
    write_cstr(ti.weekday >= 1 && ti.weekday <= 7 ? weekdays[ti.weekday - 1] : "???");
    write_char(' ');
    write_cstr(ti.month >= 1 && ti.month <= 12 ? months[ti.month - 1] : "???");
    write_char(' ');
    write_two_digits(ti.day);
    write_char(' ');
    write_two_digits(ti.hour);
    write_char(':');
    write_two_digits(ti.minute);
    write_char(':');
    write_two_digits(ti.second);
    write_char(' ');
    write_u64(ti.year);
    write_char('\n');
    return 0;
}

static int cmd_touch(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc < 2) {
        write_cstr("usage: touch FILE...\n");
        return 1;
    }
    for (uint64_t i = 1; i < argc; i++) {
        long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)argv[i], O_WRONLY | O_CREAT, 0);
        if (fd < 0) {
            write_cstr("touch: failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
            continue;
        }
        syscall1(SYS_CLOSE, fd);
    }
    return rc;
}

static int cmd_rm(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc < 2) {
        write_cstr("usage: rm FILE...\n");
        return 1;
    }
    for (uint64_t i = 1; i < argc; i++) {
        if (syscall3(SYS_UNLINKAT, AT_FDCWD, (long)argv[i], 0) < 0) {
            write_cstr("rm: failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
        }
    }
    return rc;
}

static int cmd_rmdir(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc < 2) {
        write_cstr("usage: rmdir DIRECTORY...\n");
        return 1;
    }
    for (uint64_t i = 1; i < argc; i++) {
        if (syscall3(SYS_UNLINKAT, AT_FDCWD, (long)argv[i], AT_REMOVEDIR) < 0) {
            write_cstr("rmdir: failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
        }
    }
    return rc;
}

static int cmd_mkdir(uint64_t argc, char** argv) {
    int rc = 0;
    if (argc < 2) {
        write_cstr("usage: mkdir DIRECTORY...\n");
        return 1;
    }
    for (uint64_t i = 1; i < argc; i++) {
        if (syscall3(SYS_MKDIRAT, AT_FDCWD, (long)argv[i], 0755) < 0) {
            write_cstr("mkdir: failed: ");
            write_cstr(argv[i]);
            write_char('\n');
            rc = 1;
        }
    }
    return rc;
}

static int copy_file_asheel(const char* src, const char* dst, const char* name) {
    long in_fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)src, O_RDONLY, 0);
    long out_fd;
    if (in_fd < 0) {
        write_cstr(name);
        write_cstr(": open source failed: ");
        write_cstr(src);
        write_char('\n');
        return -1;
    }
    out_fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        syscall1(SYS_CLOSE, in_fd);
        write_cstr(name);
        write_cstr(": open destination failed: ");
        write_cstr(dst);
        write_char('\n');
        return -1;
    }
    for (;;) {
        long got = syscall3(SYS_READ, in_fd, (long)io_buf, sizeof(io_buf));
        if (got < 0) {
            write_cstr(name);
            write_cstr(": read failed\n");
            syscall1(SYS_CLOSE, in_fd);
            syscall1(SYS_CLOSE, out_fd);
            return -1;
        }
        if (got == 0) break;
        if (write_all_fd((int)out_fd, io_buf, (uint64_t)got) != 0) {
            write_cstr(name);
            write_cstr(": write failed\n");
            syscall1(SYS_CLOSE, in_fd);
            syscall1(SYS_CLOSE, out_fd);
            return -1;
        }
    }
    syscall1(SYS_CLOSE, in_fd);
    syscall1(SYS_CLOSE, out_fd);
    return 0;
}

static int cmd_cp(uint64_t argc, char** argv) {
    if (argc != 3) {
        write_cstr("usage: cp SRC DEST\n");
        return 1;
    }
    return copy_file_asheel(argv[1], argv[2], "cp") == 0 ? 0 : 1;
}

static int cmd_mv(uint64_t argc, char** argv) {
    if (argc != 3) {
        write_cstr("usage: mv SRC DEST\n");
        return 1;
    }
    if (copy_file_asheel(argv[1], argv[2], "mv") != 0) return 1;
    if (syscall3(SYS_UNLINKAT, AT_FDCWD, (long)argv[1], 0) < 0) {
        write_cstr("mv: unlink source failed: ");
        write_cstr(argv[1]);
        write_char('\n');
        return 1;
    }
    return 0;
}

static int cmd_uname(uint64_t argc, char** argv) {
    struct utsname_user u;
    int all = 0;
    if (argc > 1) {
        if (streq(argv[1], "-a") || streq(argv[1], "--all")) all = 1;
        else {
            write_cstr("usage: uname [-a]\n");
            return 1;
        }
    }
    if (syscall1(SYS_UNAME, (long)&u) < 0) {
        write_cstr("uname: failed to read system name\n");
        return 1;
    }
    write_cstr(u.sysname);
    if (all) {
        write_char(' ');
        write_cstr(u.nodename);
        write_char(' ');
        write_cstr(u.release);
        write_char(' ');
        write_cstr(u.version);
        write_char(' ');
        write_cstr(u.machine);
        write_char(' ');
        write_cstr(u.domainname);
    }
    write_char('\n');
    return 0;
}

static int read_user_info(struct user_info* ui) {
    if (syscall1(AOS_SYS_USER_INFO, (long)ui) == 0) return 0;
    ui->uid = (uint32_t)syscall1(SYS_GETUID, 0);
    ui->gid = (uint32_t)syscall1(SYS_GETGID, 0);
    ui->euid = (uint32_t)syscall1(SYS_GETEUID, 0);
    ui->egid = (uint32_t)syscall1(SYS_GETEGID, 0);
    ui->username[0] = 0;
    ui->home[0] = 0;
    return -1;
}

static int cmd_whoami(uint64_t argc, char** argv) {
    struct user_info ui;
    (void)argc;
    (void)argv;
    read_user_info(&ui);
    if (ui.euid == 0) write_cstr("root");
    else if (ui.username[0]) write_cstr(ui.username);
    else write_cstr("unknown");
    write_char('\n');
    return 0;
}

static void write_user_suffix(uint32_t uid, const char* username) {
    if (uid == 0) {
        write_cstr("(root)");
    } else if (username && username[0]) {
        write_char('(');
        write_cstr(username);
        write_char(')');
    }
}

static void write_group_suffix(uint32_t gid) {
    if (gid == 0) write_cstr("(root)");
    else if (gid == 1000) write_cstr("(users)");
}

static int cmd_id(uint64_t argc, char** argv) {
    struct user_info ui;
    (void)argc;
    (void)argv;
    read_user_info(&ui);
    write_cstr("uid=");
    write_u64(ui.uid);
    write_user_suffix(ui.uid, ui.username);
    write_cstr(" gid=");
    write_u64(ui.gid);
    write_group_suffix(ui.gid);
    write_cstr(" euid=");
    write_u64(ui.euid);
    write_user_suffix(ui.euid, ui.username);
    write_cstr(" egid=");
    write_u64(ui.egid);
    write_group_suffix(ui.egid);
    write_char('\n');
    return 0;
}

static int cmd_uptime(uint64_t argc, char** argv) {
    struct uptime_info up;
    uint64_t hours;
    uint64_t minutes;
    uint64_t seconds;
    (void)argc;
    (void)argv;
    if (syscall1(AOS_SYS_UPTIME_INFO, (long)&up) < 0) {
        write_cstr("uptime: failed to read AOS uptime\n");
        return 1;
    }
    hours = up.seconds / 3600;
    minutes = (up.seconds % 3600) / 60;
    seconds = up.seconds % 60;
    write_cstr("AOS uptime: ");
    if (hours < 100) write_two_digits(hours);
    else write_u64(hours);
    write_char(':');
    write_two_digits(minutes);
    write_char(':');
    write_two_digits(seconds);
    write_cstr("\nticks: ");
    write_u64(up.ticks);
    write_char('\n');
    return 0;
}

static void write_mib(uint64_t bytes) {
    write_u64((bytes + 1048575) >> 20);
    write_cstr(" MiB\n");
}

static int cmd_mem(uint64_t argc, char** argv) {
    struct mem_info mi;
    int verbose = 0;
    if (argc > 1) {
        if (streq(argv[1], "-v") || streq(argv[1], "--verbose")) verbose = 1;
        else {
            write_cstr("usage: mem [-v]\n");
            return 1;
        }
    }
    if (syscall1(AOS_SYS_MEM_INFO, (long)&mi) < 0) {
        write_cstr("mem: failed to read AOS memory info\n");
        return 1;
    }
    write_cstr("AOS memory\ntotal: ");
    write_mib(mi.total);
    write_cstr("used:  ");
    write_mib(mi.used);
    write_cstr("free:  ");
    write_mib(mi.free);
    if (verbose) {
        write_cstr("details:\ntotal bytes: ");
        write_u64(mi.total);
        write_cstr("\nused bytes:  ");
        write_u64(mi.used);
        write_cstr("\nfree bytes:  ");
        write_u64(mi.free);
        write_cstr("\ntotal pages: ");
        write_u64(mi.total >> 12);
        write_cstr("\nused pages:  ");
        write_u64(mi.used >> 12);
        write_cstr("\nfree pages:  ");
        write_u64(mi.free >> 12);
        write_cstr("\nused:        ");
        write_u64(mi.total ? (mi.used * 100) / mi.total : 0);
        write_cstr("%\n");
    }
    return 0;
}

static int unsupported(const char* cmd, const char* why) {
    write_cstr(cmd);
    write_cstr(": not supported yet");
    if (why) {
        write_cstr(" (");
        write_cstr(why);
        write_cstr(")");
    }
    write_cstr("\n");
    return 1;
}

static int cmd_sh(uint64_t argc, char** argv) {
    const char* shell_argv[] = { "shell.elf", 0 };
    if (argc > 1 && streq(argv[1], "-c")) {
        return unsupported(basename(argv[0]), "script parser is not implemented; use AOS shell directly");
    }
    syscall3(SYS_EXECVE, (long)"/shell.elf", (long)shell_argv, 0);
    write_cstr("sh: failed to exec /shell.elf\n");
    return 1;
}

static int test_eval(uint64_t argc, char** argv) {
    if (argc == 0) return 1;
    if (argc == 1) return argv[0][0] ? 0 : 1;
    if (argc == 2) {
        if (streq(argv[0], "!")) return test_eval(1, argv + 1) == 0 ? 1 : 0;
        if (streq(argv[0], "-n")) return argv[1][0] ? 0 : 1;
        if (streq(argv[0], "-z")) return argv[1][0] ? 1 : 0;
        if (streq(argv[0], "-e")) return stat_path(argv[1], (struct linux_stat*)io_buf) < 0 ? 1 : 0;
        if (streq(argv[0], "-f")) {
            struct linux_stat st;
            return stat_path(argv[1], &st) == 0 && ((st.st_mode & S_IFMT) == S_IFREG) ? 0 : 1;
        }
        if (streq(argv[0], "-d")) return is_dir_path(argv[1]) ? 0 : 1;
    }
    if (argc == 3) {
        if (streq(argv[1], "=")) return streq(argv[0], argv[2]) ? 0 : 1;
        if (streq(argv[1], "!=")) return streq(argv[0], argv[2]) ? 1 : 0;
        int64_t a, b;
        if (parse_i64(argv[0], &a) == 0 && parse_i64(argv[2], &b) == 0) {
            if (streq(argv[1], "-eq")) return a == b ? 0 : 1;
            if (streq(argv[1], "-ne")) return a != b ? 0 : 1;
            if (streq(argv[1], "-gt")) return a > b ? 0 : 1;
            if (streq(argv[1], "-ge")) return a >= b ? 0 : 1;
            if (streq(argv[1], "-lt")) return a < b ? 0 : 1;
            if (streq(argv[1], "-le")) return a <= b ? 0 : 1;
        }
    }
    return 1;
}

static int cmd_test(uint64_t argc, char** argv) {
    const char* name = basename(argv[0]);
    if (streq(name, "[") && argc > 1 && streq(argv[argc - 1], "]")) argc--;
    return test_eval(argc > 0 ? argc - 1 : 0, argv + 1);
}

static char decode_escape(const char** p) {
    char c = **p;
    if (c == 'n') return '\n';
    if (c == 't') return '\t';
    if (c == 'r') return '\r';
    if (c == '\\') return '\\';
    return c;
}

static int cmd_printf(uint64_t argc, char** argv) {
    const char* fmt;
    uint64_t ai = 2;
    if (argc < 2) return 0;
    fmt = argv[1];
    for (uint64_t i = 0; fmt[i]; i++) {
        if (fmt[i] == '\\' && fmt[i + 1]) {
            const char* p = fmt + i + 1;
            write_char(decode_escape(&p));
            i++;
        } else if (fmt[i] == '%' && fmt[i + 1]) {
            i++;
            if (fmt[i] == '%') {
                write_char('%');
            } else if (fmt[i] == 's') {
                write_cstr(ai < argc ? argv[ai++] : "");
            } else if (fmt[i] == 'd') {
                int64_t v = 0;
                if (ai < argc) parse_i64(argv[ai++], &v);
                if (v < 0) {
                    write_char('-');
                    write_u64((uint64_t)-v);
                } else {
                    write_u64((uint64_t)v);
                }
            } else {
                write_char('%');
                write_char(fmt[i]);
            }
        } else {
            write_char(fmt[i]);
        }
    }
    return 0;
}

static int cmd_env(uint64_t argc, char** argv) {
    (void)argv;
    if (argc == 1) return 0;
    return unsupported("env", "process environment and env command exec are not implemented");
}

static int contains_at(const char* s, const char* needle, int ignore_case) {
    uint64_t n = cstrlen(needle);
    if (n == 0) return 1;
    for (uint64_t i = 0; s[i]; i++) {
        uint64_t j = 0;
        while (j < n && s[i + j]) {
            char a = s[i + j];
            char b = needle[j];
            if (ignore_case) {
                if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
                if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            }
            if (a != b) break;
            j++;
        }
        if (j == n) return 1;
    }
    return 0;
}

static int grep_fd(int fd, const char* pattern, int ignore_case, int numbered) {
    uint64_t line_no = 1;
    int matched = 1;
    for (;;) {
        int n = read_line_fd(fd, line_buf, sizeof(line_buf));
        if (n < 0) return 2;
        if (n == 0) break;
        if (contains_at(line_buf, pattern, ignore_case)) {
            if (numbered) {
                write_u64(line_no);
                write_char(':');
            }
            write_buf(line_buf, (uint64_t)n);
            matched = 0;
        }
        line_no++;
    }
    return matched;
}

static int cmd_grep(uint64_t argc, char** argv) {
    int ignore_case = 0;
    int numbered = 0;
    uint64_t i = 1;
    const char* pattern;
    int rc = 1;
    while (i < argc && argv[i][0] == '-') {
        if (streq(argv[i], "-i")) ignore_case = 1;
        else if (streq(argv[i], "-n")) numbered = 1;
        else break;
        i++;
    }
    if (i >= argc) {
        write_cstr("usage: grep [-i] [-n] PATTERN [FILE...]\n");
        return 2;
    }
    pattern = argv[i++];
    if (i >= argc) return grep_fd(0, pattern, ignore_case, numbered);
    for (; i < argc; i++) {
        int fd = open_read(argv[i]);
        if (fd < 0) {
            write_cstr("grep: open failed: ");
            write_cstr(argv[i]);
            write_cstr("\n");
            rc = 2;
            continue;
        }
        int r = grep_fd(fd, pattern, ignore_case, numbered);
        syscall1(SYS_CLOSE, fd);
        if (r == 0) rc = 0;
        else if (r == 2) rc = 2;
    }
    return rc;
}

static int name_match(const char* name, const char* pat) {
    if (!pat || !pat[0]) return 1;
    if (pat[0] == '*' && pat[1] == 0) return 1;
    if (pat[0] == '*') {
        uint64_t nl = cstrlen(name), pl = cstrlen(pat + 1);
        if (pl > nl) return 0;
        return streq(name + nl - pl, pat + 1);
    }
    return streq(name, pat);
}

static int find_walk(const char* path, const char* name_pat) {
    char child[512];
    int fd;
    int rc = 0;
    if (name_match(basename(path), name_pat)) {
        write_cstr(path);
        write_char('\n');
    }
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) return 0;
    for (;;) {
        long got = syscall3(SYS_GETDENTS64, fd, (long)io_buf, sizeof(io_buf));
        if (got < 0) {
            rc = 1;
            break;
        }
        if (got == 0) break;
        uint64_t off = 0;
        while (off < (uint64_t)got) {
            struct linux_dirent64* ent = (struct linux_dirent64*)(io_buf + off);
            if (ent->d_reclen == 0) break;
            if (!streq(ent->d_name, ".") && !streq(ent->d_name, "..")) {
                if (path_join(child, sizeof(child), path, ent->d_name) == 0) {
                    if (ent->d_type == DT_DIR || is_dir_path(child)) {
                        if (find_walk(child, name_pat) != 0) rc = 1;
                    } else if (name_match(ent->d_name, name_pat)) {
                        write_cstr(child);
                        write_char('\n');
                    }
                }
            }
            off += ent->d_reclen;
        }
    }
    syscall1(SYS_CLOSE, fd);
    return rc;
}

static int cmd_find(uint64_t argc, char** argv) {
    const char* root = ".";
    const char* name_pat = 0;
    if (argc > 1) root = argv[1];
    for (uint64_t i = 2; i + 1 < argc; i++) {
        if (streq(argv[i], "-name")) name_pat = argv[i + 1];
    }
    return find_walk(root, name_pat);
}

static void sed_emit_replace(const char* line, const char* old, const char* neu) {
    uint64_t old_len = cstrlen(old);
    for (uint64_t i = 0; line[i]; i++) {
        uint64_t j = 0;
        while (j < old_len && line[i + j] == old[j]) j++;
        if (old_len > 0 && j == old_len) {
            write_cstr(neu);
            i += old_len - 1;
        } else {
            write_char(line[i]);
        }
    }
}

static int parse_s_expr(const char* expr, char** old, char** neu) {
    char delim;
    uint64_t i = 2, n = 0;
    if (!expr || expr[0] != 's' || !expr[1]) return -1;
    delim = expr[1];
    while (expr[i] && expr[i] != delim && n + 1 < sizeof(path_buf)) path_buf[n++] = expr[i++];
    path_buf[n] = 0;
    if (expr[i] != delim) return -1;
    i++;
    n = 0;
    while (expr[i] && expr[i] != delim && n + 1 < sizeof(path_buf2)) path_buf2[n++] = expr[i++];
    path_buf2[n] = 0;
    *old = path_buf;
    *neu = path_buf2;
    return 0;
}

static int sed_fd(int fd, const char* expr) {
    char* old;
    char* neu;
    int replace = parse_s_expr(expr, &old, &neu) == 0;
    for (;;) {
        int n = read_line_fd(fd, line_buf, sizeof(line_buf));
        if (n < 0) return 1;
        if (n == 0) break;
        if (replace) sed_emit_replace(line_buf, old, neu);
        else write_buf(line_buf, (uint64_t)n);
    }
    return 0;
}

static int cmd_sed(uint64_t argc, char** argv) {
    if (argc < 2) {
        write_cstr("usage: sed 's/OLD/NEW/' [FILE...]\n");
        return 1;
    }
    if (argc == 2) return sed_fd(0, argv[1]);
    int rc = 0;
    for (uint64_t i = 2; i < argc; i++) {
        int fd = open_read(argv[i]);
        if (fd < 0) {
            write_cstr("sed: open failed: ");
            write_cstr(argv[i]);
            write_cstr("\n");
            rc = 1;
            continue;
        }
        if (sed_fd(fd, argv[1]) != 0) rc = 1;
        syscall1(SYS_CLOSE, fd);
    }
    return rc;
}

static void awk_print_field(const char* line, uint64_t field) {
    uint64_t current = 0;
    uint64_t i = 0;
    while (line[i]) {
        while (line[i] == ' ' || line[i] == '\t') i++;
        if (!line[i] || line[i] == '\n') break;
        current++;
        uint64_t start = i;
        while (line[i] && line[i] != ' ' && line[i] != '\t' && line[i] != '\n') i++;
        if (current == field) {
            write_buf(line + start, i - start);
            write_char('\n');
            return;
        }
    }
    write_char('\n');
}

static int awk_fd(int fd, uint64_t field) {
    for (;;) {
        int n = read_line_fd(fd, line_buf, sizeof(line_buf));
        if (n < 0) return 1;
        if (n == 0) break;
        if (field == 0) write_buf(line_buf, (uint64_t)n);
        else awk_print_field(line_buf, field);
    }
    return 0;
}

static int cmd_awk(uint64_t argc, char** argv) {
    uint64_t field = 0;
    const char* prog;
    if (argc < 2) {
        write_cstr("usage: awk '{print}'|'{print $N}' [FILE...]\n");
        return 1;
    }
    prog = argv[1];
    for (uint64_t i = 0; prog[i]; i++) {
        if (prog[i] == '$' && prog[i + 1] >= '0' && prog[i + 1] <= '9') {
            int64_t v = 0;
            parse_i64(prog + i + 1, &v);
            field = (uint64_t)v;
            break;
        }
    }
    if (!starts_with(prog, "{print")) return unsupported("awk", "only {print} and {print $N} are implemented");
    if (argc == 2) return awk_fd(0, field);
    int rc = 0;
    for (uint64_t i = 2; i < argc; i++) {
        int fd = open_read(argv[i]);
        if (fd < 0) {
            write_cstr("awk: open failed: ");
            write_cstr(argv[i]);
            write_cstr("\n");
            rc = 1;
            continue;
        }
        if (awk_fd(fd, field) != 0) rc = 1;
        syscall1(SYS_CLOSE, fd);
    }
    return rc;
}

static int cmd_mount(uint64_t argc, char** argv) {
    if (argc > 1) return unsupported("mount", "mount creation syscall is not implemented");
    syscall3(SYS_EXECVE, (long)"/mounts", (long)argv, 0);
    return unsupported("mount", "mount listing applet missing");
}

static int cmd_df(uint64_t argc, char** argv) {
    (void)argc;
    (void)argv;
    write_cstr("Filesystem Mounted-on Source\n");
    for (uint64_t i = 0;; i++) {
        struct mount_info mi;
        long rc = syscall3(AOS_SYS_MOUNT_INFO, (long)i, (long)&mi, 0);
        if (rc < 0) break;
        write_u64(mi.backend);
        write_char(' ');
        write_cstr(mi.path[0] ? mi.path : "/");
        write_char(' ');
        write_cstr(mi.backend_root[0] ? mi.backend_root : "-");
        write_char('\n');
    }
    return 0;
}

static uint64_t du_walk(const char* path) {
    char child[512];
    struct linux_stat st;
    uint64_t total = 0;
    int fd;
    if (stat_path(path, &st) == 0 && st.st_size > 0) total += (uint64_t)st.st_size;
    fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) return total;
    for (;;) {
        long got = syscall3(SYS_GETDENTS64, fd, (long)io_buf, sizeof(io_buf));
        if (got <= 0) break;
        uint64_t off = 0;
        while (off < (uint64_t)got) {
            struct linux_dirent64* ent = (struct linux_dirent64*)(io_buf + off);
            if (ent->d_reclen == 0) break;
            if (!streq(ent->d_name, ".") && !streq(ent->d_name, "..") &&
                path_join(child, sizeof(child), path, ent->d_name) == 0) {
                total += du_walk(child);
            }
            off += ent->d_reclen;
        }
    }
    syscall1(SYS_CLOSE, fd);
    return total;
}

static int cmd_du(uint64_t argc, char** argv) {
    if (argc == 1) {
        write_u64(du_walk("."));
        write_cstr("\t.\n");
        return 0;
    }
    for (uint64_t i = 1; i < argc; i++) {
        write_u64(du_walk(argv[i]));
        write_char('\t');
        write_cstr(argv[i]);
        write_char('\n');
    }
    return 0;
}

static int cmd_sleep(uint64_t argc, char** argv) {
    int64_t sec;
    struct timespec ts;
    if (argc != 2 || parse_i64(argv[1], &sec) != 0 || sec < 0) {
        write_cstr("usage: sleep SECONDS\n");
        return 1;
    }
    ts.tv_sec = sec;
    ts.tv_nsec = 0;
    return syscall2(SYS_NANOSLEEP, (long)&ts, 0) < 0 ? 1 : 0;
}

static int cmd_dmesg(uint64_t argc, char** argv) {
    (void)argc;
    (void)argv;
    return unsupported("dmesg", "kernel log read syscall is not implemented");
}

static int cmd_kill(uint64_t argc, char** argv) {
    int64_t pid;
    long rc;
    if (argc != 2 || parse_i64(argv[1], &pid) != 0 || pid <= 0) {
        write_cstr("usage: kill PID\n");
        return 1;
    }
    rc = syscall3(AOS_SYS_PROCESS_KILL, pid, 15, 0);
    if (rc < 0) {
        write_cstr("kill: failed for pid ");
        write_u64((uint64_t)pid);
        write_cstr("\n");
        return 1;
    }
    return 0;
}

static int cmd_process_control(uint64_t argc, char** argv, long syscall_nr, const char* name) {
    int64_t pid;
    long rc;
    if (argc != 2 || parse_i64(argv[1], &pid) != 0 || pid <= 0) {
        write_cstr("usage: ");
        write_cstr(name);
        write_cstr(" PID\n");
        return 1;
    }
    rc = syscall3(syscall_nr, pid, 0, 0);
    if (rc < 0) {
        write_cstr(name);
        write_cstr(": failed for pid ");
        write_u64((uint64_t)pid);
        write_cstr("\n");
        return 1;
    }
    return 0;
}

static int cmd_admin_stub(uint64_t argc, char** argv) {
    (void)argc;
    return unsupported(basename(argv[0]), "kernel backend syscall is not implemented");
}

static int cmd_archive_stub(uint64_t argc, char** argv) {
    (void)argc;
    return unsupported(basename(argv[0]), "archive/compression engine is not implemented");
}

static int cmd_asheel(void) {
    write_cstr("Asheel: AOS native command box\n");
    write_cstr("applets: sh ash test [ printf env grep find sed awk chmod chown ln mount umount df du tar gzip kill pause resume sleep dmesg cat echo true false pwd ls date stat head tail touch rm mkdir rmdir cp mv uname whoami id uptime mem\n");
    return 0;
}

void aos_main(uint64_t argc, char** argv) {
    const char* cmd = argc > 0 ? basename(argv[0]) : "asheel";
    int rc;
    if (streq(cmd, "asheel") || streq(cmd, "Asheel") || streq(cmd, "asheel.elf")) rc = cmd_asheel();
    else if (streq(cmd, "sh") || streq(cmd, "ash")) rc = cmd_sh(argc, argv);
    else if (streq(cmd, "test") || streq(cmd, "[")) rc = cmd_test(argc, argv);
    else if (streq(cmd, "printf")) rc = cmd_printf(argc, argv);
    else if (streq(cmd, "env")) rc = cmd_env(argc, argv);
    else if (streq(cmd, "grep")) rc = cmd_grep(argc, argv);
    else if (streq(cmd, "find")) rc = cmd_find(argc, argv);
    else if (streq(cmd, "sed")) rc = cmd_sed(argc, argv);
    else if (streq(cmd, "awk")) rc = cmd_awk(argc, argv);
    else if (streq(cmd, "mount")) rc = cmd_mount(argc, argv);
    else if (streq(cmd, "umount")) rc = cmd_admin_stub(argc, argv);
    else if (streq(cmd, "df")) rc = cmd_df(argc, argv);
    else if (streq(cmd, "du")) rc = cmd_du(argc, argv);
    else if (streq(cmd, "tar") || streq(cmd, "gzip")) rc = cmd_archive_stub(argc, argv);
    else if (streq(cmd, "chmod") || streq(cmd, "chown") || streq(cmd, "ln")) rc = cmd_admin_stub(argc, argv);
    else if (streq(cmd, "kill")) rc = cmd_kill(argc, argv);
    else if (streq(cmd, "pause")) rc = cmd_process_control(argc, argv, AOS_SYS_PROCESS_PAUSE, "pause");
    else if (streq(cmd, "resume")) rc = cmd_process_control(argc, argv, AOS_SYS_PROCESS_RESUME, "resume");
    else if (streq(cmd, "sleep")) rc = cmd_sleep(argc, argv);
    else if (streq(cmd, "dmesg")) rc = cmd_dmesg(argc, argv);
    else if (streq(cmd, "cat")) rc = cmd_cat(argc, argv);
    else if (streq(cmd, "echo")) rc = cmd_echo(argc, argv);
    else if (streq(cmd, "true")) rc = 0;
    else if (streq(cmd, "false")) rc = 1;
    else if (streq(cmd, "pwd")) rc = cmd_pwd(argc, argv);
    else if (streq(cmd, "ls")) rc = cmd_ls(argc, argv);
    else if (streq(cmd, "date")) rc = cmd_date(argc, argv);
    else if (streq(cmd, "stat")) rc = cmd_stat(argc, argv);
    else if (streq(cmd, "head")) rc = cmd_head(argc, argv);
    else if (streq(cmd, "tail")) rc = cmd_tail(argc, argv);
    else if (streq(cmd, "touch")) rc = cmd_touch(argc, argv);
    else if (streq(cmd, "rm")) rc = cmd_rm(argc, argv);
    else if (streq(cmd, "mkdir")) rc = cmd_mkdir(argc, argv);
    else if (streq(cmd, "rmdir")) rc = cmd_rmdir(argc, argv);
    else if (streq(cmd, "cp")) rc = cmd_cp(argc, argv);
    else if (streq(cmd, "mv")) rc = cmd_mv(argc, argv);
    else if (streq(cmd, "uname")) rc = cmd_uname(argc, argv);
    else if (streq(cmd, "whoami")) rc = cmd_whoami(argc, argv);
    else if (streq(cmd, "id")) rc = cmd_id(argc, argv);
    else if (streq(cmd, "uptime")) rc = cmd_uptime(argc, argv);
    else if (streq(cmd, "mem")) rc = cmd_mem(argc, argv);
    else {
        write_cstr("Asheel: applet not found\n");
        rc = 127;
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
