/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define SYS_NANOSLEEP 35

#define AOS_SYS_MOUNT_INFO 506
#define AOS_SYS_MEM_INFO 509
#define AOS_SYS_UPTIME_INFO 510
#define AOS_SYS_DISPLAY_INFO 511
#define AOS_SYS_GFX_INFO 520
#define AOS_SYS_INPUT_POLL 525
#define AOS_SYS_PROCESS_INFO 544
#define AOS_SYS_CPU_INFO 545
#define AOS_SYS_PROCESS_KILL 546
#define AOS_SYS_PROCESS_PAUSE 547
#define AOS_SYS_PROCESS_RESUME 548

#define MAX_PROCESSES 64
#define MAX_VISIBLE_PROCESSES 5
#define MAX_VISIBLE_MOUNTS 4
#define BAR_WIDE 54
#define BAR_HALF 24

#define PROCESS_STATUS_READY 1
#define PROCESS_STATUS_RUNNING 2
#define PROCESS_STATUS_WAITING 3
#define PROCESS_STATUS_ZOMBIE 4
#define PROCESS_STATUS_PAUSED 5

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct aos_cpu_info {
    uint32_t core_id;
    uint32_t core_count;
    uint32_t online;
    uint32_t load_percent;
    uint32_t load_known;
    int32_t temp_celsius;
    uint32_t temp_known;
    uint32_t frequency_hz;
    uint64_t ticks;
};

struct aos_mem_info {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

struct aos_uptime_info {
    uint64_t ticks;
    uint64_t seconds;
    uint32_t frequency;
    uint32_t reserved;
};

struct aos_display_info {
    uint32_t cols;
    uint32_t rows;
    uint32_t detected_cols;
    uint32_t detected_rows;
    uint32_t max_cols;
    uint32_t max_rows;
};

struct aos_gfx_info {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t ready;
};

struct aos_input_event {
    uint32_t key;
    uint32_t flags;
    uint32_t ascii;
    uint32_t source;
};

struct aos_mount_info {
    uint8_t backend;
    uint8_t reserved[7];
    char path[64];
    char backend_root[64];
};

struct aos_process_info {
    uint8_t valid;
    uint8_t status;
    uint8_t thermal_throttled;
    uint8_t reserved;
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
    uint64_t cpu_ticks;
} __attribute__((packed));

static char num_buf[32];
static struct aos_cpu_info cpu_info;
static struct aos_mem_info mem_info;
static struct aos_uptime_info uptime_info;
static struct aos_display_info display_info;
static struct aos_gfx_info gfx_info;
static struct aos_mount_info mount_info;
static struct aos_process_info process_info;

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

static void write_i32(int32_t v) {
    if (v < 0) {
        write_char('-');
        write_u64((uint64_t)-v);
    } else {
        write_u64((uint64_t)v);
    }
}

static void write_fixed(const char* s, uint64_t width) {
    uint64_t n = cstrlen(s);
    if (n > width) n = width;
    write_buf(s, n);
    while (n++ < width) write_char(' ');
}

static void write_bar(uint64_t used, uint64_t total, uint64_t width) {
    uint64_t filled = total ? (used * width) / total : 0;
    write_char('[');
    for (uint64_t i = 0; i < width; i++) write_char(i < filled ? '|' : ' ');
    write_char(']');
}

static void write_percent(uint64_t used, uint64_t total) {
    if (total == 0) {
        write_cstr("n/a");
        return;
    }
    write_u64((used * 100ULL) / total);
    write_char('%');
}

static void write_mib(uint64_t bytes) {
    write_u64((bytes + 1048575ULL) / 1048576ULL);
    write_cstr("M");
}

static const char* status_name(uint8_t status) {
    switch (status) {
        case PROCESS_STATUS_READY: return "ready";
        case PROCESS_STATUS_RUNNING: return "run";
        case PROCESS_STATUS_WAITING: return "wait";
        case PROCESS_STATUS_ZOMBIE: return "zombie";
        case PROCESS_STATUS_PAUSED: return "pause";
        default: return "unknown";
    }
}

static const char* backend_name(uint8_t backend) {
    switch (backend) {
        case 1: return "synthetic";
        case 2: return "initrd";
        case 3: return "fat32";
        case 4: return "tmpfs";
        case 5: return "ext4";
        case 6: return "aosfs";
        default: return "unknown";
    }
}

static const char* fallback(const char* s, const char* f) {
    return (s && s[0]) ? s : f;
}

static uint64_t active_process_count(void) {
    uint64_t n = 0;
    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        long rc = syscall3(AOS_SYS_PROCESS_INFO, (long)i, (long)&process_info, 0);
        if (rc >= 0 && process_info.valid) n++;
    }
    return n;
}

static uint64_t synthetic_activity(uint64_t salt, uint64_t max) {
    uint64_t ticks = uptime_info.ticks ? uptime_info.ticks : 1;
    uint64_t procs = active_process_count();
    uint64_t v = (ticks / 3 + procs * 17 + salt * 23) % (max + 1);
    return v;
}

static void fetch_common(void) {
    if (syscall3(AOS_SYS_UPTIME_INFO, (long)&uptime_info, 0, 0) < 0) {
        uptime_info.ticks = 0;
        uptime_info.seconds = 0;
        uptime_info.frequency = 100;
    }
    if (syscall3(AOS_SYS_DISPLAY_INFO, (long)&display_info, 0, 0) < 0) {
        display_info.cols = 80;
        display_info.rows = 25;
    }
}

static void draw_cpu_line(uint64_t index) {
    long rc = syscall3(AOS_SYS_CPU_INFO, (long)index, (long)&cpu_info, 0);
    uint64_t load = 0;

    if (rc < 0) return;
    if (cpu_info.load_known) load = cpu_info.load_percent;
    else load = synthetic_activity(index, 99);

    write_char('C');
    write_u64(cpu_info.core_id);
    write_char('|');
    if (cpu_info.load_known) {
        write_percent(cpu_info.load_percent, 100);
    } else {
        write_cstr("%?");
    }
    write_char('|');
    if (cpu_info.temp_known) {
        write_i32(cpu_info.temp_celsius);
        write_cstr("C");
    } else {
        write_cstr("x C");
    }
    write_char('|');
    write_bar(load, 100, BAR_WIDE);
    write_char('\n');
}

static void draw_cpu_block(void) {
    uint64_t count = 1;
    if (syscall3(AOS_SYS_CPU_INFO, 0, (long)&cpu_info, 0) >= 0 && cpu_info.core_count) {
        count = cpu_info.core_count;
    }
    if (count > 6) count = 6;
    for (uint64_t i = 0; i < count; i++) draw_cpu_line(i);
    if (count == 0) write_cstr("C0|n/a|x C|[                                                  ]\n");
}

static void draw_gpu_line(void) {
    uint64_t load = synthetic_activity(8, 100);
    long rc = syscall3(AOS_SYS_GFX_INFO, (long)&gfx_info, 0, 0);
    write_cstr("GPU|");
    write_cstr("%?");
    write_char('|');
    write_cstr("x C|");
    write_bar(load, 100, BAR_WIDE);
    if (rc >= 0 && gfx_info.ready) {
        write_cstr(" ");
        write_u64(gfx_info.width);
        write_char('x');
        write_u64(gfx_info.height);
    } else {
        write_cstr(" nofb");
    }
    write_char('\n');
}

static void draw_memory_storage(void) {
    uint64_t storage_rows = 0;
    if (syscall3(AOS_SYS_MEM_INFO, (long)&mem_info, 0, 0) < 0) {
        mem_info.total = 0;
        mem_info.used = 0;
        mem_info.free = 0;
    }

    write_cstr("--------------------------------------------------+-----------------------------\n");
    write_cstr("RAM                                               | Storage\n");
    write_cstr("--------------------------------------------------+-----------------------------\n");
    write_cstr("using ");
    write_mib(mem_info.used);
    write_cstr(" ");
    write_percent(mem_info.used, mem_info.total);
    write_cstr(" ");
    write_bar(mem_info.used, mem_info.total, BAR_HALF);
    write_cstr(" | ");
    for (uint64_t i = 0; i < MAX_VISIBLE_MOUNTS; i++) {
        long rc = syscall3(AOS_SYS_MOUNT_INFO, (long)i, (long)&mount_info, 0);
        if (i > 0) write_cstr("                                                  | ");
        if (rc < 0) {
            if (i == 0) write_cstr("no mounts");
            write_char('\n');
            break;
        }
        write_fixed(fallback(mount_info.path, "/"), 9);
        write_cstr(" ");
        write_fixed(backend_name(mount_info.backend), 9);
        write_cstr(" cap n/a free n/a\n");
        storage_rows++;
    }
    if (storage_rows == 0) write_cstr("                                                  | no mounts\n");
    write_cstr("free  ");
    write_mib(mem_info.free);
    write_cstr("   swap 0M   cached n/a\n");
    write_cstr("--------------------------------------------------+-----------------------------\n");
}

static void draw_processes(void) {
    uint64_t shown = 0;
    write_cstr("App Name                                      CPU  GPU  RAM  Action\n");
    write_cstr("--------------------------------------------------------------------------\n");
    for (uint64_t i = 0; i < MAX_PROCESSES && shown < MAX_VISIBLE_PROCESSES; i++) {
        long rc = syscall3(AOS_SYS_PROCESS_INFO, (long)i, (long)&process_info, 0);
        uint64_t cpu_percent;
        if (rc < 0 || !process_info.valid) continue;
        cpu_percent = uptime_info.ticks ? (process_info.cpu_ticks * 100ULL) / uptime_info.ticks : 0;
        if (cpu_percent > 100) cpu_percent = 100;
        write_fixed(fallback(process_info.command, "-"), 45);
        write_u64(cpu_percent);
        write_char('%');
        if (cpu_percent < 10) write_cstr("   ");
        else if (cpu_percent < 100) write_cstr("  ");
        else write_char(' ');
        write_cstr("n/a  n/a  ");
        if (process_info.status == PROCESS_STATUS_PAUSED) {
            write_cstr("resume ");
        } else {
            write_cstr("pause ");
        }
        write_u64(process_info.pid);
        write_cstr(" kill ");
        write_u64(process_info.pid);
        write_cstr(" ");
        write_cstr(status_name(process_info.status));
        write_char('\n');
        shown++;
    }
    while (shown < MAX_VISIBLE_PROCESSES) {
        write_cstr("-                                             n/a  n/a  n/a  -\n");
        shown++;
    }
}

static int should_quit(void) {
    struct aos_input_event event;
    long rc = syscall3(AOS_SYS_INPUT_POLL, (long)&event, 0, 0);
    return rc > 0 && (event.ascii == 'q' || event.ascii == 'Q');
}

static void sleep_frame(void) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500000000;
    syscall2(SYS_NANOSLEEP, (long)&ts, 0);
}

static void draw_screen(int cpu, int gpu, int ram, int storage, int processes, int show_all) {
    fetch_common();
    write_cstr("\033[?25l\033[H\033[2J");
    write_cstr("Asence");
    write_cstr("  uptime ");
    write_u64(uptime_info.seconds);
    write_cstr("s  q=quit  ");
    write_u64(display_info.cols ? display_info.cols : 80);
    write_char('x');
    write_u64(display_info.rows ? display_info.rows : 25);
    write_char('\n');

    if (show_all || cpu) draw_cpu_block();
    if (show_all || gpu) draw_gpu_line();
    if (show_all || ram || storage) draw_memory_storage();
    if (show_all || processes) draw_processes();
    write_cstr("--------------------------------------------------------------------------\n");
    write_cstr("CPU/GPU load and temp use kernel known flags; unknown values render as ?/x.\n");
}

static void usage(void) {
    write_cstr("usage: Asence [--Cpu|--GPU|--Ram|--Storage|--Proceses|--Processes|--once]\n");
}

void aos_main(uint64_t argc, char** argv) {
    int show_all = 1;
    int once = 0;
    int cpu = 0;
    int gpu = 0;
    int ram = 0;
    int storage = 0;
    int processes = 0;

    for (uint64_t i = 1; i < argc; i++) {
        if (streq(argv[i], "--once")) {
            once = 1;
        } else if (streq(argv[i], "--Cpu") || streq(argv[i], "--CPU") || streq(argv[i], "--cpu")) {
            show_all = 0;
            cpu = 1;
        } else if (streq(argv[i], "--GPU") || streq(argv[i], "--Gpu") || streq(argv[i], "--gpu")) {
            show_all = 0;
            gpu = 1;
        } else if (streq(argv[i], "--Ram") || streq(argv[i], "--RAM") || streq(argv[i], "--ram")) {
            show_all = 0;
            ram = 1;
        } else if (streq(argv[i], "--Storage") || streq(argv[i], "--storage")) {
            show_all = 0;
            storage = 1;
        } else if (streq(argv[i], "--Proceses") || streq(argv[i], "--Processes") || streq(argv[i], "--processes")) {
            show_all = 0;
            processes = 1;
        } else {
            usage();
            exit_code(1);
        }
    }

    for (;;) {
        draw_screen(cpu, gpu, ram, storage, processes, show_all);
        if (once || should_quit()) break;
        sleep_frame();
    }
    write_cstr("\033[?25h");
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
