/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>
#include <aos_install.h>
#include <aos_session.h>
#include "mui_font_outfit.h"
#include "mui_wallpaper.h"

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_POLL 7
#define SYS_ACCESS 21
#define SYS_PIPE 22
#define SYS_DUP2 33
#define SYS_FORK 57
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61
#define SYS_GETCWD 79
#define SYS_CHDIR 80
#define SYS_NANOSLEEP 35
#define SYS_GETDENTS64 217
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258
#define SYS_UNLINKAT 263

#define AT_FDCWD -100
#define AT_REMOVEDIR 0x200
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 64
#define O_TRUNC 512
#define O_DIRECTORY 0x10000
#define DT_DIR 4
#define POLLIN 0x0001
#define POLLHUP 0x0010
#define WNOHANG 1

#define AOS_SYS_MEM_INFO 509
#define AOS_SYS_UPTIME_INFO 510
#define AOS_SYS_SHUTDOWN 513
#define AOS_SYS_RESTART 514
#define AOS_SYS_TIME_INFO 515
#define AOS_SYS_GFX_INFO 520
#define AOS_SYS_GFX_CLEAR 521
#define AOS_SYS_GFX_RECT 523
#define AOS_SYS_GFX_PRESENT 524
#define AOS_SYS_GFX_BLIT_RGB565 553
#define AOS_SYS_GFX_BLEND_ROUND_RECT 554
#define AOS_SYS_GFX_CURSOR 555
#define AOS_SYS_GFX_AA_LINE 556
#define AOS_SYS_GFX_AA_CIRCLE 557
#define AOS_SYS_GFX_BLIT_RGB565_RECT 558
#define AOS_SYS_GFX_BLIT_ALPHA_MASK 559
#define AOS_SYS_PROCESS_YIELD 560
#define AOS_SYS_THERMAL_INFO 561
#define AOS_SYS_INSTALLER 562
#define AOS_SYS_SESSION 563
#define AOS_SYS_BLKDEV_INFO 504
#define AOS_SYS_PCI_INFO 518
#define AOS_SYS_DRIVER_INFO 519
#define AOS_SYS_INPUT_POLL 525
#define AOS_SYS_PROCESS_INFO 544
#define AOS_SYS_CPU_INFO 545
#define AOS_SYS_PROCESS_KILL 546
#define AOS_SYS_BLUETOOTH_INFO 549

#define KEY_UP 0x11
#define KEY_DOWN 0x12
#define KEY_LEFT 0x13
#define KEY_RIGHT 0x14
#define KEY_POINTER 0x100

#define AOS_INPUT_SOURCE_MOUSE 3
#define AOS_INPUT_SOURCE_USB 2
#define AOS_INPUT_FLAG_PRESSED 1U
#define AOS_INPUT_FLAG_CTRL (1U << 8)
#define AOS_INPUT_FLAG_SHIFT (1U << 9)
#define AOS_INPUT_FLAG_ALT (1U << 10)
#define AOS_POINTER_LEFT 1
#define AOS_POINTER_RIGHT 2

#define INPUT_RESULT_QUIT 1
#define INPUT_RESULT_REDRAW 2
#define INPUT_RESULT_POINTER 4

#define MAX_PROCESSES 64
#define MAX_VISIBLE_PROCESSES 8
#define MAX_FILE_ENTRIES 128
#define MAX_SOFTWARE_PACKAGES 24
#define TERMINAL_INPUT_CAP 256
#define TERMINAL_MAX_ARGS 16
#define TERMINAL_MAX_LINES 96
#define TERMINAL_LINE_CAP 96
#define TERMINAL_HISTORY_CAP 16

#define PROCESS_STATUS_READY 1
#define PROCESS_STATUS_RUNNING 2
#define PROCESS_STATUS_WAITING 3
#define PROCESS_STATUS_ZOMBIE 4
#define PROCESS_STATUS_PAUSED 5

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

struct aos_input_event {
    uint32_t key;
    uint32_t flags;
    uint32_t ascii;
    uint32_t source;
    int32_t pointer_x;
    int32_t pointer_y;
    int32_t pointer_dx;
    int32_t pointer_dy;
    uint32_t pointer_buttons;
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

struct aos_time_info {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
    uint8_t reserved;
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

struct aos_thermal_info {
    uint32_t sensor_present;
    uint32_t temp_known;
    uint32_t emergency_active;
    uint32_t hardware_throttling;
    uint32_t critical_active;
    uint32_t package_sensor_present;
    uint32_t throttled_processes;
    uint32_t reserved;
    int32_t current_celsius;
    int32_t maximum_celsius;
    int32_t tjmax_celsius;
    int32_t emergency_celsius;
    int32_t clear_celsius;
    uint32_t reserved2;
    uint64_t sample_ticks;
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

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

struct linux_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

struct mui_file_entry {
    char name[64];
    uint8_t is_dir;
};

struct mui_package_entry {
    char id[48];
    char name[64];
    char version[32];
    char kind[16];
    char compatibility[24];
    char entry[160];
    char description[96];
};

enum software_action {
    SOFTWARE_ACTION_NONE,
    SOFTWARE_ACTION_INSTALL,
    SOFTWARE_ACTION_RUN,
    SOFTWARE_ACTION_REMOVE,
};

struct bluetooth_info {
    uint8_t present;
    uint8_t controller_type;
    uint8_t interface_number;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t endpoints;
    uint8_t hci_ready;
    uint8_t event_endpoint;
    uint8_t event_interval;
    uint8_t last_event;
    uint8_t last_status;
    uint16_t event_max_packet;
    uint16_t hci_revision;
    uint16_t manufacturer;
    uint16_t lmp_subversion;
    uint8_t hci_version;
    uint8_t lmp_version;
    uint8_t scanning;
    uint8_t pairing;
    uint8_t discovered_count;
    uint8_t last_scan_status;
    uint8_t last_pair_status;
    char driver[32];
    char status[64];
} __attribute__((packed));

struct app_meta {
    const char* name;
    const char* short_name;
    char mark;
    uint32_t tile_a;
    uint32_t tile_b;
    uint8_t pinned;
    uint8_t desktop;
};

struct aos_blkdev_user {
    uint32_t id;
    uint32_t block_size;
    uint64_t size;
    uint8_t read_only;
    uint8_t has_ops;
    uint8_t reserved[6];
    char name[16];
};

static const struct app_meta apps[] = {
    {"Files", "FILES", 'F', 0xd1882d, 0xbd6f22, 1, 1},
    {"Settings", "SET", 'S', 0x527697, 0x405e78, 1, 1},
    {"Terminal", "TERM", '>', 0x263c32, 0x17261f, 1, 0},
    {"Text Editor", "EDIT", 'E', 0x1699b7, 0x1284a3, 1, 0},
    {"System Monitor", "SYS", 'A', 0xdc3d55, 0xc82f48, 1, 0},
    {"Calculator", "CALC", '+', 0x7450b9, 0x6240a6, 1, 0},
    {"Calendar", "CAL", 'C', 0xcf544f, 0xbd413c, 0, 0},
    {"Clocks", "TIME", 'T', 0x0896a6, 0x087f96, 0, 0},
    {"Photos", "PHOTO", 'I', 0xbe58ae, 0xa74295, 0, 0},
    {"Disks", "DISK", 'D', 0x5c748a, 0x475f75, 0, 0},
    {"Software", "SOFT", 'P', 0x159c6d, 0x0b8259, 0, 0},
    {"About AOS", "ABOUT", 'A', 0x4f9c55, 0xb78d24, 0, 1},
    {"Install AOS", "INSTALL", 'I', 0x238c50, 0xc8872d, 0, 0},
};

#define APP_COUNT ((int)(sizeof(apps) / sizeof(apps[0])))

static const char* terminal_commands[] = {
    "help", "clear", "cd", "history", "exit", "files",
    "sh", "ash", "test", "[", "printf", "env", "grep", "find",
    "sed", "awk", "chmod", "chown", "ln", "mount", "umount", "df",
    "du", "tar", "gzip", "kill", "pause", "resume", "sleep", "dmesg",
    "cat", "echo", "true", "false", "pwd", "ls", "date", "stat",
    "head", "tail", "touch", "rm", "mkdir", "rmdir", "cp", "mv",
    "uname", "whoami", "id", "uptime", "mem", "ps", "asheel", "asence",
    "asnece", "vsys", "partitions", "partition", "mounts", "lspci",
    "drivers", "driver", "net", "ifconfig", "ip", "wifi", "bluetooth",
    "bt", "bluethoot-NOW", "bluetooth-NOW", "bluethoot-now",
    "bluetooth-now", "firmware", "fw", "usb", "ping", "dns", "nslookup",
    "ping6", "rdisc6", "route", "neigh", "dhcp", "tcp", "httpget",
    "curl", "acur", "uni", "afetch", "kshttpget", "tcpstress", "dlstress",
    "wget", "download", "https", "tlsprobe", "sockclose", "gethost",
    "netcache", "netstat", "netrawtest", "aossetup", "arootinstall",
    "install-aos", "display",
    "settings", "sudo", "nano", "aosnano", "devtest", "gfxdemo",
    "inputtest", "shutdown", "restart", "reboot",
};

#define TERMINAL_COMMAND_COUNT \
    ((int)(sizeof(terminal_commands) / sizeof(terminal_commands[0])))

static struct aos_gfx_info gfx;
static struct aos_mem_info mem_info;
static struct aos_uptime_info uptime_info;
static struct aos_time_info time_info;
static struct aos_cpu_info cpu_info;
static struct aos_thermal_info thermal_info;
static int thermal_state_initialized;
static uint32_t thermal_last_emergency;
static struct bluetooth_info bt_info;
static struct aos_process_info proc_info;
static struct aos_process_info visible_procs[MAX_VISIBLE_PROCESSES];
static uint64_t visible_proc_count;
static uint64_t process_count;
static uint64_t total_cpu_percent;
static int active_app;
static int window_open;
static int overview_open;
static int once_mode;
static int pointer_x;
static int pointer_y;
static uint32_t pointer_buttons;
static int hover_desktop_app;
static int hover_dock_app;
static int hover_overview_app;
static int hover_all_apps;
static int hover_activities;
static int hover_topbar_target;
static int hover_power_action;
static int hover_context_action;
static int hover_close;
static int power_menu_open;
static int power_action_failed;
static int context_menu_open;
static int context_menu_kind;
static int context_menu_x;
static int context_menu_y;
static int window_dragging;
static int window_drag_offset_x;
static int window_drag_offset_y;
static int window_has_position;
static int window_maximized;
static int window_x;
static int window_y;
static int files_place;
static int files_view;
static int files_scroll;
static int files_last_clicked;
static uint64_t files_last_click_ticks;
static char files_current_path[256];
static struct mui_file_entry file_entries[MAX_FILE_ENTRIES];
static int file_entry_count;
static int files_selected;
static char files_status[64];
static int settings_section;
static int settings_theme_dark;
static int settings_accent;
static int settings_animations;
static int settings_transparency;
static int settings_wifi;
static int settings_volume;
static int settings_input_volume;
static int settings_system_sounds;
static int settings_brightness;
static int settings_night_light;
static int settings_notifications;
static int settings_do_not_disturb;
static int settings_notification_banners;
static int settings_power_mode;
static int settings_reduce_motion_saver;
static int settings_remember_history;
static int settings_feature_enabled[10];
static int settings_auto_updates[10];
static int settings_slider_drag;
static int settings_config_dirty;
static int settings_save_failed;
static int settings_notice_frames;
static uint64_t settings_notice_until_second;
static char settings_notice[80];
static int calendar_selected;
static int calendar_month;
static int calendar_year;
static int photo_index;
static int software_category;
static int software_scroll;
static struct mui_package_entry software_packages[MAX_SOFTWARE_PACKAGES];
static int software_package_count;
static int software_child_pid;
static int software_child_stdout;
static int software_action;
static char software_action_target[64];
static char software_status[112];
static char software_db_buffer[8192];
static char software_output_line[112];
static int software_output_length;
static struct aos_install_config install_config;
static struct aos_install_status install_status;
static struct aos_session_status session_status;
static struct aos_session_request session_request;
static int login_active;
static int login_password_focused;
static int login_button_hovered;
static char login_notice[96];
static struct aos_blkdev_user install_disks[8];
static int install_disk_count;
static int install_page;
static int install_focus;
static int install_scan_pci;
static int install_scan_drivers;
static char install_keyboard_test[64];
static char install_notice[96];
static char calc_display[24];
static int calc_value;
static int calc_operand;
static char calc_op;
static int calc_fresh;
static char editor_text[12][96];
static int editor_lines;
static int editor_saved;
static int editor_save_failed;
static char editor_filename[64];
static char editor_path[256];
static char terminal_input[TERMINAL_INPUT_CAP];
static int terminal_input_len;
static char terminal_last[TERMINAL_MAX_LINES][TERMINAL_LINE_CAP];
static int terminal_last_count;
static char terminal_stream_line[TERMINAL_LINE_CAP];
static int terminal_stream_len;
static int terminal_stream_cursor;
static int terminal_escape_state;
static int terminal_escape_value;
static int terminal_child_pid;
static int terminal_child_stdin;
static int terminal_child_stdout;
static char terminal_active_command[64];
static char terminal_cwd[256];
static char terminal_history[TERMINAL_HISTORY_CAP][TERMINAL_INPUT_CAP];
static int terminal_history_count;
static int terminal_history_position;
static char num_buf[32];

static int app_is_available(int app) {
    if (app < 0 || app >= APP_COUNT) return 0;
    if (app == 12)
        return install_status.live_mode && install_status.payload_ready;
    return 1;
}

#define UI_PANEL (settings_theme_dark ? 0x1d2923 : 0x28372f)
#define UI_PANEL_2 (settings_theme_dark ? 0x33443a : 0x405246)
#define UI_PANEL_BORDER (settings_theme_dark ? 0x526359 : 0x6f7d72)
#define UI_PANEL_TEXT 0xf5f7ef
#define UI_WINDOW (settings_theme_dark ? 0x202b25 : 0xf1f5ed)
#define UI_WINDOW_HEADER (settings_theme_dark ? 0x29372f : 0xe4ece0)
#define UI_CARD (settings_theme_dark ? 0x25322b : 0xf8faf4)
#define UI_CARD_ALT (settings_theme_dark ? 0x2c3b32 : 0xe8efe4)
#define UI_BORDER (settings_theme_dark ? 0x46564b : 0xcbd6c5)
#define UI_TEXT (settings_theme_dark ? 0xf0f5ef : 0x26352d)
#define UI_MUTED (settings_theme_dark ? 0xaab7ad : 0x667160)
#define UI_ACCENT ((settings_accent) == 1 ? 0xcd604c : \
                   (settings_accent) == 2 ? 0x3f7fa8 : \
                   (settings_accent) == 3 ? 0xb650ae : 0x238c50)
#define UI_SHADOW 0x152016
#define UI_WARNING 0xc35b30
#define UI_GLASS_DARK 0x5d5b4f
#define UI_DOCK_GLASS 0x5f5847
#define UI_GLASS_LIGHT (settings_theme_dark ? 0x26332c : 0xf7faf4)
#define UI_GLASS_SIDE (settings_theme_dark ? 0x202c26 : 0xe4eee5)
#define UI_ICON_GREEN UI_ACCENT

#define TOPBAR_HEIGHT 36
#define DOCK_HEIGHT 74
#define DOCK_BOTTOM 12
#define DOCK_TILE_SIZE 48
#define DOCK_ITEM_STEP 56
#define POWER_MENU_WIDTH 244
#define POWER_MENU_HEIGHT 282
#define CONTEXT_MENU_WIDTH 220
#define CONTEXT_MENU_ROW_HEIGHT 40

enum topbar_target {
    TOPBAR_TARGET_NONE = -1,
    TOPBAR_TARGET_ACTIVITIES,
    TOPBAR_TARGET_CLOCK,
    TOPBAR_TARGET_SEARCH,
    TOPBAR_TARGET_WIFI,
    TOPBAR_TARGET_VOLUME,
    TOPBAR_TARGET_BATTERY,
    TOPBAR_TARGET_POWER,
};

enum power_action {
    POWER_ACTION_NONE = -1,
    POWER_ACTION_RESTART,
    POWER_ACTION_SHUTDOWN,
    POWER_ACTION_LOGOUT,
    POWER_ACTION_CANCEL,
};

enum context_kind {
    CONTEXT_KIND_NONE,
    CONTEXT_KIND_DESKTOP,
    CONTEXT_KIND_FILES,
    CONTEXT_KIND_EDITOR,
};

enum context_action {
    CONTEXT_ACTION_NONE = -1,
    CONTEXT_ACTION_OPEN_FILE,
    CONTEXT_ACTION_NEW_FOLDER,
    CONTEXT_ACTION_NEW_TEXT_FILE,
    CONTEXT_ACTION_NEW_DOCUMENT,
    CONTEXT_ACTION_SAVE,
    CONTEXT_ACTION_OPEN_FILES,
    CONTEXT_ACTION_OPEN_TERMINAL,
    CONTEXT_ACTION_OPEN_TERMINAL_HERE,
    CONTEXT_ACTION_REFRESH,
    CONTEXT_ACTION_CLEAR_DOCUMENT,
    CONTEXT_ACTION_DUPLICATE,
    CONTEXT_ACTION_MOVE_TO_TRASH,
    CONTEXT_ACTION_DELETE_PERMANENTLY,
};

static const uint16_t app_default_width[APP_COUNT] = {
    800, 860, 720, 760, 780, 340, 820, 640, 820, 760, 840, 560, 900,
};

static const uint16_t app_default_height[APP_COUNT] = {
    540, 580, 460, 520, 540, 520, 560, 460, 560, 500, 580, 520, 600,
};

static const char* files_place_path(int place);
static int files_refresh(void);
static int files_current_is_trash(void);

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

static long syscall5(long n, long a, long b, long c, long d, long e) {
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8)
                     : "rcx", "r11", "memory");
    return ret;
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int cstrcmp(const char* a, const char* b) {
    uint64_t i = 0;

    if (!a) a = "";
    if (!b) b = "";
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
}

static int starts_with(const char* value, const char* prefix) {
    uint64_t i = 0;
    if (!value || !prefix) return 0;
    while (prefix[i]) {
        if (value[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static int ends_with(const char* value, const char* suffix) {
    uint64_t value_length = cstrlen(value);
    uint64_t suffix_length = cstrlen(suffix);

    if (suffix_length > value_length) return 0;
    return streq(value + value_length - suffix_length, suffix);
}

static void copy_cstr(char* dst, uint64_t dst_size, const char* src) {
    uint64_t i = 0;
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    while (i + 1 < dst_size && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void append_char(char* dst, uint64_t dst_size, char c) {
    uint64_t n = cstrlen(dst);
    if (!dst || dst_size == 0 || n + 1 >= dst_size) return;
    dst[n] = c;
    dst[n + 1] = 0;
}

static void append_cstr(char* dst, uint64_t dst_size, const char* src) {
    uint64_t n = cstrlen(dst);
    uint64_t i = 0;
    if (!dst || dst_size == 0 || !src) return;
    while (n + 1 < dst_size && src[i]) {
        dst[n++] = src[i++];
    }
    dst[n] = 0;
}

static int dec_to_int(const char* s) {
    int v = 0;
    int negative = 0;

    if (s && *s == '-') {
        negative = 1;
        s++;
    }
    while (s && *s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return negative ? -v : v;
}

static void write_buf(const char* s, uint64_t n) {
    uint64_t off = 0;
    while (off < n) {
        long rc = syscall3(SYS_WRITE, 1, (long)(s + off), (long)(n - off));
        if (rc <= 0) return;
        off += (uint64_t)rc;
    }
}

static int write_fd_all(int fd, const char* s, uint64_t n) {
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

static void exit_code(int code) {
    syscall1(SYS_EXIT, code);
    for (;;) {}
}

static uint32_t rgb_lerp(uint32_t a, uint32_t b, uint32_t num, uint32_t den) {
    uint32_t ar = (a >> 16) & 0xff;
    uint32_t ag = (a >> 8) & 0xff;
    uint32_t ab = a & 0xff;
    uint32_t br = (b >> 16) & 0xff;
    uint32_t bg = (b >> 8) & 0xff;
    uint32_t bb = b & 0xff;
    uint32_t r = ar + ((br > ar ? br - ar : 0) * num) / den;
    uint32_t g = ag + ((bg > ag ? bg - ag : 0) * num) / den;
    uint32_t bl = ab + ((bb > ab ? bb - ab : 0) * num) / den;
    if (br < ar) r = ar - ((ar - br) * num) / den;
    if (bg < ag) g = ag - ((ag - bg) * num) / den;
    if (bb < ab) bl = ab - ((ab - bb) * num) / den;
    return (r << 16) | (g << 8) | bl;
}

static void rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= (int)gfx.width || y >= (int)gfx.height) return;
    if (x + w > (int)gfx.width) w = (int)gfx.width - x;
    if (y + h > (int)gfx.height) h = (int)gfx.height - y;
    if (w <= 0 || h <= 0) return;
    syscall5(AOS_SYS_GFX_RECT, x, y, w, h, color);
}

static void round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    uint64_t style;
    if (w <= 0 || h <= 0) return;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    style = ((uint64_t)(r & 0xff) << 32) |
            ((uint64_t)255 << 24) |
            (uint64_t)(color & 0xffffffU);
    syscall5(AOS_SYS_GFX_BLEND_ROUND_RECT, x, y, w, h, (long)style);
}

static void panel(int x, int y, int w, int h, uint32_t fill, uint32_t border) {
    int r = h > 80 ? 14 : (h > 50 ? 12 : 8);
    round_rect(x + 5, y + 7, w, h, r, UI_SHADOW);
    round_rect(x, y, w, h, r, border);
    round_rect(x + 1, y + 1, w - 2, h - 2, r - 1, fill);
    rect(x + 6, y + 2, w - 12, 1, rgb_lerp(fill, 0xffffff, 1, 3));
}

static void flat_panel(int x, int y, int w, int h, uint32_t fill, uint32_t border) {
    int r = h > 40 ? 12 : 8;
    round_rect(x, y, w, h, r, border);
    round_rect(x + 1, y + 1, w - 2, h - 2, r - 1, fill);
    rect(x + 6, y + 1, w - 12, 1, rgb_lerp(fill, 0xffffff, 1, 4));
}

static void disc(int cx, int cy, int r, uint32_t color) {
    int rr = r * r;
    for (int y = -r; y <= r; y++) {
        int half = 0;
        while (half < r && half * half + y * y <= rr) half++;
        rect(cx - half + 1, cy + y, half * 2 - 1, 1, color);
    }
}

static int point_in(int px, int py, int x, int y, int w, int h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}

static int font_target_height(int scale) {
    static const uint8_t heights[] = {18, 18, 23, 31, 46, 58};
    if (scale < 1) scale = 1;
    if (scale > 5) scale = 5;
    return heights[scale];
}

static int font_glyph_index(char c) {
    unsigned int codepoint = (unsigned char)c;
    if (codepoint < MUI_FONT_FIRST ||
        codepoint >= MUI_FONT_FIRST + MUI_FONT_COUNT) {
        codepoint = '?';
    }
    return (int)(codepoint - MUI_FONT_FIRST);
}

static int glyph_advance(char c, int scale) {
    int target_height = font_target_height(scale);
    int advance = mui_font_advance[font_glyph_index(c)];
    return (advance * target_height + MUI_FONT_CELL_HEIGHT / 2) /
           MUI_FONT_CELL_HEIGHT;
}

static void draw_char(int x, int y, char c, uint32_t color, int scale) {
    int glyph = font_glyph_index(c);
    int target_height = font_target_height(scale);
    int target_width =
        (MUI_FONT_CELL_WIDTH * target_height + MUI_FONT_CELL_HEIGHT / 2) /
        MUI_FONT_CELL_HEIGHT;
    uint64_t source =
        ((uint64_t)MUI_FONT_CELL_WIDTH << 32) | MUI_FONT_CELL_HEIGHT;
    uint64_t position =
        ((uint64_t)(uint32_t)x << 32) |
        (uint32_t)(y - target_height / 6);
    uint64_t destination =
        ((uint64_t)(uint32_t)target_width << 32) |
        (uint32_t)target_height;

    if (c == ' ') return;
    syscall5(AOS_SYS_GFX_BLIT_ALPHA_MASK,
             (long)mui_font_alpha[glyph], (long)source,
             (long)position, (long)destination, color);
}

static int text_width(const char* s, int scale) {
    int width = 0;
    while (s && *s) {
        width += glyph_advance(*s, scale);
        s++;
    }
    return width;
}

static void draw_text(int x, int y, const char* s, uint32_t color, int scale) {
    int cx = x;
    while (s && *s) {
        draw_char(cx, y, *s, color, scale);
        cx += glyph_advance(*s, scale);
        s++;
    }
}

static void draw_text_center(int cx, int y, const char* s, uint32_t color, int scale) {
    draw_text(cx - text_width(s, scale) / 2, y, s, color, scale);
}

static void draw_text_right(int right, int y, const char* s, uint32_t color, int scale) {
    draw_text(right - text_width(s, scale), y, s, color, scale);
}

static void draw_text_center_ellipsized(int cx, int y, const char* s,
                                        int max_width, uint32_t color, int scale) {
    char shortened[64];
    int max_chars;
    int i;

    if (text_width(s, scale) <= max_width) {
        draw_text_center(cx, y, s, color, scale);
        return;
    }
    max_chars = max_width / glyph_advance('n', scale);
    if (max_chars < 4) return;
    if (max_chars > (int)sizeof(shortened) - 1) {
        max_chars = (int)sizeof(shortened) - 1;
    }
    for (i = 0; i < max_chars - 3 && s[i]; i++) shortened[i] = s[i];
    shortened[i++] = '.';
    shortened[i++] = '.';
    shortened[i++] = '.';
    shortened[i] = 0;
    draw_text_center(cx, y, shortened, color, scale);
}

static void draw_text_ellipsized(int x, int y, const char* s,
                                 int max_width, uint32_t color, int scale) {
    char shortened[96];
    int i = 0;

    if (text_width(s, scale) <= max_width) {
        draw_text(x, y, s, color, scale);
        return;
    }
    while (s[i] && i + 4 < (int)sizeof(shortened)) {
        shortened[i] = s[i];
        shortened[i + 1] = '.';
        shortened[i + 2] = '.';
        shortened[i + 3] = '.';
        shortened[i + 4] = 0;
        if (text_width(shortened, scale) > max_width) break;
        i++;
    }
    if (i < 1) return;
    i--;
    shortened[i++] = '.';
    shortened[i++] = '.';
    shortened[i++] = '.';
    shortened[i] = 0;
    draw_text(x, y, shortened, color, scale);
}

static const char* month_name(uint8_t month) {
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 1 || month > 12) return "Jan";
    return months[month - 1];
}

static const char* full_month_name(uint8_t month) {
    static const char* months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December",
    };
    if (month < 1 || month > 12) return "January";
    return months[month - 1];
}

static int month_days(int year, int month) {
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    int result;

    if (month < 1 || month > 12) return 30;
    result = days[month - 1];
    if (month == 2 &&
        ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        result = 29;
    }
    return result;
}

static int date_weekday(int year, int month, int day) {
    static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    if (month < 3) year--;
    return (year + year / 4 - year / 100 + year / 400 +
            offsets[month - 1] + day) % 7;
}

static const char* weekday_name(uint8_t weekday) {
    static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (weekday > 6) return "Sun";
    return days[weekday];
}

static void two_digit(char* out, uint8_t v) {
    out[0] = (char)('0' + (v / 10) % 10);
    out[1] = (char)('0' + (v % 10));
    out[2] = 0;
}

static void shifted_time(char* out, int minute_offset, int with_seconds) {
    int minutes = (int)time_info.hour * 60 + (int)time_info.minute + minute_offset;
    int hour;
    int minute;

    while (minutes < 0) minutes += 24 * 60;
    while (minutes >= 24 * 60) minutes -= 24 * 60;
    hour = minutes / 60;
    minute = minutes % 60;
    out[0] = (char)('0' + hour / 10);
    out[1] = (char)('0' + hour % 10);
    out[2] = ':';
    out[3] = (char)('0' + minute / 10);
    out[4] = (char)('0' + minute % 10);
    if (with_seconds) {
        out[5] = ':';
        out[6] = (char)('0' + time_info.second / 10);
        out[7] = (char)('0' + time_info.second % 10);
        out[8] = 0;
    } else {
        out[5] = 0;
    }
}

static char* u64_to_dec(uint64_t v) {
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
    return p;
}

static uint64_t percent(uint64_t used, uint64_t total) {
    if (!total) return 0;
    return (used * 100ULL) / total;
}

static const char* fallback(const char* s, const char* f) {
    return (s && s[0]) ? s : f;
}

#define AOS_MAIN_HOME "/main"
#define AOS_LEGACY_HOME "/home/explorer"
#define SETTINGS_CONFIG_PATH AOS_MAIN_HOME "/MUI/.mui.conf"
#define SETTINGS_LEGACY_CONFIG_PATH AOS_LEGACY_HOME "/.mui.conf"

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int settings_append_value(char* buffer, uint64_t capacity,
                                 const char* key, int value) {
    const char* digits;
    uint64_t needed;

    digits = u64_to_dec((uint64_t)(value < 0 ? 0 : value));
    needed = cstrlen(buffer) + cstrlen(key) + cstrlen(digits) + 3;
    if (needed > capacity) return -1;
    append_cstr(buffer, capacity, key);
    append_char(buffer, capacity, '=');
    append_cstr(buffer, capacity, digits);
    append_char(buffer, capacity, '\n');
    return 0;
}

static int settings_save_config(void) {
    char buffer[512];
    long fd;
    int failed;

    buffer[0] = 0;
    if (settings_append_value(buffer, sizeof(buffer), "version", 1) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "theme_dark",
                              settings_theme_dark) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "accent",
                              settings_accent) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "animations",
                              settings_animations) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "transparency",
                              settings_transparency) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "output_volume",
                              settings_volume) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "input_volume",
                              settings_input_volume) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "system_sounds",
                              settings_system_sounds) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "brightness",
                              settings_brightness) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "night_light",
                              settings_night_light) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "notifications",
                              settings_notifications) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "do_not_disturb",
                              settings_do_not_disturb) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "notification_banners",
                              settings_notification_banners) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "power_mode",
                              settings_power_mode) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "reduce_motion_saver",
                              settings_reduce_motion_saver) != 0 ||
        settings_append_value(buffer, sizeof(buffer), "remember_history",
                              settings_remember_history) != 0) {
        return -1;
    }

    fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)SETTINGS_CONFIG_PATH,
                  O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    failed = write_fd_all((int)fd, buffer, cstrlen(buffer));
    if (syscall1(SYS_CLOSE, fd) < 0) failed = -1;
    return failed;
}

static void settings_apply_config_value(const char* key, int value) {
    if (streq(key, "theme_dark")) {
        settings_theme_dark = value != 0;
    } else if (streq(key, "accent")) {
        settings_accent = clamp_int(value, 0, 3);
    } else if (streq(key, "animations")) {
        settings_animations = value != 0;
    } else if (streq(key, "transparency")) {
        settings_transparency = value != 0;
    } else if (streq(key, "output_volume")) {
        settings_volume = clamp_int(value, 0, 100);
    } else if (streq(key, "input_volume")) {
        settings_input_volume = clamp_int(value, 0, 100);
    } else if (streq(key, "system_sounds")) {
        settings_system_sounds = value != 0;
    } else if (streq(key, "brightness")) {
        settings_brightness = clamp_int(value, 10, 100);
    } else if (streq(key, "night_light")) {
        settings_night_light = value != 0;
    } else if (streq(key, "notifications")) {
        settings_notifications = value != 0;
    } else if (streq(key, "do_not_disturb")) {
        settings_do_not_disturb = value != 0;
    } else if (streq(key, "notification_banners")) {
        settings_notification_banners = value != 0;
    } else if (streq(key, "power_mode")) {
        settings_power_mode = clamp_int(value, 0, 2);
    } else if (streq(key, "reduce_motion_saver")) {
        settings_reduce_motion_saver = value != 0;
    } else if (streq(key, "remember_history")) {
        settings_remember_history = value != 0;
    }
}

static void settings_load_config(void) {
    char buffer[2048];
    long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)SETTINGS_CONFIG_PATH,
                       O_RDONLY, 0);
    uint64_t used = 0;
    int migrate_legacy = 0;

    if (fd < 0) {
        fd = syscall4(SYS_OPENAT, AT_FDCWD,
                      (long)SETTINGS_LEGACY_CONFIG_PATH, O_RDONLY, 0);
        migrate_legacy = fd >= 0;
    }
    if (fd < 0) return;
    while (used + 1 < sizeof(buffer)) {
        long got = syscall3(SYS_READ, fd, (long)(buffer + used),
                            (long)(sizeof(buffer) - used - 1));
        if (got <= 0) break;
        used += (uint64_t)got;
    }
    syscall1(SYS_CLOSE, fd);
    buffer[used] = 0;

    char* line = buffer;
    while (*line) {
        char* end = line;
        char* equals = 0;

        while (*end && *end != '\n' && *end != '\r') {
            if (*end == '=' && !equals) equals = end;
            end++;
        }
        if (equals) {
            *equals = 0;
            settings_apply_config_value(line, dec_to_int(equals + 1));
        }
        while (*end == '\n' || *end == '\r') end++;
        line = end;
    }
    if (migrate_legacy) {
        (void)settings_save_config();
    }
}

static void settings_notify(const char* message) {
    struct aos_uptime_info now = uptime_info;

    if (!settings_notifications || settings_do_not_disturb ||
        !settings_notification_banners) {
        settings_notice_until_second = 0;
        settings_notice_frames = 0;
        return;
    }
    if (syscall3(AOS_SYS_UPTIME_INFO, (long)&now, 0, 0) < 0 ||
        now.frequency == 0) {
        now.frequency = 100;
    }
    copy_cstr(settings_notice, sizeof(settings_notice), message);
    settings_notice_frames = 0;
    settings_notice_until_second = now.seconds + 4ULL;
}

static void settings_commit(const char* message) {
    settings_config_dirty = 1;
    if (settings_save_config() == 0) {
        settings_config_dirty = 0;
        settings_save_failed = 0;
        settings_notify(message ? message : "Settings saved");
    } else {
        settings_save_failed = 1;
    }
}

static int settings_motion_enabled(void) {
    return settings_animations &&
           !(settings_power_mode == 0 && settings_reduce_motion_saver);
}

static void sleep_for_ns(int64_t nanoseconds) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = nanoseconds;
    syscall2(SYS_NANOSLEEP, (long)&ts, 0);
}

static void sleep_frame(void) {
    if (settings_power_mode == 0) {
        sleep_for_ns(16000000);
    } else if (settings_power_mode == 2) {
        sleep_for_ns(4000000);
    } else {
        sleep_for_ns(8000000);
    }
}

static void sleep_interactive_frame(void) {
    sleep_for_ns(2000000);
}

static void fetch_system(void) {
    process_count = 0;
    visible_proc_count = 0;
    total_cpu_percent = 0;

    if (syscall3(AOS_SYS_MEM_INFO, (long)&mem_info, 0, 0) < 0) {
        mem_info.total = 0;
        mem_info.used = 0;
        mem_info.free = 0;
    }
    if (syscall3(AOS_SYS_UPTIME_INFO, (long)&uptime_info, 0, 0) < 0) {
        uptime_info.ticks = 0;
        uptime_info.seconds = 0;
        uptime_info.frequency = 100;
    }
    if (syscall3(AOS_SYS_TIME_INFO, (long)&time_info, 0, 0) < 0) {
        time_info.year = 2026;
        time_info.month = 1;
        time_info.day = 1;
        time_info.hour = (uint8_t)((uptime_info.seconds / 3600) % 24);
        time_info.minute = (uint8_t)((uptime_info.seconds / 60) % 60);
        time_info.second = (uint8_t)(uptime_info.seconds % 60);
        time_info.weekday = 0;
    }
    if (calendar_month == 0 || calendar_year == 0) {
        calendar_month = time_info.month ? time_info.month : 1;
        calendar_year = time_info.year ? time_info.year : 2026;
    }
    if (calendar_selected == 0) {
        calendar_selected = time_info.day ? time_info.day : 1;
    }
    if (syscall3(AOS_SYS_BLUETOOTH_INFO, (long)&bt_info, 0, 0) < 0) {
        bt_info.present = 0;
        bt_info.hci_ready = 0;
        bt_info.discovered_count = 0;
        bt_info.status[0] = 0;
    }
    if (syscall3(AOS_SYS_THERMAL_INFO, (long)&thermal_info, 0, 0) < 0) {
        thermal_info.sensor_present = 0;
        thermal_info.temp_known = 0;
        thermal_info.emergency_active = 0;
        thermal_info.hardware_throttling = 0;
        thermal_info.critical_active = 0;
        thermal_info.throttled_processes = 0;
    } else {
        if (thermal_state_initialized &&
            thermal_last_emergency != thermal_info.emergency_active) {
            if (thermal_info.emergency_active) {
                settings_notify("High temperature: background tasks limited");
            } else {
                settings_notify("Temperature recovered: limits removed");
            }
        } else if (!thermal_state_initialized &&
                   thermal_info.emergency_active) {
            settings_notify("High temperature: background tasks limited");
        }
        thermal_last_emergency = thermal_info.emergency_active;
        thermal_state_initialized = 1;
    }
    if (syscall3(AOS_SYS_CPU_INFO, 0, (long)&cpu_info, 0) >= 0) {
        uint32_t count = cpu_info.core_count ? cpu_info.core_count : 1;
        if (count > 16) count = 16;
        for (uint32_t i = 0; i < count; i++) {
            struct aos_cpu_info one;
            if (syscall3(AOS_SYS_CPU_INFO, i, (long)&one, 0) >= 0) {
                total_cpu_percent += one.load_known ? one.load_percent : ((uptime_info.ticks / 4 + i * 19) % 100);
            }
        }
        total_cpu_percent /= count;
    }

    for (uint64_t i = 0; i < MAX_PROCESSES; i++) {
        long rc = syscall3(AOS_SYS_PROCESS_INFO, (long)i, (long)&proc_info, 0);
        if (rc < 0 || !proc_info.valid) continue;
        process_count++;
        if (visible_proc_count < MAX_VISIBLE_PROCESSES) {
            visible_procs[visible_proc_count++] = proc_info;
        }
    }
}

static void draw_bar(int x, int y, int w, int h, uint64_t value, uint32_t fill) {
    int inner = w - 2;
    int filled;
    if (value > 100) value = 100;
    filled = (int)((uint64_t)inner * value / 100ULL);
    rect(x, y, w, h, 0xdfe6d8);
    rect(x + 1, y + 1, inner, h - 2, 0xcfd8c8);
    rect(x + 1, y + 1, filled, h - 2, fill);
}

static void alpha_rect(int x, int y, int w, int h, uint32_t color, uint32_t alpha, int step) {
    uint64_t style;
    (void)step;
    if (w <= 0 || h <= 0 || alpha == 0) return;
    style = ((uint64_t)(alpha & 0xffU) << 24) | (uint64_t)(color & 0xffffffU);
    syscall5(AOS_SYS_GFX_BLEND_ROUND_RECT, x, y, w, h, (long)style);
}

static void alpha_round_rect(int x, int y, int w, int h, int r, uint32_t color, uint32_t alpha, int step) {
    uint64_t style;
    (void)step;
    if (w <= 0 || h <= 0 || alpha == 0) return;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    style = ((uint64_t)(r & 0xff) << 32) |
            ((uint64_t)(alpha & 0xffU) << 24) |
            (uint64_t)(color & 0xffffffU);
    syscall5(AOS_SYS_GFX_BLEND_ROUND_RECT, x, y, w, h, (long)style);
}

static void glass_panel(int x, int y, int w, int h, int r, uint32_t tint, uint32_t alpha, uint32_t border) {
    alpha_round_rect(x + 5, y + 8, w, h, r, 0x000000, 86, 8);
    alpha_round_rect(x, y, w, h, r, border, 96, 6);
    alpha_round_rect(x + 1, y + 1, w - 2, h - 2, r - 1, tint, alpha, 6);
    alpha_rect(x + 8, y + 2, w - 16, 1, 0xffffff, 120, 8);
}

static void draw_wallpaper(void) {
    syscall3(AOS_SYS_GFX_BLIT_RGB565, (long)mui_wallpaper_rgb565,
             MUI_WALLPAPER_W, MUI_WALLPAPER_H);
}

static void draw_wallpaper_rect(int x, int y, int w, int h) {
    uint64_t source;
    uint64_t position;
    uint64_t destination;

    if (w <= 0 || h <= 0) return;
    source = ((uint64_t)MUI_WALLPAPER_W << 32) | MUI_WALLPAPER_H;
    position = ((uint64_t)(uint32_t)x << 32) | (uint32_t)y;
    destination = ((uint64_t)(uint32_t)w << 32) | (uint32_t)h;
    syscall5(AOS_SYS_GFX_BLIT_RGB565_RECT, (long)mui_wallpaper_rgb565,
             (long)source, (long)position, (long)destination, 0);
}

static void draw_circle_outline(int cx, int cy, int radius, uint32_t color) {
    int x = radius;
    int y = 0;
    int error = 1 - radius;

    while (x >= y) {
        rect(cx + x, cy + y, 1, 1, color);
        rect(cx + y, cy + x, 1, 1, color);
        rect(cx - y, cy + x, 1, 1, color);
        rect(cx - x, cy + y, 1, 1, color);
        rect(cx - x, cy - y, 1, 1, color);
        rect(cx - y, cy - x, 1, 1, color);
        rect(cx + y, cy - x, 1, 1, color);
        rect(cx + x, cy - y, 1, 1, color);
        y++;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

static void draw_search_icon(int cx, int cy, uint32_t color) {
    draw_circle_outline(cx - 2, cy - 2, 5, color);
    rect(cx + 2, cy + 2, 2, 2, color);
    rect(cx + 4, cy + 4, 2, 2, color);
}

static void draw_wifi_icon(int cx, int cy, uint32_t color) {
    rect(cx - 2, cy - 7, 5, 1, color);
    rect(cx - 6, cy - 6, 4, 1, color);
    rect(cx + 3, cy - 6, 4, 1, color);
    rect(cx - 8, cy - 4, 3, 2, color);
    rect(cx + 6, cy - 4, 3, 2, color);
    rect(cx - 2, cy - 2, 5, 1, color);
    rect(cx - 5, cy - 1, 3, 1, color);
    rect(cx + 3, cy - 1, 3, 1, color);
    rect(cx - 2, cy + 2, 5, 1, color);
    disc(cx, cy + 6, 1, color);
}

static void draw_volume_icon(int cx, int cy, uint32_t color) {
    rect(cx - 8, cy - 2, 3, 5, color);
    rect(cx - 5, cy - 4, 2, 9, color);
    rect(cx - 3, cy - 6, 2, 13, color);
    if (settings_volume <= 0) {
        for (int i = 0; i < 7; i++) {
            rect(cx + 1 + i, cy - 3 + i, 1, 1, color);
            rect(cx + 7 - i, cy - 3 + i, 1, 1, color);
        }
        return;
    }
    rect(cx + 2, cy - 3, 1, 7, color);
    if (settings_volume >= 34) rect(cx + 4, cy - 5, 1, 11, color);
    if (settings_volume >= 67) rect(cx + 6, cy - 7, 1, 15, color);
}

static void draw_battery_icon(int cx, int cy, uint32_t color) {
    rect(cx - 8, cy - 5, 14, 1, color);
    rect(cx - 8, cy + 4, 14, 1, color);
    rect(cx - 8, cy - 4, 1, 8, color);
    rect(cx + 5, cy - 4, 1, 8, color);
    rect(cx + 6, cy - 2, 2, 4, color);
    rect(cx - 5, cy - 2, 8, 5, color);
}

static void draw_power_icon(int cx, int cy, uint32_t color) {
    draw_circle_outline(cx, cy + 1, 7, color);
    rect(cx, cy - 8, 1, 9, color);
}

static int topbar_target_at(int x, int y) {
    int w = (int)gfx.width;

    if (y < 0 || y >= TOPBAR_HEIGHT || x < 0 || x >= w) {
        return TOPBAR_TARGET_NONE;
    }
    if (x < 128) return TOPBAR_TARGET_ACTIVITIES;
    if (x >= w - 40) return TOPBAR_TARGET_POWER;
    if (x >= w - 72) return TOPBAR_TARGET_BATTERY;
    if (x >= w - 104) return TOPBAR_TARGET_VOLUME;
    if (x >= w - 138) return TOPBAR_TARGET_WIFI;
    if (x >= w - 176) return TOPBAR_TARGET_SEARCH;
    if (point_in(x, y, w / 2 - 120, 0, 240, TOPBAR_HEIGHT)) {
        return TOPBAR_TARGET_CLOCK;
    }
    return TOPBAR_TARGET_NONE;
}

static void draw_topbar(void) {
    int w = (int)gfx.width;
    char hh[3];
    char mm[3];
    char date[40];
    uint8_t display_hour = (uint8_t)(time_info.hour % 12);

    if (display_hour == 0) display_hour = 12;
    two_digit(hh, display_hour);
    two_digit(mm, time_info.minute);
    copy_cstr(date, sizeof(date), weekday_name(time_info.weekday));
    append_cstr(date, sizeof(date), ", ");
    append_cstr(date, sizeof(date), month_name(time_info.month));
    append_cstr(date, sizeof(date), " ");
    append_cstr(date, sizeof(date), u64_to_dec(time_info.day ? time_info.day : 1));
    append_cstr(date, sizeof(date), "  ");
    append_cstr(date, sizeof(date), hh);
    append_cstr(date, sizeof(date), ":");
    append_cstr(date, sizeof(date), mm);
    append_cstr(date, sizeof(date), time_info.hour >= 12 ? " PM" : " AM");

    alpha_rect(0, 0, w, TOPBAR_HEIGHT,
               settings_theme_dark ? 0x131b17 : 0x20271f,
               settings_transparency ? 176 : 255, 1);
    alpha_rect(0, TOPBAR_HEIGHT - 1, w, 1, 0xffffff, 18, 1);
    if (hover_topbar_target == TOPBAR_TARGET_ACTIVITIES) {
        alpha_round_rect(12, 4, 108, 28, 8, 0xffffff, 28, 1);
    }
    if (hover_topbar_target == TOPBAR_TARGET_CLOCK) {
        alpha_round_rect(w / 2 - 120, 4, 240, 28, 8, 0xffffff, 24, 1);
    }
    if (hover_topbar_target == TOPBAR_TARGET_SEARCH) {
        alpha_round_rect(w - 174, 4, 36, 28, 8, 0xffffff, 28, 1);
    }
    if (hover_topbar_target == TOPBAR_TARGET_WIFI) {
        alpha_round_rect(w - 138, 4, 34, 28, 8, 0xffffff, 28, 1);
    }
    if (hover_topbar_target == TOPBAR_TARGET_VOLUME) {
        alpha_round_rect(w - 104, 4, 32, 28, 8, 0xffffff, 28, 1);
    }
    if (hover_topbar_target == TOPBAR_TARGET_BATTERY) {
        alpha_round_rect(w - 72, 4, 32, 28, 8, 0xffffff, 28, 1);
    }
    if (hover_topbar_target == TOPBAR_TARGET_POWER || power_menu_open) {
        alpha_round_rect(w - 40, 4, 36, 28, 8, 0xffffff,
                         power_menu_open ? 42 : 28, 1);
    }
    round_rect(22, 10, 16, 16, 3, UI_ACCENT);
    draw_text(27, 11, "A", UI_PANEL_TEXT, 1);
    draw_text(46, 11, "Activities", UI_PANEL_TEXT, 1);
    draw_text_center(w / 2, 11, date, UI_PANEL_TEXT, 1);

    draw_search_icon(w - 155, 18, UI_PANEL_TEXT);
    draw_wifi_icon(w - 119, 18, UI_PANEL_TEXT);
    draw_volume_icon(w - 87, 18, UI_PANEL_TEXT);
    draw_battery_icon(w - 56, 18, UI_PANEL_TEXT);
    draw_power_icon(w - 22, 18, UI_PANEL_TEXT);
}

static void stroke_line(int x0, int y0, int x1, int y1, int thickness, uint32_t color) {
    uint64_t style;

    if (thickness < 1) thickness = 1;
    if (thickness > 255) thickness = 255;
    style = ((uint64_t)(uint32_t)thickness << 24) | (color & 0xffffffU);
    syscall5(AOS_SYS_GFX_AA_LINE, x0, y0, x1, y1, (long)style);
}

static void circle_stroke(int cx, int cy, int radius, int thickness, uint32_t color) {
    if (radius < 1) radius = 1;
    if (thickness < 1) thickness = 1;
    syscall5(AOS_SYS_GFX_AA_CIRCLE, cx, cy, radius, thickness, color);
}

static void power_menu_bounds(int* x, int* y, int* w, int* h) {
    *w = POWER_MENU_WIDTH;
    *h = session_status.installed && session_status.authenticated
             ? POWER_MENU_HEIGHT : 222;
    *x = (int)gfx.width - *w - 12;
    *y = TOPBAR_HEIGHT + 8;
    if (*x < 8) *x = 8;
}

static int power_action_at(int px, int py) {
    int x;
    int y;
    int w;
    int h;

    power_menu_bounds(&x, &y, &w, &h);
    (void)h;
    if (point_in(px, py, x + 10, y + 61, w - 20, 54)) {
        return POWER_ACTION_RESTART;
    }
    if (point_in(px, py, x + 10, y + 121, w - 20, 54)) {
        return POWER_ACTION_SHUTDOWN;
    }
    if (session_status.installed && session_status.authenticated &&
        point_in(px, py, x + 10, y + 181, w - 20, 54)) {
        return POWER_ACTION_LOGOUT;
    }
    if (point_in(px, py, x + 10,
                 y + (session_status.installed && session_status.authenticated
                          ? 244 : 184),
                 w - 20, 28)) {
        return POWER_ACTION_CANCEL;
    }
    return POWER_ACTION_NONE;
}

static void draw_restart_icon(int cx, int cy, uint32_t color) {
    circle_stroke(cx, cy, 8, 2, color);
    stroke_line(cx + 2, cy - 9, cx + 8, cy - 9, 2, color);
    stroke_line(cx + 8, cy - 9, cx + 8, cy - 3, 2, color);
}

static void draw_cancel_icon(int cx, int cy, uint32_t color) {
    stroke_line(cx - 5, cy - 5, cx + 5, cy + 5, 2, color);
    stroke_line(cx + 5, cy - 5, cx - 5, cy + 5, 2, color);
}

static void draw_power_menu(void) {
    int x;
    int y;
    int w;
    int h;
    uint32_t restart_fill;
    uint32_t shutdown_fill;
    uint32_t logout_fill;
    uint32_t cancel_fill;
    int show_logout;
    int cancel_y;

    if (!power_menu_open) return;
    power_menu_bounds(&x, &y, &w, &h);
    show_logout = session_status.installed && session_status.authenticated;
    cancel_y = y + (show_logout ? 244 : 184);
    glass_panel(x, y, w, h, 14, UI_GLASS_LIGHT,
                settings_transparency ? 246 : 255, UI_BORDER);

    draw_power_icon(x + 23, y + 23, UI_ACCENT);
    draw_text(x + 42, y + 13, "System", UI_TEXT, 2);
    draw_text(x + 14, y + 39,
              power_action_failed
                  ? "Action was not accepted"
                  : "Choose a system action",
              power_action_failed ? UI_WARNING : UI_MUTED, 1);
    rect(x + 12, y + 55, w - 24, 1, UI_BORDER);

    restart_fill = hover_power_action == POWER_ACTION_RESTART
                       ? rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 5)
                       : UI_CARD_ALT;
    shutdown_fill = hover_power_action == POWER_ACTION_SHUTDOWN
                        ? rgb_lerp(UI_CARD_ALT, 0xe6a28a, 1, 3)
                        : UI_CARD_ALT;
    logout_fill = hover_power_action == POWER_ACTION_LOGOUT
                      ? rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 5)
                      : UI_CARD_ALT;
    cancel_fill = hover_power_action == POWER_ACTION_CANCEL
                      ? rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 6)
                      : UI_GLASS_LIGHT;

    flat_panel(x + 10, y + 61, w - 20, 54, restart_fill, UI_BORDER);
    draw_restart_icon(x + 30, y + 88, UI_ACCENT);
    draw_text(x + 51, y + 72, "Restart", UI_TEXT, 1);
    draw_text(x + 51, y + 92, "Boot AOS again", UI_MUTED, 1);

    flat_panel(x + 10, y + 121, w - 20, 54, shutdown_fill, UI_BORDER);
    draw_power_icon(x + 30, y + 148, UI_WARNING);
    draw_text(x + 51, y + 132, "Power Off", UI_TEXT, 1);
    draw_text(x + 51, y + 152, "Turn this computer off", UI_MUTED, 1);

    if (show_logout) {
        flat_panel(x + 10, y + 181, w - 20, 54, logout_fill, UI_BORDER);
        disc(x + 30, y + 202, 4, UI_ACCENT);
        round_rect(x + 24, y + 208, 13, 8, 5, UI_ACCENT);
        draw_text(x + 51, y + 192, "Sign Out", UI_TEXT, 1);
        draw_text(x + 51, y + 212, "Return to the login screen", UI_MUTED, 1);
    }

    flat_panel(x + 10, cancel_y, w - 20, 28, cancel_fill, UI_BORDER);
    draw_cancel_icon(x + 28, cancel_y + 14, UI_MUTED);
    draw_text_center(x + w / 2 + 4, cancel_y + 7, "Cancel", UI_TEXT, 1);
}

static int icon_coord(int center, int size, int value) {
    return center - size / 2 + (value * size) / 24;
}

static void icon_line(int cx, int cy, int size,
                      int x0, int y0, int x1, int y1,
                      int thickness, uint32_t color) {
    stroke_line(icon_coord(cx, size, x0), icon_coord(cy, size, y0),
                icon_coord(cx, size, x1), icon_coord(cy, size, y1),
                thickness, color);
}

static void icon_circle(int cx, int cy, int size, int x, int y, int radius,
                        int thickness, uint32_t color) {
    int scaled_radius = (radius * size) / 24;
    circle_stroke(icon_coord(cx, size, x), icon_coord(cy, size, y),
                  scaled_radius, thickness, color);
}

static void icon_dot(int cx, int cy, int size, int x, int y, int radius,
                     uint32_t color) {
    int scaled_radius = (radius * size) / 24;
    if (scaled_radius < 1) scaled_radius = 1;
    syscall5(AOS_SYS_GFX_AA_CIRCLE,
             icon_coord(cx, size, x), icon_coord(cy, size, y),
             scaled_radius, 0, color);
}

static void icon_round_box(int cx, int cy, int size,
                           int x0, int y0, int x1, int y1,
                           int thickness, uint32_t color) {
    int left = icon_coord(cx, size, x0);
    int top = icon_coord(cy, size, y0);
    int right = icon_coord(cx, size, x1);
    int bottom = icon_coord(cy, size, y1);
    int radius = size / 12;
    if (radius < 1) radius = 1;

    stroke_line(left + radius, top, right - radius, top, thickness, color);
    stroke_line(right, top + radius, right, bottom - radius, thickness, color);
    stroke_line(right - radius, bottom, left + radius, bottom, thickness, color);
    stroke_line(left, bottom - radius, left, top + radius, thickness, color);
    stroke_line(left, top + radius, left + radius, top, thickness, color);
    stroke_line(right - radius, top, right, top + radius, thickness, color);
    stroke_line(right, bottom - radius, right - radius, bottom, thickness, color);
    stroke_line(left + radius, bottom, left, bottom - radius, thickness, color);
}

static void draw_app_symbol(int cx, int cy, int app, uint32_t color, int size) {
    int stroke = size >= 20 ? 2 : 1;

    switch (app) {
        case 0: /* Folder */
            icon_line(cx, cy, size, 3, 8, 9, 8, stroke, color);
            icon_line(cx, cy, size, 9, 8, 11, 10, stroke, color);
            icon_line(cx, cy, size, 11, 10, 21, 10, stroke, color);
            icon_line(cx, cy, size, 21, 10, 21, 19, stroke, color);
            icon_line(cx, cy, size, 21, 19, 3, 19, stroke, color);
            icon_line(cx, cy, size, 3, 19, 3, 8, stroke, color);
            break;
        case 1: { /* Settings */
            static const uint8_t gear[][2] = {
                {10, 2}, {14, 2}, {15, 5}, {18, 4}, {20, 6}, {19, 9},
                {22, 10}, {22, 14}, {19, 15}, {20, 18}, {18, 20}, {15, 19},
                {14, 22}, {10, 22}, {9, 19}, {6, 20}, {4, 18}, {5, 15},
                {2, 14}, {2, 10}, {5, 9}, {4, 6}, {6, 4}, {9, 5},
            };
            int count = (int)(sizeof(gear) / sizeof(gear[0]));
            for (int i = 0; i < count; i++) {
                int next = (i + 1) % count;
                icon_line(cx, cy, size, gear[i][0], gear[i][1],
                          gear[next][0], gear[next][1], stroke, color);
            }
            icon_circle(cx, cy, size, 12, 12, 3, stroke, color);
            break;
        }
        case 2: /* TerminalSquare */
            icon_round_box(cx, cy, size, 3, 4, 21, 20, stroke, color);
            icon_line(cx, cy, size, 7, 9, 10, 12, stroke, color);
            icon_line(cx, cy, size, 10, 12, 7, 15, stroke, color);
            icon_line(cx, cy, size, 13, 15, 17, 15, stroke, color);
            break;
        case 3: /* FileText */
            icon_line(cx, cy, size, 6, 2, 14, 2, stroke, color);
            icon_line(cx, cy, size, 14, 2, 20, 8, stroke, color);
            icon_line(cx, cy, size, 20, 8, 20, 22, stroke, color);
            icon_line(cx, cy, size, 20, 22, 6, 22, stroke, color);
            icon_line(cx, cy, size, 6, 22, 6, 2, stroke, color);
            icon_line(cx, cy, size, 14, 2, 14, 8, stroke, color);
            icon_line(cx, cy, size, 14, 8, 20, 8, stroke, color);
            icon_line(cx, cy, size, 9, 13, 17, 13, stroke, color);
            icon_line(cx, cy, size, 9, 17, 17, 17, stroke, color);
            break;
        case 4: /* Activity */
            icon_line(cx, cy, size, 2, 12, 6, 12, stroke, color);
            icon_line(cx, cy, size, 6, 12, 9, 4, stroke, color);
            icon_line(cx, cy, size, 9, 4, 14, 20, stroke, color);
            icon_line(cx, cy, size, 14, 20, 17, 12, stroke, color);
            icon_line(cx, cy, size, 17, 12, 22, 12, stroke, color);
            break;
        case 5: /* Calculator */
            icon_round_box(cx, cy, size, 4, 2, 20, 22, stroke, color);
            icon_round_box(cx, cy, size, 7, 5, 17, 9, stroke, color);
            for (int row = 0; row < 2; row++) {
                for (int col = 0; col < 3; col++) {
                    icon_dot(cx, cy, size, 8 + col * 4, 14 + row * 4, 1, color);
                }
            }
            break;
        case 6: /* CalendarDays */
            icon_round_box(cx, cy, size, 3, 5, 21, 21, stroke, color);
            icon_line(cx, cy, size, 3, 10, 21, 10, stroke, color);
            icon_line(cx, cy, size, 8, 3, 8, 7, stroke, color);
            icon_line(cx, cy, size, 16, 3, 16, 7, stroke, color);
            icon_dot(cx, cy, size, 8, 14, 1, color);
            icon_dot(cx, cy, size, 12, 14, 1, color);
            icon_dot(cx, cy, size, 16, 14, 1, color);
            icon_dot(cx, cy, size, 8, 18, 1, color);
            icon_dot(cx, cy, size, 12, 18, 1, color);
            break;
        case 7: /* Clock */
            icon_circle(cx, cy, size, 12, 12, 9, stroke, color);
            icon_line(cx, cy, size, 12, 7, 12, 12, stroke, color);
            icon_line(cx, cy, size, 12, 12, 16, 14, stroke, color);
            break;
        case 8: /* Image */
            icon_round_box(cx, cy, size, 3, 4, 21, 20, stroke, color);
            icon_circle(cx, cy, size, 9, 10, 2, stroke, color);
            icon_line(cx, cy, size, 3, 18, 9, 13, stroke, color);
            icon_line(cx, cy, size, 9, 13, 13, 17, stroke, color);
            icon_line(cx, cy, size, 13, 17, 16, 14, stroke, color);
            icon_line(cx, cy, size, 16, 14, 21, 19, stroke, color);
            break;
        case 9: /* HardDrive */
            icon_round_box(cx, cy, size, 3, 5, 21, 19, stroke, color);
            icon_line(cx, cy, size, 3, 14, 21, 14, stroke, color);
            icon_dot(cx, cy, size, 7, 17, 1, color);
            icon_dot(cx, cy, size, 11, 17, 1, color);
            break;
        case 10: /* Package */
            icon_line(cx, cy, size, 12, 3, 21, 8, stroke, color);
            icon_line(cx, cy, size, 21, 8, 12, 13, stroke, color);
            icon_line(cx, cy, size, 12, 13, 3, 8, stroke, color);
            icon_line(cx, cy, size, 3, 8, 12, 3, stroke, color);
            icon_line(cx, cy, size, 3, 8, 3, 17, stroke, color);
            icon_line(cx, cy, size, 3, 17, 12, 22, stroke, color);
            icon_line(cx, cy, size, 12, 22, 21, 17, stroke, color);
            icon_line(cx, cy, size, 21, 17, 21, 8, stroke, color);
            icon_line(cx, cy, size, 12, 13, 12, 22, stroke, color);
            icon_line(cx, cy, size, 7, 5, 16, 10, stroke, color);
            break;
        case 11: /* Info */
            icon_circle(cx, cy, size, 12, 12, 9, stroke, color);
            icon_dot(cx, cy, size, 12, 7, 1, color);
            icon_line(cx, cy, size, 12, 11, 12, 17, stroke, color);
            break;
        case 12: /* Install to disk */
            icon_round_box(cx, cy, size, 3, 12, 21, 21, stroke, color);
            icon_dot(cx, cy, size, 7, 17, 1, color);
            icon_line(cx, cy, size, 12, 3, 12, 13, stroke, color);
            icon_line(cx, cy, size, 8, 9, 12, 13, stroke, color);
            icon_line(cx, cy, size, 16, 9, 12, 13, stroke, color);
            break;
        default:
            icon_round_box(cx, cy, size, 4, 4, 20, 20, stroke, color);
            break;
    }
}

enum ui_symbol {
    UI_SYM_HOME,
    UI_SYM_GRID,
    UI_SYM_DOCUMENT,
    UI_SYM_DOWNLOAD,
    UI_SYM_IMAGE,
    UI_SYM_MUSIC,
    UI_SYM_STAR,
    UI_SYM_TRASH,
    UI_SYM_DISK,
    UI_SYM_WIFI,
    UI_SYM_BLUETOOTH,
    UI_SYM_MONITOR,
    UI_SYM_PALETTE,
    UI_SYM_VOLUME,
    UI_SYM_BELL,
    UI_SYM_BATTERY,
    UI_SYM_LOCK,
    UI_SYM_GLOBE,
    UI_SYM_USER,
    UI_SYM_SEARCH,
    UI_SYM_CHEVRON,
    UI_SYM_UP,
    UI_SYM_LIST,
    UI_SYM_CPU,
    UI_SYM_TEMPERATURE,
    UI_SYM_MEMORY,
    UI_SYM_ACTIVITY,
    UI_SYM_SAVE,
    UI_SYM_TYPE,
    UI_SYM_CHECK,
};

static void draw_ui_symbol(int cx, int cy, enum ui_symbol symbol,
                           uint32_t color, int size) {
    int stroke = size >= 20 ? 2 : 1;

    switch (symbol) {
        case UI_SYM_HOME:
            icon_line(cx, cy, size, 3, 11, 12, 4, stroke, color);
            icon_line(cx, cy, size, 12, 4, 21, 11, stroke, color);
            icon_line(cx, cy, size, 6, 10, 6, 21, stroke, color);
            icon_line(cx, cy, size, 6, 21, 18, 21, stroke, color);
            icon_line(cx, cy, size, 18, 21, 18, 10, stroke, color);
            icon_line(cx, cy, size, 10, 21, 10, 15, stroke, color);
            icon_line(cx, cy, size, 10, 15, 14, 15, stroke, color);
            icon_line(cx, cy, size, 14, 15, 14, 21, stroke, color);
            break;
        case UI_SYM_GRID:
            icon_round_box(cx, cy, size, 4, 4, 10, 10, stroke, color);
            icon_round_box(cx, cy, size, 14, 4, 20, 10, stroke, color);
            icon_round_box(cx, cy, size, 4, 14, 10, 20, stroke, color);
            icon_round_box(cx, cy, size, 14, 14, 20, 20, stroke, color);
            break;
        case UI_SYM_DOCUMENT:
            icon_round_box(cx, cy, size, 6, 3, 18, 21, stroke, color);
            icon_line(cx, cy, size, 13, 3, 18, 8, stroke, color);
            icon_line(cx, cy, size, 13, 3, 13, 8, stroke, color);
            icon_line(cx, cy, size, 13, 8, 18, 8, stroke, color);
            icon_line(cx, cy, size, 9, 13, 15, 13, stroke, color);
            icon_line(cx, cy, size, 9, 17, 15, 17, stroke, color);
            break;
        case UI_SYM_DOWNLOAD:
            icon_line(cx, cy, size, 12, 3, 12, 15, stroke, color);
            icon_line(cx, cy, size, 7, 11, 12, 16, stroke, color);
            icon_line(cx, cy, size, 17, 11, 12, 16, stroke, color);
            icon_line(cx, cy, size, 5, 18, 5, 21, stroke, color);
            icon_line(cx, cy, size, 5, 21, 19, 21, stroke, color);
            icon_line(cx, cy, size, 19, 21, 19, 18, stroke, color);
            break;
        case UI_SYM_IMAGE:
            icon_round_box(cx, cy, size, 3, 4, 21, 20, stroke, color);
            icon_circle(cx, cy, size, 8, 9, 2, stroke, color);
            icon_line(cx, cy, size, 5, 18, 10, 13, stroke, color);
            icon_line(cx, cy, size, 10, 13, 14, 17, stroke, color);
            icon_line(cx, cy, size, 14, 17, 17, 14, stroke, color);
            icon_line(cx, cy, size, 17, 14, 21, 18, stroke, color);
            break;
        case UI_SYM_MUSIC:
            icon_line(cx, cy, size, 10, 6, 19, 4, stroke, color);
            icon_line(cx, cy, size, 10, 6, 10, 18, stroke, color);
            icon_line(cx, cy, size, 19, 4, 19, 16, stroke, color);
            icon_circle(cx, cy, size, 7, 18, 3, 0, color);
            icon_circle(cx, cy, size, 16, 16, 3, 0, color);
            break;
        case UI_SYM_STAR:
            icon_line(cx, cy, size, 12, 3, 15, 9, stroke, color);
            icon_line(cx, cy, size, 15, 9, 21, 10, stroke, color);
            icon_line(cx, cy, size, 21, 10, 17, 15, stroke, color);
            icon_line(cx, cy, size, 17, 15, 18, 21, stroke, color);
            icon_line(cx, cy, size, 18, 21, 12, 18, stroke, color);
            icon_line(cx, cy, size, 12, 18, 6, 21, stroke, color);
            icon_line(cx, cy, size, 6, 21, 7, 15, stroke, color);
            icon_line(cx, cy, size, 7, 15, 3, 10, stroke, color);
            icon_line(cx, cy, size, 3, 10, 9, 9, stroke, color);
            icon_line(cx, cy, size, 9, 9, 12, 3, stroke, color);
            break;
        case UI_SYM_TRASH:
            icon_round_box(cx, cy, size, 6, 8, 18, 21, stroke, color);
            icon_line(cx, cy, size, 4, 7, 20, 7, stroke, color);
            icon_line(cx, cy, size, 9, 4, 15, 4, stroke, color);
            icon_line(cx, cy, size, 10, 11, 10, 18, stroke, color);
            icon_line(cx, cy, size, 14, 11, 14, 18, stroke, color);
            break;
        case UI_SYM_DISK:
            icon_round_box(cx, cy, size, 3, 6, 21, 19, stroke, color);
            icon_line(cx, cy, size, 5, 15, 19, 15, stroke, color);
            icon_dot(cx, cy, size, 17, 11, 1, color);
            break;
        case UI_SYM_WIFI:
            icon_line(cx, cy, size, 3, 9, 7, 6, stroke, color);
            icon_line(cx, cy, size, 7, 6, 12, 5, stroke, color);
            icon_line(cx, cy, size, 12, 5, 17, 6, stroke, color);
            icon_line(cx, cy, size, 17, 6, 21, 9, stroke, color);
            icon_line(cx, cy, size, 7, 13, 9, 11, stroke, color);
            icon_line(cx, cy, size, 9, 11, 12, 10, stroke, color);
            icon_line(cx, cy, size, 12, 10, 15, 11, stroke, color);
            icon_line(cx, cy, size, 15, 11, 17, 13, stroke, color);
            icon_line(cx, cy, size, 10, 17, 12, 15, stroke, color);
            icon_line(cx, cy, size, 12, 15, 14, 17, stroke, color);
            icon_dot(cx, cy, size, 12, 20, 1, color);
            break;
        case UI_SYM_BLUETOOTH:
            icon_line(cx, cy, size, 10, 3, 17, 9, stroke, color);
            icon_line(cx, cy, size, 17, 9, 10, 15, stroke, color);
            icon_line(cx, cy, size, 10, 15, 17, 21, stroke, color);
            icon_line(cx, cy, size, 17, 21, 17, 9, stroke, color);
            icon_line(cx, cy, size, 5, 7, 14, 17, stroke, color);
            icon_line(cx, cy, size, 5, 17, 14, 7, stroke, color);
            break;
        case UI_SYM_MONITOR:
            icon_round_box(cx, cy, size, 3, 4, 21, 17, stroke, color);
            icon_line(cx, cy, size, 12, 17, 12, 21, stroke, color);
            icon_line(cx, cy, size, 8, 21, 16, 21, stroke, color);
            break;
        case UI_SYM_PALETTE:
            icon_circle(cx, cy, size, 12, 12, 9, stroke, color);
            icon_circle(cx, cy, size, 16, 16, 3, stroke, color);
            icon_dot(cx, cy, size, 8, 9, 1, color);
            icon_dot(cx, cy, size, 12, 7, 1, color);
            icon_dot(cx, cy, size, 16, 10, 1, color);
            break;
        case UI_SYM_VOLUME:
            icon_line(cx, cy, size, 4, 10, 8, 10, stroke, color);
            icon_line(cx, cy, size, 8, 10, 13, 6, stroke, color);
            icon_line(cx, cy, size, 13, 6, 13, 18, stroke, color);
            icon_line(cx, cy, size, 13, 18, 8, 14, stroke, color);
            icon_line(cx, cy, size, 8, 14, 4, 14, stroke, color);
            icon_line(cx, cy, size, 16, 9, 18, 12, stroke, color);
            icon_line(cx, cy, size, 18, 12, 16, 15, stroke, color);
            break;
        case UI_SYM_BELL:
            icon_line(cx, cy, size, 6, 17, 18, 17, stroke, color);
            icon_line(cx, cy, size, 7, 17, 8, 10, stroke, color);
            icon_line(cx, cy, size, 8, 10, 10, 7, stroke, color);
            icon_line(cx, cy, size, 10, 7, 14, 7, stroke, color);
            icon_line(cx, cy, size, 14, 7, 16, 10, stroke, color);
            icon_line(cx, cy, size, 16, 10, 17, 17, stroke, color);
            icon_line(cx, cy, size, 10, 20, 14, 20, stroke, color);
            break;
        case UI_SYM_BATTERY:
            icon_round_box(cx, cy, size, 4, 7, 19, 18, stroke, color);
            icon_line(cx, cy, size, 19, 10, 21, 10, stroke, color);
            icon_line(cx, cy, size, 21, 10, 21, 15, stroke, color);
            icon_line(cx, cy, size, 21, 15, 19, 15, stroke, color);
            icon_line(cx, cy, size, 11, 9, 8, 13, stroke, color);
            icon_line(cx, cy, size, 8, 13, 12, 13, stroke, color);
            icon_line(cx, cy, size, 12, 13, 10, 17, stroke, color);
            break;
        case UI_SYM_LOCK:
            icon_round_box(cx, cy, size, 5, 10, 19, 21, stroke, color);
            icon_circle(cx, cy, size, 12, 10, 5, stroke, color);
            icon_line(cx, cy, size, 12, 14, 12, 18, stroke, color);
            break;
        case UI_SYM_GLOBE:
            icon_circle(cx, cy, size, 12, 12, 9, stroke, color);
            icon_line(cx, cy, size, 3, 12, 21, 12, stroke, color);
            icon_line(cx, cy, size, 12, 3, 12, 21, stroke, color);
            icon_line(cx, cy, size, 7, 5, 5, 12, stroke, color);
            icon_line(cx, cy, size, 5, 12, 7, 19, stroke, color);
            icon_line(cx, cy, size, 17, 5, 19, 12, stroke, color);
            icon_line(cx, cy, size, 19, 12, 17, 19, stroke, color);
            break;
        case UI_SYM_USER:
            icon_circle(cx, cy, size, 12, 8, 4, stroke, color);
            icon_line(cx, cy, size, 5, 21, 6, 17, stroke, color);
            icon_line(cx, cy, size, 6, 17, 9, 14, stroke, color);
            icon_line(cx, cy, size, 9, 14, 15, 14, stroke, color);
            icon_line(cx, cy, size, 15, 14, 18, 17, stroke, color);
            icon_line(cx, cy, size, 18, 17, 19, 21, stroke, color);
            break;
        case UI_SYM_SEARCH:
            icon_circle(cx, cy, size, 10, 10, 6, stroke, color);
            icon_line(cx, cy, size, 14, 14, 20, 20, stroke, color);
            break;
        case UI_SYM_CHEVRON:
            icon_line(cx, cy, size, 9, 6, 15, 12, stroke, color);
            icon_line(cx, cy, size, 15, 12, 9, 18, stroke, color);
            break;
        case UI_SYM_UP:
            icon_line(cx, cy, size, 6, 14, 12, 8, stroke, color);
            icon_line(cx, cy, size, 12, 8, 18, 14, stroke, color);
            icon_line(cx, cy, size, 12, 8, 12, 20, stroke, color);
            break;
        case UI_SYM_LIST:
            for (int row = 0; row < 3; row++) {
                icon_dot(cx, cy, size, 5, 7 + row * 5, 1, color);
                icon_line(cx, cy, size, 9, 7 + row * 5,
                          20, 7 + row * 5, stroke, color);
            }
            break;
        case UI_SYM_CPU:
            icon_round_box(cx, cy, size, 6, 6, 18, 18, stroke, color);
            icon_round_box(cx, cy, size, 9, 9, 15, 15, stroke, color);
            for (int p = 8; p <= 16; p += 4) {
                icon_line(cx, cy, size, p, 3, p, 6, stroke, color);
                icon_line(cx, cy, size, p, 18, p, 21, stroke, color);
                icon_line(cx, cy, size, 3, p, 6, p, stroke, color);
                icon_line(cx, cy, size, 18, p, 21, p, stroke, color);
            }
            break;
        case UI_SYM_TEMPERATURE:
            icon_circle(cx, cy, size, 12, 18, 4, stroke, color);
            icon_line(cx, cy, size, 12, 5, 12, 18, stroke, color);
            icon_line(cx, cy, size, 12, 5, 16, 5, stroke, color);
            icon_line(cx, cy, size, 16, 5, 16, 15, stroke, color);
            icon_line(cx, cy, size, 16, 15, 15, 17, stroke, color);
            icon_line(cx, cy, size, 12, 11, 12, 19, 2, color);
            break;
        case UI_SYM_MEMORY:
            icon_round_box(cx, cy, size, 4, 7, 20, 17, stroke, color);
            for (int p = 7; p <= 17; p += 5) {
                icon_line(cx, cy, size, p, 9, p, 15, stroke, color);
            }
            icon_line(cx, cy, size, 6, 17, 6, 21, stroke, color);
            icon_line(cx, cy, size, 10, 17, 10, 21, stroke, color);
            icon_line(cx, cy, size, 14, 17, 14, 21, stroke, color);
            icon_line(cx, cy, size, 18, 17, 18, 21, stroke, color);
            break;
        case UI_SYM_ACTIVITY:
            icon_line(cx, cy, size, 2, 13, 7, 13, stroke, color);
            icon_line(cx, cy, size, 7, 13, 10, 5, stroke, color);
            icon_line(cx, cy, size, 10, 5, 14, 19, stroke, color);
            icon_line(cx, cy, size, 14, 19, 17, 11, stroke, color);
            icon_line(cx, cy, size, 17, 11, 22, 11, stroke, color);
            break;
        case UI_SYM_SAVE:
            icon_round_box(cx, cy, size, 4, 3, 20, 21, stroke, color);
            icon_round_box(cx, cy, size, 8, 4, 16, 10, stroke, color);
            icon_round_box(cx, cy, size, 7, 14, 17, 20, stroke, color);
            break;
        case UI_SYM_TYPE:
            icon_line(cx, cy, size, 4, 5, 20, 5, stroke, color);
            icon_line(cx, cy, size, 12, 5, 12, 20, stroke, color);
            icon_line(cx, cy, size, 8, 20, 16, 20, stroke, color);
            break;
        case UI_SYM_CHECK:
            icon_line(cx, cy, size, 4, 12, 9, 17, stroke, color);
            icon_line(cx, cy, size, 9, 17, 20, 6, stroke, color);
            break;
    }
}

static int context_actions(int* actions, int capacity) {
    int count = 0;

#define ADD_CONTEXT_ACTION(action) \
    do { \
        if (count < capacity) actions[count] = (action); \
        count++; \
    } while (0)

    if (context_menu_kind == CONTEXT_KIND_DESKTOP) {
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_NEW_DOCUMENT);
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_OPEN_FILES);
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_OPEN_TERMINAL);
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_REFRESH);
    } else if (context_menu_kind == CONTEXT_KIND_FILES) {
        if (files_selected >= 0 &&
            files_selected < file_entry_count) {
            ADD_CONTEXT_ACTION(CONTEXT_ACTION_OPEN_FILE);
            if (!file_entries[files_selected].is_dir) {
                ADD_CONTEXT_ACTION(CONTEXT_ACTION_DUPLICATE);
                ADD_CONTEXT_ACTION(files_current_is_trash()
                    ? CONTEXT_ACTION_DELETE_PERMANENTLY
                    : CONTEXT_ACTION_MOVE_TO_TRASH);
            } else if (files_current_is_trash()) {
                ADD_CONTEXT_ACTION(CONTEXT_ACTION_DELETE_PERMANENTLY);
            }
        }
        if (files_current_path[0]) {
            ADD_CONTEXT_ACTION(CONTEXT_ACTION_NEW_FOLDER);
            ADD_CONTEXT_ACTION(CONTEXT_ACTION_NEW_TEXT_FILE);
            ADD_CONTEXT_ACTION(CONTEXT_ACTION_OPEN_TERMINAL_HERE);
        }
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_REFRESH);
    } else if (context_menu_kind == CONTEXT_KIND_EDITOR) {
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_SAVE);
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_NEW_DOCUMENT);
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_OPEN_FILES);
        ADD_CONTEXT_ACTION(CONTEXT_ACTION_CLEAR_DOCUMENT);
    }

#undef ADD_CONTEXT_ACTION
    return count > capacity ? capacity : count;
}

static const char* context_action_label(int action) {
    switch (action) {
        case CONTEXT_ACTION_OPEN_FILE:
            return files_selected >= 0 &&
                   files_selected < file_entry_count &&
                   file_entries[files_selected].is_dir
                       ? "Open Folder" : "Open in Text Editor";
        case CONTEXT_ACTION_NEW_FOLDER: return "New Folder";
        case CONTEXT_ACTION_NEW_TEXT_FILE: return "New Text File";
        case CONTEXT_ACTION_NEW_DOCUMENT: return "New Document";
        case CONTEXT_ACTION_SAVE: return "Save";
        case CONTEXT_ACTION_OPEN_FILES: return "Open Files";
        case CONTEXT_ACTION_OPEN_TERMINAL: return "Open Terminal";
        case CONTEXT_ACTION_OPEN_TERMINAL_HERE: return "Open Terminal Here";
        case CONTEXT_ACTION_REFRESH: return "Refresh";
        case CONTEXT_ACTION_CLEAR_DOCUMENT: return "Clear Document";
        case CONTEXT_ACTION_DUPLICATE: return "Duplicate";
        case CONTEXT_ACTION_MOVE_TO_TRASH: return "Move to Trash";
        case CONTEXT_ACTION_DELETE_PERMANENTLY: return "Delete Permanently";
        default: return "";
    }
}

static int context_menu_height(void) {
    int actions[8];
    return context_actions(actions, 8) * CONTEXT_MENU_ROW_HEIGHT + 12;
}

static int context_action_at(int px, int py) {
    int actions[8];
    int count = context_actions(actions, 8);

    for (int i = 0; i < count; i++) {
        if (point_in(px, py, context_menu_x + 6,
                     context_menu_y + 6 + i * CONTEXT_MENU_ROW_HEIGHT,
                     CONTEXT_MENU_WIDTH - 12,
                     CONTEXT_MENU_ROW_HEIGHT - 2)) {
            return actions[i];
        }
    }
    return CONTEXT_ACTION_NONE;
}

static void open_context_menu(int kind, int x, int y) {
    int h;

    context_menu_kind = kind;
    h = context_menu_height();
    if (h <= 12) {
        context_menu_open = 0;
        context_menu_kind = CONTEXT_KIND_NONE;
        return;
    }
    if (x + CONTEXT_MENU_WIDTH > (int)gfx.width - 8) {
        x = (int)gfx.width - CONTEXT_MENU_WIDTH - 8;
    }
    if (y + h > (int)gfx.height - 8) {
        y = (int)gfx.height - h - 8;
    }
    if (x < 8) x = 8;
    if (y < TOPBAR_HEIGHT + 4) y = TOPBAR_HEIGHT + 4;
    context_menu_x = x;
    context_menu_y = y;
    context_menu_open = 1;
    hover_context_action = CONTEXT_ACTION_NONE;
    power_menu_open = 0;
}

static void draw_context_action_icon(int action, int cx, int cy,
                                     uint32_t color) {
    switch (action) {
        case CONTEXT_ACTION_OPEN_FILE:
        case CONTEXT_ACTION_NEW_TEXT_FILE:
        case CONTEXT_ACTION_NEW_DOCUMENT:
            draw_ui_symbol(cx, cy, UI_SYM_DOCUMENT, color, 17);
            break;
        case CONTEXT_ACTION_NEW_FOLDER:
        case CONTEXT_ACTION_OPEN_FILES:
            draw_app_symbol(cx, cy, 0, color, 18);
            break;
        case CONTEXT_ACTION_OPEN_TERMINAL:
        case CONTEXT_ACTION_OPEN_TERMINAL_HERE:
            draw_app_symbol(cx, cy, 2, color, 18);
            break;
        case CONTEXT_ACTION_SAVE:
            draw_ui_symbol(cx, cy, UI_SYM_SAVE, color, 17);
            break;
        case CONTEXT_ACTION_REFRESH:
            draw_restart_icon(cx, cy, color);
            break;
        case CONTEXT_ACTION_CLEAR_DOCUMENT:
        case CONTEXT_ACTION_MOVE_TO_TRASH:
        case CONTEXT_ACTION_DELETE_PERMANENTLY:
            draw_ui_symbol(cx, cy, UI_SYM_TRASH, color, 17);
            break;
        case CONTEXT_ACTION_DUPLICATE:
            draw_ui_symbol(cx, cy, UI_SYM_DOCUMENT, color, 17);
            break;
        default:
            break;
    }
}

static void draw_context_menu(void) {
    int actions[8];
    int count;
    int h;

    if (!context_menu_open) return;
    count = context_actions(actions, 8);
    h = context_menu_height();
    glass_panel(context_menu_x, context_menu_y, CONTEXT_MENU_WIDTH, h,
                12, UI_GLASS_LIGHT,
                settings_transparency ? 246 : 255, UI_BORDER);

    for (int i = 0; i < count; i++) {
        int row_y =
            context_menu_y + 6 + i * CONTEXT_MENU_ROW_HEIGHT;
        uint32_t color =
            actions[i] == CONTEXT_ACTION_CLEAR_DOCUMENT
                ? UI_WARNING
                : UI_TEXT;
        if (hover_context_action == actions[i]) {
            round_rect(context_menu_x + 6, row_y,
                       CONTEXT_MENU_WIDTH - 12,
                       CONTEXT_MENU_ROW_HEIGHT - 2, 8,
                       actions[i] == CONTEXT_ACTION_CLEAR_DOCUMENT
                           ? rgb_lerp(UI_CARD_ALT, 0xe6a28a, 1, 3)
                           : rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 6));
        }
        draw_context_action_icon(actions[i], context_menu_x + 24,
                                 row_y + 19, color);
        draw_text(context_menu_x + 43, row_y + 13,
                  context_action_label(actions[i]), color, 1);
    }
}

static void draw_tile(int x, int y, int size, int app, int selected) {
    uint32_t fill = selected ? rgb_lerp(apps[app].tile_a, 0xffffff, 1, 7) : apps[app].tile_a;
    int r = size >= 58 ? 16 : 13;
    int symbol_size = size >= 58 ? 32 : 24;
    alpha_round_rect(x + 2, y + 4, size, size, r, 0x000000, 58, 1);
    round_rect(x, y, size, size, r, fill);
    draw_app_symbol(x + size / 2, y + size / 2, app, 0xffffff, symbol_size);
}

static void draw_file_folder(int x, int y, int scale) {
    draw_app_symbol(x + 16 * scale, y + 16 * scale, 0,
                    0xc99240, 32 * scale);
}

static void draw_file_doc(int x, int y, int scale) {
    draw_app_symbol(x + 16 * scale, y + 16 * scale, 3,
                    0x607064, 32 * scale);
}

static void draw_desktop_icons(void) {
    int y = 54;
    int x = 32;
    for (int i = 0; i < APP_COUNT; i++) {
        if (!apps[i].desktop || !app_is_available(i)) continue;
        if ((window_open && active_app == i && !overview_open) || hover_desktop_app == i) {
            alpha_round_rect(x - 16, y - 8, 84, 82, 16, hover_desktop_app == i ? 0x615a4a : 0x464639, 130, 6);
        }
        draw_tile(x, y, 48, i,
                  (window_open && active_app == i && !overview_open) ||
                  hover_desktop_app == i);
        draw_text_center(x + 24, y + 58, apps[i].name, UI_PANEL_TEXT, 1);
        y += 86;
    }
}

static void draw_dock(void) {
    int w = (int)gfx.width;
    int h = (int)gfx.height;
    int pinned = 0;
    int dock_w;
    int dock_y;
    int x;
    for (int i = 0; i < APP_COUNT; i++)
        if (apps[i].pinned && app_is_available(i)) pinned++;
    dock_w = pinned * DOCK_ITEM_STEP + 88;
    dock_y = h - DOCK_BOTTOM - DOCK_HEIGHT;
    x = (w - dock_w) / 2;
    alpha_round_rect(x + 4, dock_y + 8, dock_w, DOCK_HEIGHT, 20, 0x000000, 92, 1);
    alpha_round_rect(x, dock_y, dock_w, DOCK_HEIGHT, 20, 0xffffff, 28, 1);
    alpha_round_rect(x + 1, dock_y + 1, dock_w - 2, DOCK_HEIGHT - 2,
                     19, settings_theme_dark ? 0x18231d : 0x273027,
                     settings_transparency ? 148 : 255, 1);
    alpha_rect(x + 18, dock_y + 2, dock_w - 36, 1, 0xffffff, 34, 1);

    int cx = x + 10;
    for (int i = 0; i < APP_COUNT; i++) {
        if (!apps[i].pinned || !app_is_available(i)) continue;
        int lift = hover_dock_app == i && settings_motion_enabled() ? 5 : 0;
        if (hover_dock_app == i) {
            int tip_w = text_width(apps[i].name, 1) + 16;
            alpha_round_rect(cx + DOCK_TILE_SIZE / 2 - tip_w / 2, dock_y - 30,
                             tip_w, 22, 6, 0x20271f, 220, 1);
            draw_text_center(cx + DOCK_TILE_SIZE / 2, dock_y - 23,
                             apps[i].name, UI_PANEL_TEXT, 1);
        }
        draw_tile(cx, dock_y + 8 - lift, DOCK_TILE_SIZE, i,
                  active_app == i || hover_dock_app == i);
        if (window_open && active_app == i) {
            disc(cx + DOCK_TILE_SIZE / 2, dock_y + DOCK_HEIGHT - 7, 2, UI_PANEL_TEXT);
        }
        cx += DOCK_ITEM_STEP;
    }
    alpha_rect(cx + 3, dock_y + 13, 1, 48, 0xffffff, 40, 1);
    cx += 18;
    if (hover_all_apps) {
        alpha_round_rect(cx - 1, dock_y - 30, 62, 22, 6, 0x20271f, 220, 1);
        draw_text_center(cx + DOCK_TILE_SIZE / 2, dock_y - 23,
                         "All Apps", UI_PANEL_TEXT, 1);
    }
    int all_y = dock_y + 8 -
                (hover_all_apps && settings_motion_enabled() ? 5 : 0);
    alpha_round_rect(cx + 3, all_y + 5, DOCK_TILE_SIZE, DOCK_TILE_SIZE,
                     13, 0x000000, 72, 1);
    alpha_round_rect(cx, all_y, DOCK_TILE_SIZE, DOCK_TILE_SIZE, 13,
                     0xffffff, overview_open || hover_all_apps ? 48 : 25, 1);
    for (int gy = 0; gy < 3; gy++) {
        for (int gx = 0; gx < 3; gx++) {
            round_rect(cx + 10 + gx * 9, all_y + 10 + gy * 9, 5, 5, 1, UI_PANEL_TEXT);
        }
    }
    if (overview_open) {
        disc(cx + DOCK_TILE_SIZE / 2, dock_y + DOCK_HEIGHT - 7, 2, UI_PANEL_TEXT);
    }
}

static void draw_window_shell(int x, int y, int w, int h, const char* title) {
    alpha_round_rect(x + 8, y + 10, w, h, 18, 0x000000, 72, 8);
    glass_panel(x, y, w, h, 18, UI_GLASS_LIGHT,
                settings_transparency ? 238 : 255, UI_BORDER);
    rect(x + 1, y + 34, w - 2, 1, UI_BORDER);
    alpha_round_rect(x + 1, y + 1, w - 2, 34, 17, UI_WINDOW_HEADER,
                     settings_transparency ? 235 : 255, 6);
    draw_app_symbol(x + 18, y + 17, active_app, UI_ICON_GREEN, 16);
    draw_text(x + 34, y + 12, title, UI_TEXT, 1);
    if (hover_close) {
        alpha_round_rect(x + w - 29, y + 6, 24, 24, 12, 0xc35b30, 210, 1);
    }
    stroke_line(x + w - 81, y + 18, x + w - 73, y + 18, 1, UI_MUTED);
    icon_round_box(x + w - 47, y + 18, 12, 4, 4, 20, 20, 1, UI_MUTED);
    stroke_line(x + w - 21, y + 14, x + w - 13, y + 22, 1,
                hover_close ? 0xffffff : UI_MUTED);
    stroke_line(x + w - 13, y + 14, x + w - 21, y + 22, 1,
                hover_close ? 0xffffff : UI_MUTED);
}

static void draw_gauge(int x, int y, int w, const char* label, uint64_t value,
                       uint32_t color, const char* sub, enum ui_symbol symbol) {
    flat_panel(x, y, w, 120, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(x + 18, y + 18, symbol, UI_MUTED, 14);
    draw_text(x + 32, y + 14, label, UI_MUTED, 1);
    draw_text(x + 14, y + 39, u64_to_dec(value), UI_TEXT, 3);
    draw_text(x + 14 + text_width(u64_to_dec(value), 3) + 6,
              y + 50, "%", UI_MUTED, 2);
    draw_bar(x + 14, y + 82, w - 28, 7, value, color);
    if (sub) draw_text(x + 14, y + 101, sub, UI_MUTED, 1);
}

static void draw_temperature_gauge(int x, int y, int w) {
    char value[16];
    char sub[64];
    uint64_t bar = 0;
    uint32_t color = UI_ACCENT;
    int32_t temp = thermal_info.maximum_celsius;
    int32_t scale = thermal_info.tjmax_celsius;

    flat_panel(x, y, w, 120, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(x + 18, y + 18, UI_SYM_TEMPERATURE, UI_MUTED, 14);
    draw_text(x + 32, y + 14, "CPU Temperature", UI_MUTED, 1);

    if (!thermal_info.sensor_present || !thermal_info.temp_known) {
        draw_text(x + 14, y + 39, "--", UI_TEXT, 3);
        draw_bar(x + 14, y + 82, w - 28, 7, 0, UI_MUTED);
        draw_text(x + 14, y + 101, "Sensor unavailable", UI_MUTED, 1);
        return;
    }

    if (temp < 0) temp = 0;
    if (scale <= 0) scale = 120;
    bar = (uint64_t)temp * 100ULL / (uint64_t)scale;
    if (bar > 100) bar = 100;

    if (thermal_info.emergency_active || thermal_info.critical_active) {
        color = 0xcf4a4a;
        copy_cstr(sub, sizeof(sub), "Emergency limiting active");
    } else if (thermal_info.hardware_throttling) {
        color = 0xd17831;
        copy_cstr(sub, sizeof(sub), "Hardware throttling active");
    } else {
        if (thermal_info.emergency_celsius > 0 &&
            temp >= thermal_info.emergency_celsius - 10) {
            color = UI_WARNING;
        }
        copy_cstr(sub, sizeof(sub), "Limit ");
        append_cstr(sub, sizeof(sub),
                    u64_to_dec((uint64_t)thermal_info.emergency_celsius));
        append_cstr(sub, sizeof(sub), " C");
    }

    copy_cstr(value, sizeof(value), u64_to_dec((uint64_t)temp));
    draw_text(x + 14, y + 39, value, UI_TEXT, 3);
    draw_text(x + 14 + text_width(value, 3) + 6,
              y + 50, "C", UI_MUTED, 2);
    draw_bar(x + 14, y + 82, w - 28, 7, bar, color);
    draw_text(x + 14, y + 101, sub, color, 1);
}

static void draw_system_monitor(int x, int y, int w, int h) {
    static char ram_sub[32];
    static char thermal_summary[64];
    int gy = y + 52;
    int gw = (w - 52) / 3;
    int section_y = gy + 140;
    uint64_t ram = percent(mem_info.used, mem_info.total);
    copy_cstr(ram_sub, sizeof(ram_sub), u64_to_dec(mem_info.used / (1024ULL * 1024ULL)));
    append_cstr(ram_sub, sizeof(ram_sub), " / ");
    append_cstr(ram_sub, sizeof(ram_sub), u64_to_dec(mem_info.total / (1024ULL * 1024ULL)));
    append_cstr(ram_sub, sizeof(ram_sub), " MiB");

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    draw_gauge(x + 14, gy, gw, "CPU", total_cpu_percent,
               UI_WARNING, "All kernel cores", UI_SYM_CPU);
    draw_gauge(x + 26 + gw, gy, gw, "Memory", ram,
               UI_ACCENT, ram_sub, UI_SYM_MEMORY);
    draw_temperature_gauge(x + 38 + gw * 2, gy, gw);

    draw_ui_symbol(x + 20, section_y + 11, UI_SYM_ACTIVITY, UI_ACCENT, 16);
    draw_text(x + 34, section_y + 8, "Processes", UI_TEXT, 2);
    if (thermal_info.emergency_active) {
        copy_cstr(thermal_summary, sizeof(thermal_summary),
                  u64_to_dec(thermal_info.throttled_processes));
        append_cstr(thermal_summary, sizeof(thermal_summary),
                    " background tasks limited");
        draw_text_right(x + w - 18, section_y + 10,
                        thermal_summary, 0xcf4a4a, 1);
    }
    draw_text(x + 18, section_y + 38, "Name", UI_MUTED, 1);
    draw_text_right(x + w - 168, section_y + 38, "User", UI_MUTED, 1);
    draw_text_right(x + w - 88, section_y + 38, "CPU", UI_MUTED, 1);
    draw_text_right(x + w - 22, section_y + 38, "Thermal", UI_MUTED, 1);
    for (uint64_t i = 0; i < visible_proc_count; i++) {
        int row = section_y + 58 + (int)i * 31;
        uint64_t cpu = uptime_info.ticks ? (visible_procs[i].cpu_ticks * 100ULL) / uptime_info.ticks : 0;
        if (cpu > 100) cpu = 100;
        if (i & 1) round_rect(x + 10, row - 7, w - 20, 27, 6, UI_CARD_ALT);
        draw_text(x + 18, row, fallback(visible_procs[i].command, "-"), UI_TEXT, 1);
        draw_text_right(x + w - 168, row,
                        fallback(visible_procs[i].username, "-"), UI_MUTED, 1);
        draw_text_right(x + w - 88, row, u64_to_dec(cpu),
                        cpu > 3 ? 0xb58b33 : UI_MUTED, 1);
        draw_text_right(x + w - 22, row,
                        visible_procs[i].thermal_throttled ? "Limited" : "Normal",
                        visible_procs[i].thermal_throttled ? 0xcf4a4a : UI_MUTED,
                        1);
    }
}

static uint32_t settings_accent_color(void) {
    static const uint32_t colors[] = {
        0x238c50, 0xcd604c, 0x3f7fa8, 0xb650ae,
    };
    int index = settings_accent;
    if (index < 0 || index >= 4) index = 0;
    return colors[index];
}

static uint32_t settings_text_color(void) {
    return settings_theme_dark ? 0xf0f5ef : UI_TEXT;
}

static uint32_t settings_muted_color(void) {
    return settings_theme_dark ? 0xaab7ad : UI_MUTED;
}

static uint32_t settings_surface_color(void) {
    return settings_theme_dark ? 0x25322b : UI_CARD;
}

static uint32_t settings_sidebar_color(void) {
    return settings_theme_dark ? 0x202c26 : UI_CARD_ALT;
}

static uint32_t settings_border_color(void) {
    return settings_theme_dark ? 0x46564b : UI_BORDER;
}

static void settings_content_geometry(int x, int w,
                                      int* content_x, int* content_w) {
    int sidebar_w = 240;
    int area_w = w - sidebar_w;
    int inner_w = area_w - 48;

    if (inner_w > 576) inner_w = 576;
    if (inner_w < 280) inner_w = 280;
    *content_x = x + sidebar_w + (area_w - inner_w) / 2;
    *content_w = inner_w;
}

static void draw_settings_card(int x, int y, int w, int h) {
    uint32_t alpha = settings_transparency ? 218U : 255U;
    alpha_round_rect(x + 1, y + 3, w, h, 12, 0x172019, 24, 1);
    alpha_round_rect(x, y, w, h, 12, settings_border_color(), 190, 1);
    alpha_round_rect(x + 1, y + 1, w - 2, h - 2, 11,
                     settings_surface_color(), alpha, 1);
}

static void draw_settings_row_text(int x, int y, int w,
                                   const char* title, const char* desc,
                                   int divider) {
    int title_y = y + (desc ? 20 : 27);
    draw_text(x + 16, title_y, title, settings_text_color(), 1);
    if (desc) {
        draw_text(x + 16, y + 39, desc, settings_muted_color(), 1);
    }
    if (divider) {
        alpha_rect(x + 16, y + 63, w - 32, 1,
                   settings_border_color(), 150, 1);
    }
}

static void draw_settings_switch(int x, int y, int on) {
    uint32_t track = on ? settings_accent_color() : settings_border_color();
    alpha_round_rect(x, y, 36, 20, 10, track, on ? 255 : 210, 1);
    round_rect(x + (on ? 18 : 2), y + 2, 16, 16, 8, 0xffffff);
}

static void draw_settings_slider(int x, int y, int w, int value) {
    int fill;
    int knob;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    fill = w * value / 100;
    knob = x + (w - 14) * value / 100;
    round_rect(x, y + 6, w, 4, 2, settings_border_color());
    if (fill > 0) {
        round_rect(x, y + 6, fill, 4, 2, settings_accent_color());
    }
    circle_stroke(knob + 7, y + 8, 8, 1, settings_border_color());
    icon_dot(knob + 7, y + 8, 24, 12, 12, 7, 0xffffff);
}

static void draw_settings_choice(int x, int y, int w,
                                 const char* label, int selected) {
    uint32_t border = selected ? settings_accent_color()
                               : settings_border_color();
    uint32_t fill = selected ? settings_accent_color()
                             : settings_surface_color();
    alpha_round_rect(x, y, w, 30, 9, border, 255, 1);
    alpha_round_rect(x + 1, y + 1, w - 2, 28, 8, fill,
                     selected ? 255 : 205, 1);
    draw_text_center(x + w / 2, y + 11, label,
                     selected ? 0xffffff : settings_text_color(), 1);
}

static void draw_settings(int x, int y, int w, int h) {
    static char resolution[24];
    static char volume_text[12];
    const char* rows[] = {
        "Wi-Fi", "Bluetooth", "Displays", "Appearance", "Sound",
        "Notifications", "Power", "Privacy & Security", "Network", "Users",
    };
    const char* subs[10];
    const enum ui_symbol symbols[] = {
        UI_SYM_WIFI, UI_SYM_BLUETOOTH, UI_SYM_MONITOR, UI_SYM_PALETTE,
        UI_SYM_VOLUME, UI_SYM_BELL, UI_SYM_BATTERY, UI_SYM_LOCK,
        UI_SYM_GLOBE, UI_SYM_USER,
    };
    uint32_t accent = settings_accent_color();
    uint32_t text = settings_text_color();
    uint32_t muted = settings_muted_color();
    uint32_t body_alpha = settings_transparency ? 232U : 255U;
    int sidebar_w = 240;
    int content_x;
    int card_w;
    const char* power_mode = settings_power_mode == 0 ? "Saver"
                             : settings_power_mode == 2 ? "Performance"
                             : "Balanced";

    if (settings_section < 0 || settings_section >= 10) settings_section = 3;
    copy_cstr(resolution, sizeof(resolution), u64_to_dec(gfx.width));
    append_cstr(resolution, sizeof(resolution), " x ");
    append_cstr(resolution, sizeof(resolution), u64_to_dec(gfx.height));
    copy_cstr(volume_text, sizeof(volume_text), u64_to_dec(settings_volume));
    append_cstr(volume_text, sizeof(volume_text), "%");
    subs[0] = settings_wifi ? "Mountain-Net" : "Off";
    subs[1] = settings_feature_enabled[1] ? "On" : "Off";
    subs[2] = resolution;
    subs[3] = settings_theme_dark ? "Dark" : "Forest";
    subs[4] = volume_text;
    subs[5] = !settings_notifications ? "Off"
              : settings_do_not_disturb ? "Quiet" : "Allowed";
    subs[6] = power_mode;
    subs[7] = settings_remember_history ? "Protected" : "No history";
    subs[8] = "Kernel";
    subs[9] = session_status.username[0] ? session_status.username : "Local";
    settings_content_geometry(x, w, &content_x, &card_w);

    alpha_round_rect(x + 1, y + 35, w - 2, h - 36, 16,
                     settings_surface_color(), body_alpha, 1);
    alpha_round_rect(x + 1, y + 35, sidebar_w - 1, h - 36, 16,
                     settings_sidebar_color(), body_alpha, 1);
    alpha_rect(x + sidebar_w, y + 37, 1, h - 54,
               settings_border_color(), 170, 1);

    alpha_round_rect(x + 13, y + 49, sidebar_w - 26, 38, 10,
                     settings_border_color(), 190, 1);
    alpha_round_rect(x + 14, y + 50, sidebar_w - 28, 36, 9,
                     settings_surface_color(), 190, 1);
    draw_ui_symbol(x + 28, y + 68, UI_SYM_SEARCH, muted, 14);
    draw_text(x + 43, y + 64, "Search", muted, 1);

    for (int i = 0; i < 10; i++) {
        int ry = y + 96 + i * 39;
        int sub_w = text_width(subs[i], 1);
        int label_w = sidebar_w - 65 - sub_w;
        uint32_t row_text = i == settings_section ? 0xffffff : text;
        uint32_t row_sub = i == settings_section ? 0xdff2e6 : muted;
        if (i == settings_section) {
            alpha_round_rect(x + 12, ry, sidebar_w - 24, 36, 9,
                             accent, 255, 1);
        }
        draw_ui_symbol(x + 29, ry + 18, symbols[i], row_text, 16);
        if (label_w < 36) label_w = 36;
        draw_text_ellipsized(x + 45, ry + 13, rows[i], label_w,
                             row_text, 1);
        draw_text_right(x + sidebar_w - 20, ry + 13, subs[i], row_sub, 1);
    }

    alpha_round_rect(content_x, y + 60, 44, 44, 12, accent, 35, 1);
    draw_ui_symbol(content_x + 22, y + 82, symbols[settings_section],
                   accent, 20);
    draw_text(content_x + 58, y + 67, rows[settings_section], text, 2);
    draw_text(content_x + 58, y + 88, "AOS System Settings", muted, 1);
    draw_text_right(content_x + card_w, y + 88,
                    settings_save_failed ? "Save failed" : "Local settings",
                    settings_save_failed ? UI_WARNING : muted, 1);

    if (settings_section == 3) {
        static const uint32_t accents[] = {
            0x238c50, 0xcd604c, 0x3f7fa8, 0xb650ae,
        };
        int theme_x = content_x + card_w - 126;
        int swatch_x = content_x + card_w - 126;

        draw_settings_card(content_x, y + 128, card_w, 128);
        draw_settings_row_text(content_x, y + 128, card_w, "Theme",
                               "Switch between light and dark mode", 1);
        draw_settings_choice(theme_x, y + 145, 56, "Light",
                             !settings_theme_dark);
        draw_settings_choice(theme_x + 62, y + 145, 56, "Dark",
                             settings_theme_dark);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Accent color", 0, 0);
        for (int i = 0; i < 4; i++) {
            int cx = swatch_x + 12 + i * 34;
            if (settings_accent == i) {
                circle_stroke(cx, y + 224, 15, 2, accent);
            }
            icon_dot(cx, y + 224, 24, 12, 12, 10, accents[i]);
        }

        draw_settings_card(content_x, y + 280, card_w, 128);
        draw_settings_row_text(content_x, y + 280, card_w, "Animations",
                               "Window and menu motion", 1);
        draw_settings_switch(content_x + card_w - 52, y + 302,
                             settings_animations);
        draw_settings_row_text(content_x, y + 344, card_w, "Transparency",
                               "Glass effect on panels", 0);
        draw_settings_switch(content_x + card_w - 52, y + 366,
                             settings_transparency);
    } else if (settings_section == 4) {
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w,
                               "Output volume", 0, 1);
        draw_settings_slider(content_x + card_w - 176, y + 152, 144,
                             settings_volume);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Input volume", 0, 1);
        draw_settings_slider(content_x + card_w - 176, y + 216, 144,
                             settings_input_volume);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "System sounds", 0, 0);
        draw_settings_switch(content_x + card_w - 52, y + 278,
                             settings_system_sounds);
    } else if (settings_section == 2) {
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w,
                               "Resolution", "Built-in display", 1);
        draw_text_right(content_x + card_w - 18, y + 155,
                        resolution, muted, 1);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Brightness", 0, 1);
        draw_settings_slider(content_x + card_w - 176, y + 216, 144,
                             settings_brightness);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "Night Light", "Warmer colors after sunset", 0);
        draw_settings_switch(content_x + card_w - 52, y + 278,
                             settings_night_light);
    } else if (settings_section == 0) {
        draw_settings_card(content_x, y + 128, card_w, 256);
        draw_settings_row_text(content_x, y + 128, card_w, "Wi-Fi", 0, 1);
        draw_settings_switch(content_x + card_w - 52, y + 150,
                             settings_wifi);
        draw_settings_row_text(content_x, y + 192, card_w, "Mountain-Net",
                               settings_wifi ? "Connected, secured" : "Unavailable", 1);
        draw_ui_symbol(content_x + card_w - 28, y + 224, UI_SYM_WIFI,
                       settings_wifi ? accent : muted, 16);
        draw_settings_row_text(content_x, y + 256, card_w, "Pine-Lodge 5G",
                               "Secured", 1);
        draw_ui_symbol(content_x + card_w - 28, y + 288, UI_SYM_CHEVRON,
                       muted, 14);
        draw_settings_row_text(content_x, y + 320, card_w, "ValleyGuest",
                               "Secured", 0);
        draw_ui_symbol(content_x + card_w - 28, y + 352, UI_SYM_CHEVRON,
                       muted, 14);
    } else if (settings_section == 5) {
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w, "Notifications",
                               "Allow MUI notification banners", 1);
        draw_settings_switch(content_x + card_w - 52, y + 150,
                             settings_notifications);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Do Not Disturb",
                               "Silence noncritical banners", 1);
        draw_settings_switch(content_x + card_w - 52, y + 214,
                             settings_do_not_disturb);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "Banner previews", "Show desktop popups", 0);
        draw_settings_switch(content_x + card_w - 52, y + 278,
                             settings_notification_banners);
    } else if (settings_section == 6) {
        int choice_w = (card_w - 32) / 3;
        int choice_x = content_x + 12;

        draw_settings_card(content_x, y + 128, card_w, 160);
        draw_settings_row_text(content_x, y + 128, card_w, "Power mode",
                               0, 0);
        draw_settings_choice(choice_x, y + 176, choice_w, "Saver",
                             settings_power_mode == 0);
        draw_settings_choice(choice_x + choice_w + 4, y + 176, choice_w,
                             "Balanced", settings_power_mode == 1);
        draw_settings_choice(choice_x + (choice_w + 4) * 2, y + 176,
                             choice_w, "Performance",
                             settings_power_mode == 2);
        alpha_rect(content_x + 16, y + 214, card_w - 32, 1,
                   settings_border_color(), 150, 1);
        draw_settings_row_text(content_x, y + 216, card_w,
                               "Reduce motion in Power Saver",
                               "Lowers animation work", 0);
        draw_settings_switch(content_x + card_w - 52, y + 238,
                             settings_reduce_motion_saver);
    } else if (settings_section == 7) {
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w,
                               "Remember command history",
                               "Keep commands for this MUI session", 1);
        draw_settings_switch(content_x + card_w - 52, y + 150,
                             settings_remember_history);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Command history", "Remove saved commands", 1);
        draw_settings_choice(content_x + card_w - 94, y + 209, 76,
                             "Clear", 0);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "Settings storage",
                               "Stored locally in your home folder", 0);
        draw_text_right(content_x + card_w - 18, y + 283,
                        "Private", accent, 1);
    } else if (settings_section == 8) {
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w,
                               "Kernel network stack",
                               "Managed by AOS networking services", 1);
        draw_text_right(content_x + card_w - 18, y + 155,
                        "Available", accent, 1);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Network status",
                               "Inspect interfaces and addresses", 1);
        draw_settings_choice(content_x + card_w - 94, y + 209, 76,
                             "Open", 0);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "Wireless controls",
                               "Wi-Fi and Bluetooth are managed separately", 0);
    } else if (settings_section == 9) {
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w,
                               "Current account",
                               session_status.administrator
                                   ? "Administrator" : "Standard user", 1);
        draw_text_right(content_x + card_w - 18, y + 155,
                        session_status.username[0]
                            ? session_status.username : "Local user",
                        accent, 1);
        draw_settings_row_text(content_x, y + 192, card_w,
                               "Home folder", AOS_MAIN_HOME, 1);
        draw_settings_choice(content_x + card_w - 94, y + 209, 76,
                             "Open", 0);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "Account data",
                               "Files remain under the user profile", 0);
    } else {
        char enabled_title[48];
        copy_cstr(enabled_title, sizeof(enabled_title), rows[settings_section]);
        append_cstr(enabled_title, sizeof(enabled_title), " enabled");
        draw_settings_card(content_x, y + 128, card_w, 192);
        draw_settings_row_text(content_x, y + 128, card_w, enabled_title,
                               "Manage this system feature", 1);
        draw_settings_switch(content_x + card_w - 52, y + 150,
                             settings_feature_enabled[settings_section]);
        draw_settings_row_text(content_x, y + 192, card_w, "Status",
                               "Current state", 1);
        draw_text_right(content_x + card_w - 18, y + 219,
                        subs[settings_section], muted, 1);
        draw_settings_row_text(content_x, y + 256, card_w,
                               "Automatic updates", 0, 0);
        draw_settings_switch(content_x + card_w - 52, y + 278,
                             settings_auto_updates[settings_section]);
    }
}

static void draw_settings_notice(void) {
    struct aos_uptime_info now = uptime_info;
    int width = 312;
    int x = (int)gfx.width - width - 16;
    int y = TOPBAR_HEIGHT + 12;

    if (settings_notice_until_second == 0 || !settings_notice[0]) return;
    if (syscall3(AOS_SYS_UPTIME_INFO, (long)&now, 0, 0) == 0 &&
        now.seconds >= settings_notice_until_second) {
        settings_notice_until_second = 0;
        settings_notice_frames = 0;
        return;
    }
    if (x < 8) {
        x = 8;
        width = (int)gfx.width - 16;
    }
    glass_panel(x, y, width, 54, 10, UI_GLASS_LIGHT,
                settings_transparency ? 242 : 255, UI_BORDER);
    alpha_round_rect(x + 10, y + 10, 34, 34, 9, UI_ACCENT, 42, 1);
    draw_ui_symbol(x + 27, y + 27, UI_SYM_BELL, UI_ACCENT, 18);
    draw_text(x + 53, y + 9, "MUI", UI_MUTED, 1);
    draw_text_ellipsized(x + 53, y + 29, settings_notice,
                         width - 65, UI_TEXT, 1);
}

static void apply_display_settings(void) {
    int dim_alpha = (100 - clamp_int(settings_brightness, 10, 100)) * 13 / 10;

    if (settings_night_light) {
        alpha_rect(0, 0, (int)gfx.width, (int)gfx.height,
                   0xffa35a, 18, 1);
    }
    if (dim_alpha > 0) {
        alpha_rect(0, 0, (int)gfx.width, (int)gfx.height,
                   0x000000, (uint32_t)dim_alpha, 1);
    }
}

static void terminal_make_prompt(char* out, uint64_t cap) {
    const char* home = AOS_MAIN_HOME;
    const char* username =
        session_status.username[0] ? session_status.username : "explorer";
    uint64_t home_len = cstrlen(home);

    copy_cstr(out, cap, username);
    append_cstr(out, cap, "@aos:");
    if (streq(terminal_cwd, home)) {
        append_cstr(out, cap, "~");
    } else if (starts_with(terminal_cwd, home) &&
               terminal_cwd[home_len] == '/') {
        append_cstr(out, cap, "~");
        append_cstr(out, cap, terminal_cwd + home_len);
    } else {
        append_cstr(out, cap, terminal_cwd[0] ? terminal_cwd : "/");
    }
    append_cstr(out, cap, "$ ");
}

static void draw_terminal(int x, int y, int w, int h) {
    char prompt[192];
    const char* visible_input = terminal_input;
    int max_rows = (h - 55) / 20;
    int extra_rows =
        ((terminal_stream_len > 0 || terminal_child_pid > 0) ? 1 : 0) +
        (terminal_child_pid > 0 ? 0 : 1);
    int history_rows = max_rows - extra_rows;
    int history_start;
    int line_y = y + 55;
    int row = 0;
    int prompt_x;

    if (history_rows < 0) history_rows = 0;
    history_start = terminal_last_count - history_rows;
    if (history_start < 0) history_start = 0;

    rect(x + 1, y + 36, w - 2, h - 37, 0x101b15);
    for (int i = history_start; i < terminal_last_count; i++) {
        draw_text_ellipsized(x + 14, line_y + row * 20,
                             terminal_last[i], w - 28,
                             starts_with(terminal_last[i], "AOS Terminal")
                                 ? 0x9ee3b8 : 0xe6efe5,
                             1);
        row++;
    }
    if (terminal_stream_len > 0 || terminal_child_pid > 0) {
        draw_text_ellipsized(x + 14, line_y + row * 20,
                             terminal_stream_line, w - 28,
                             0xe6efe5, 1);
        if (terminal_child_pid > 0) {
            rect(x + 14 + text_width(terminal_stream_line, 1),
                 line_y + row * 20 - 2, 2, 15, 0x78d49c);
        }
        row++;
    }
    if (terminal_child_pid > 0) {
        char running[96];
        copy_cstr(running, sizeof(running), "Running ");
        append_cstr(running, sizeof(running), terminal_active_command);
        append_cstr(running, sizeof(running), "  Ctrl+C stop");
        draw_text_right(x + w - 102, y + 14, running, UI_MUTED, 1);
        return;
    }

    terminal_make_prompt(prompt, sizeof(prompt));
    prompt_x = x + 14;
    while (visible_input[0] &&
           text_width(prompt, 1) + text_width(visible_input, 1) >
               w - 34) {
        visible_input++;
    }
    draw_text(prompt_x, line_y + row * 20, prompt, 0x78d49c, 1);
    prompt_x += text_width(prompt, 1);
    draw_text(prompt_x, line_y + row * 20, visible_input, 0xe6efe5, 1);
    rect(prompt_x + text_width(visible_input, 1),
         line_y + row * 20 - 2, 2, 15, 0x78d49c);
}

static const char* files_place_path(int place) {
    static const char* paths[] = {
        AOS_MAIN_HOME,
        AOS_MAIN_HOME "/Desktop",
        AOS_MAIN_HOME "/Documents",
        AOS_MAIN_HOME "/Downloads",
        AOS_MAIN_HOME "/Photos",
        AOS_MAIN_HOME "/Music",
        AOS_MAIN_HOME "/Starred",
        AOS_MAIN_HOME "/Trash",
    };

    if (place < 0 || place >= (int)(sizeof(paths) / sizeof(paths[0]))) {
        return 0;
    }
    return paths[place];
}

static void ensure_mui_user_layout(void) {
    static const char* paths[] = {
        AOS_MAIN_HOME,
        AOS_MAIN_HOME "/Desktop",
        AOS_MAIN_HOME "/Documents",
        AOS_MAIN_HOME "/Downloads",
        AOS_MAIN_HOME "/Music",
        AOS_MAIN_HOME "/Photos",
        AOS_MAIN_HOME "/Video",
        AOS_MAIN_HOME "/MUI",
        AOS_MAIN_HOME "/Starred",
        AOS_MAIN_HOME "/Trash",
        "/home",
        AOS_LEGACY_HOME,
    };

    for (uint64_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        syscall3(SYS_MKDIRAT, AT_FDCWD, (long)paths[i], 0755);
    }
}

static void join_path(char* out, uint64_t cap,
                      const char* directory, const char* name) {
    copy_cstr(out, cap, directory ? directory : "");
    if (cstrlen(out) > 0 && out[cstrlen(out) - 1] != '/') {
        append_char(out, cap, '/');
    }
    append_cstr(out, cap, name ? name : "");
}

static int choose_unique_path(const char* directory, const char* base,
                              const char* extension, char* name,
                              uint64_t name_cap, char* path,
                              uint64_t path_cap) {
    for (int suffix = 0; suffix < 100; suffix++) {
        copy_cstr(name, name_cap, base);
        if (suffix > 0) {
            append_cstr(name, name_cap, " ");
            append_cstr(name, name_cap, u64_to_dec((uint64_t)suffix + 1));
        }
        append_cstr(name, name_cap, extension);
        join_path(path, path_cap, directory, name);
        if (syscall2(SYS_ACCESS, (long)path, 0) < 0) return 0;
    }
    return -1;
}

static int files_refresh(void) {
    uint8_t dirent_buffer[1024];
    const char* path = files_current_path;
    long fd;

    file_entry_count = 0;
    files_selected = -1;
    files_scroll = 0;
    files_last_clicked = -1;
    if (!path || !path[0]) return 0;

    fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path,
                  O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) return -1;

    while (file_entry_count < MAX_FILE_ENTRIES) {
        long got = syscall3(SYS_GETDENTS64, fd, (long)dirent_buffer,
                            sizeof(dirent_buffer));
        uint64_t offset = 0;

        if (got < 0) {
            syscall1(SYS_CLOSE, fd);
            return -1;
        }
        if (got == 0) break;
        while (offset < (uint64_t)got &&
               file_entry_count < MAX_FILE_ENTRIES) {
            struct linux_dirent64* ent =
                (struct linux_dirent64*)(dirent_buffer + offset);
            if (ent->d_reclen < 20 ||
                offset + ent->d_reclen > (uint64_t)got) {
                break;
            }
            if (ent->d_name[0] != '.') {
                copy_cstr(file_entries[file_entry_count].name,
                          sizeof(file_entries[file_entry_count].name),
                          ent->d_name);
                file_entries[file_entry_count].is_dir =
                    ent->d_type == DT_DIR ? 1 : 0;
                file_entry_count++;
            }
            offset += ent->d_reclen;
        }
    }
    syscall1(SYS_CLOSE, fd);

    for (int i = 0; i < file_entry_count; i++) {
        for (int j = i + 1; j < file_entry_count; j++) {
            int swap = 0;
            if (file_entries[j].is_dir > file_entries[i].is_dir) {
                swap = 1;
            } else if (file_entries[j].is_dir ==
                           file_entries[i].is_dir &&
                       cstrcmp(file_entries[j].name,
                               file_entries[i].name) < 0) {
                swap = 1;
            }
            if (swap) {
                struct mui_file_entry tmp = file_entries[i];
                file_entries[i] = file_entries[j];
                file_entries[j] = tmp;
            }
        }
    }
    return 0;
}

static void files_sync_place(void) {
    files_place = -1;
    for (int i = 0; i < 8; i++) {
        const char* path = files_place_path(i);
        if (path && streq(path, files_current_path)) {
            files_place = i;
            return;
        }
    }
}

static int files_set_path(const char* path) {
    char previous[sizeof(files_current_path)];

    if (!path || !path[0]) return -1;
    copy_cstr(previous, sizeof(previous), files_current_path);
    copy_cstr(files_current_path, sizeof(files_current_path), path);
    files_sync_place();
    if (files_refresh() == 0) {
        files_status[0] = 0;
        return 0;
    }

    copy_cstr(files_current_path, sizeof(files_current_path), previous);
    files_sync_place();
    files_refresh();
    copy_cstr(files_status, sizeof(files_status), "Could not open folder");
    return -1;
}

static void files_go_parent(void) {
    char parent[sizeof(files_current_path)];
    uint64_t length;

    copy_cstr(parent, sizeof(parent), files_current_path);
    length = cstrlen(parent);
    while (length > 1 && parent[length - 1] == '/') {
        parent[--length] = 0;
    }
    while (length > 1 && parent[length - 1] != '/') {
        parent[--length] = 0;
    }
    if (length > 1) {
        parent[length - 1] = 0;
    } else {
        parent[0] = '/';
        parent[1] = 0;
    }
    files_set_path(parent);
}

static const char* files_display_path(void) {
    static char label[128];
    const char* home = AOS_MAIN_HOME;
    uint64_t home_len = cstrlen(home);

    if (streq(files_current_path, "/")) return "AOS Disk";
    if (streq(files_current_path, home)) return "Home";
    if (starts_with(files_current_path, home) &&
        files_current_path[home_len] == '/') {
        copy_cstr(label, sizeof(label), "Home");
        append_cstr(label, sizeof(label), files_current_path + home_len);
        return label;
    }
    return files_current_path;
}

static int files_current_is_trash(void) {
    const char* trash = AOS_MAIN_HOME "/Trash";
    uint64_t length = cstrlen(trash);

    return streq(files_current_path, trash) ||
           (starts_with(files_current_path, trash) &&
            files_current_path[length] == '/');
}

static int file_entry_path(int index, char* out, uint64_t cap) {
    const char* directory = files_current_path;

    if (!directory || index < 0 || index >= file_entry_count) return -1;
    join_path(out, cap, directory, file_entries[index].name);
    return 0;
}

static void editor_clear_content(void) {
    editor_lines = 1;
    editor_text[0][0] = 0;
    editor_saved = 0;
    editor_save_failed = 0;
}

static int editor_load_file(const char* path, const char* filename) {
    char buffer[512];
    long fd;
    int last_was_newline = 0;

    if (!path || !filename) return -1;
    fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
    if (fd < 0) return -1;

    editor_lines = 1;
    editor_text[0][0] = 0;
    for (;;) {
        long got = syscall3(SYS_READ, fd, (long)buffer, sizeof(buffer));
        if (got < 0) {
            syscall1(SYS_CLOSE, fd);
            editor_saved = 0;
            editor_save_failed = 1;
            return -1;
        }
        if (got == 0) break;
        for (long i = 0; i < got; i++) {
            char c = buffer[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (editor_lines <
                    (int)(sizeof(editor_text) / sizeof(editor_text[0]))) {
                    editor_text[editor_lines][0] = 0;
                    editor_lines++;
                }
                last_was_newline = 1;
            } else {
                append_char(editor_text[editor_lines - 1],
                            sizeof(editor_text[0]), c);
                last_was_newline = 0;
            }
        }
    }
    syscall1(SYS_CLOSE, fd);
    if (last_was_newline && editor_lines > 1 &&
        editor_text[editor_lines - 1][0] == 0) {
        editor_lines--;
    }

    copy_cstr(editor_path, sizeof(editor_path), path);
    copy_cstr(editor_filename, sizeof(editor_filename), filename);
    editor_saved = 1;
    editor_save_failed = 0;
    write_cstr("MUI: loaded ");
    write_cstr(editor_path);
    write_cstr("\n");
    return 0;
}

static int editor_save_file(void) {
    long fd;
    uint64_t bytes = 0;
    char notice[80];

    if (!editor_path[0]) {
        if (choose_unique_path(AOS_MAIN_HOME, "Untitled", ".txt",
                               editor_filename, sizeof(editor_filename),
                               editor_path, sizeof(editor_path)) != 0) {
            editor_saved = 0;
            editor_save_failed = 1;
            return -1;
        }
    }

    fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)editor_path,
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        editor_saved = 0;
        editor_save_failed = 1;
        return -1;
    }

    for (int i = 0; i < editor_lines; i++) {
        uint64_t length = cstrlen(editor_text[i]);
        if (write_fd_all((int)fd, editor_text[i], length) != 0 ||
            (i + 1 < editor_lines &&
             write_fd_all((int)fd, "\n", 1) != 0)) {
            syscall1(SYS_CLOSE, fd);
            editor_saved = 0;
            editor_save_failed = 1;
            return -1;
        }
        bytes += length;
        if (i + 1 < editor_lines) bytes++;
    }
    syscall1(SYS_CLOSE, fd);

    editor_saved = 1;
    editor_save_failed = 0;
    files_refresh();
    write_cstr("MUI: saved ");
    write_cstr(editor_path);
    write_cstr(" bytes=");
    write_cstr(u64_to_dec(bytes));
    write_cstr("\n");
    copy_cstr(notice, sizeof(notice), editor_filename);
    append_cstr(notice, sizeof(notice), " saved");
    settings_notify(notice);
    return 0;
}

static void editor_new_document(const char* directory, const char* base) {
    if (!directory) directory = AOS_MAIN_HOME;
    if (!base) base = "Untitled";
    editor_path[0] = 0;
    editor_filename[0] = 0;
    if (choose_unique_path(directory, base, ".txt",
                           editor_filename, sizeof(editor_filename),
                           editor_path, sizeof(editor_path)) != 0) {
        copy_cstr(editor_filename, sizeof(editor_filename), "Untitled.txt");
        join_path(editor_path, sizeof(editor_path), directory,
                  editor_filename);
    }
    editor_clear_content();
}

static int files_visible_capacity(int window_width, int window_height) {
    int content_width = window_width - 208;
    int content_height = window_height - 130;

    if (content_height < 38) content_height = 38;
    if (files_view == 0) {
        int columns = content_width >= 560 ? 5 : 4;
        int rows = content_height / 100;
        if (rows < 1) rows = 1;
        return columns * rows;
    }
    return content_height / 38;
}

static void draw_files(int x, int y, int w, int h) {
    static char count_label[24];
    const char* places[] = {
        "Home", "Desktop", "Documents", "Downloads",
        "Pictures", "Music", "Starred", "Trash",
    };
    const enum ui_symbol place_symbols[] = {
        UI_SYM_HOME, UI_SYM_GRID, UI_SYM_DOCUMENT, UI_SYM_DOWNLOAD,
        UI_SYM_IMAGE, UI_SYM_MUSIC, UI_SYM_STAR, UI_SYM_TRASH,
    };
    int count = file_entry_count;
    int sidebar_w = 208;
    int content_x = x + sidebar_w;
    int content_w = w - sidebar_w;
    int toolbar_h = 56;
    int footer_h = 30;
    int visible_capacity = files_visible_capacity(w, h);

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    rect(x + 1, y + 36, sidebar_w - 1, h - 37, UI_CARD_ALT);
    rect(content_x, y + 36, 1, h - 37, UI_BORDER);
    for (int i = 0; i < 8; i++) {
        int ry = y + 45 + i * 38;
        uint32_t text = i == files_place ? 0xffffff : UI_TEXT;
        if (i == files_place) {
            round_rect(x + 10, ry, sidebar_w - 20, 34, 9, UI_ACCENT);
        }
        draw_ui_symbol(x + 28, ry + 17, place_symbols[i], text, 16);
        draw_text(x + 47, ry + 13, places[i], text, 1);
    }
    rect(x + 12, y + 359, sidebar_w - 24, 1, UI_BORDER);
    if (streq(files_current_path, "/")) {
        round_rect(x + 10, y + 365, sidebar_w - 20, 34, 9,
                   UI_ACCENT);
    }
    draw_ui_symbol(x + 28, y + 382, UI_SYM_DISK,
                   streq(files_current_path, "/") ? 0xffffff : UI_MUTED,
                   16);
    draw_text(x + 47, y + 378, "AOS Disk",
              streq(files_current_path, "/") ? 0xffffff : UI_MUTED, 1);

    rect(content_x + 1, y + 36, content_w - 2, toolbar_h, UI_CARD);
    rect(content_x + 1, y + 36 + toolbar_h - 1, content_w - 2, 1, UI_BORDER);
    flat_panel(content_x + 10, y + 47, 32, 34, UI_CARD, UI_BORDER);
    flat_panel(content_x + 46, y + 47, 32, 34, UI_CARD, UI_BORDER);
    draw_ui_symbol(content_x + 26, y + 64, UI_SYM_HOME, UI_TEXT, 16);
    draw_ui_symbol(content_x + 62, y + 64, UI_SYM_UP,
                   streq(files_current_path, "/") ? UI_BORDER : UI_TEXT, 16);
    draw_text_ellipsized(content_x + 90, y + 59, files_display_path(),
                         content_w - 190, UI_TEXT, 2);

    flat_panel(x + w - 84, y + 48, 68, 34, UI_CARD, UI_BORDER);
    if (files_view == 0) round_rect(x + w - 81, y + 51, 30, 28, 7, UI_CARD_ALT);
    if (files_view == 1) round_rect(x + w - 49, y + 51, 30, 28, 7, UI_CARD_ALT);
    draw_ui_symbol(x + w - 66, y + 65, UI_SYM_GRID, UI_TEXT, 16);
    draw_ui_symbol(x + w - 34, y + 65, UI_SYM_LIST, UI_TEXT, 16);

    if (files_view == 0) {
        int cols = content_w >= 560 ? 5 : 4;
        int cell_w = (content_w - 24) / cols;
        for (int slot = 0;
             slot < visible_capacity && files_scroll + slot < count;
             slot++) {
            int i = files_scroll + slot;
            int fx = content_x + 12 + (slot % cols) * cell_w;
            int fy = y + 105 + (slot / cols) * 100;
            int center = fx + cell_w / 2;
            if (i == files_selected) {
                round_rect(fx + 3, fy - 5, cell_w - 6, 78, 9,
                           rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 6));
            }
            if (file_entries[i].is_dir) {
                draw_file_folder(center - 16, fy + 3, 1);
            }
            else draw_file_doc(center - 16, fy + 3, 1);
            draw_text_center_ellipsized(center, fy + 48,
                                        file_entries[i].name,
                                        cell_w - 10, UI_TEXT, 1);
        }
        if (count == 0) {
            draw_text_center(content_x + content_w / 2, y + h / 2,
                             "This folder is empty", UI_MUTED, 1);
        }
    } else {
        for (int slot = 0;
             slot < visible_capacity && files_scroll + slot < count;
             slot++) {
            int i = files_scroll + slot;
            int ry = y + 105 + slot * 38;
            if (i == files_selected) {
                round_rect(content_x + 12, ry, content_w - 24, 34, 7,
                           rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 6));
            } else if (i & 1) {
                round_rect(content_x + 12, ry, content_w - 24, 34, 7,
                           UI_CARD_ALT);
            }
            if (file_entries[i].is_dir) {
                draw_file_folder(content_x + 18, ry + 1, 1);
            }
            else draw_file_doc(content_x + 18, ry + 1, 1);
            draw_text(content_x + 58, ry + 13,
                      file_entries[i].name, UI_TEXT, 1);
            draw_text_right(x + w - 24, ry + 13,
                            file_entries[i].is_dir ? "Folder" : "File",
                            UI_MUTED, 1);
        }
    }

    if (count > visible_capacity) {
        int track_y = y + 100;
        int track_h = h - 135;
        int thumb_h = (track_h * visible_capacity) / count;
        int thumb_y;
        if (thumb_h < 24) thumb_h = 24;
        thumb_y = track_y +
            ((track_h - thumb_h) * files_scroll) /
            (count - visible_capacity);
        round_rect(x + w - 7, track_y, 3, track_h, 2, UI_BORDER);
        round_rect(x + w - 7, thumb_y, 3, thumb_h, 2, UI_MUTED);
    }

    rect(content_x + 1, y + h - footer_h, content_w - 2, footer_h - 1, UI_CARD);
    rect(content_x + 1, y + h - footer_h, content_w - 2, 1, UI_BORDER);
    copy_cstr(count_label, sizeof(count_label), u64_to_dec((uint64_t)count));
    append_cstr(count_label, sizeof(count_label), count == 1 ? " item" : " items");
    draw_text(content_x + 16, y + h - 19, count_label, UI_MUTED, 1);
    if (files_status[0]) {
        draw_text_right(x + w - 18, y + h - 19,
                        files_status, UI_MUTED, 1);
    }
}

static uint64_t editor_character_count(void) {
    uint64_t count = 0;
    for (int i = 0; i < editor_lines; i++) {
        count += cstrlen(editor_text[i]);
        if (i + 1 < editor_lines) count++;
    }
    return count;
}

static uint64_t editor_word_count(void) {
    uint64_t words = 0;
    int in_word = 0;

    for (int i = 0; i < editor_lines; i++) {
        for (uint64_t j = 0; editor_text[i][j]; j++) {
            char c = editor_text[i][j];
            if (c == ' ' || c == '\t') {
                in_word = 0;
            } else if (!in_word) {
                words++;
                in_word = 1;
            }
        }
        in_word = 0;
    }
    return words;
}

static void draw_editor(int x, int y, int w, int h) {
    static char stat[32];

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    rect(x + 1, y + 36, w - 2, 44, UI_CARD);
    rect(x + 1, y + 79, w - 2, 1, UI_BORDER);
    draw_ui_symbol(x + 18, y + 58, UI_SYM_DOCUMENT, UI_ACCENT, 16);
    draw_text(x + 32, y + 54,
              fallback(editor_filename, "Untitled.txt"), UI_TEXT, 1);
    round_rect(x + w - 84, y + 43, 68, 30, 9, UI_ACCENT);
    draw_ui_symbol(x + w - 70, y + 58, UI_SYM_SAVE, 0xffffff, 14);
    draw_text(x + w - 58, y + 54, "Save", 0xffffff, 1);

    for (int i = 0; i < editor_lines; i++) {
        draw_text(x + 22, y + 104 + i * 24, editor_text[i], UI_TEXT, 1);
    }

    rect(x + 1, y + h - 30, w - 2, 29, UI_CARD);
    rect(x + 1, y + h - 30, w - 2, 1, UI_BORDER);
    draw_ui_symbol(x + 16, y + h - 15, UI_SYM_TYPE, UI_MUTED, 13);
    copy_cstr(stat, sizeof(stat), u64_to_dec(editor_word_count()));
    append_cstr(stat, sizeof(stat), " words");
    draw_text(x + 28, y + h - 19, stat, UI_MUTED, 1);
    copy_cstr(stat, sizeof(stat), u64_to_dec(editor_character_count()));
    append_cstr(stat, sizeof(stat), " characters");
    draw_text(x + 112, y + h - 19, stat, UI_MUTED, 1);
    copy_cstr(stat, sizeof(stat), u64_to_dec((uint64_t)editor_lines));
    append_cstr(stat, sizeof(stat), " lines");
    draw_text(x + 232, y + h - 19, stat, UI_MUTED, 1);
    draw_text_right(
        x + w - 18, y + h - 19,
        editor_save_failed
            ? "Save failed"
            : (editor_saved ? "Saved to disk" : "Unsaved changes"),
        editor_save_failed ? UI_WARNING : UI_MUTED, 1);
}

static void draw_calculator(int x, int y, int w, int h) {
    int content_x = x + 17;
    int content_w = w - 34;
    int key_w = (content_w - 24) / 4;
    int key_h = 56;
    int display_y = y + 52;
    int display_h = h - 36 - 32 - 12 - (key_h * 5 + 32);
    int keys_y;
    const char* keys[] = {
        "C", "+/-", "%", "/",
        "7", "8", "9", "*",
        "4", "5", "6", "-",
        "1", "2", "3", "+",
        "0", ".", "=",
    };

    if (display_h < 84) display_h = 84;
    keys_y = display_y + display_h + 12;
    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    round_rect(content_x, display_y, content_w, display_h, 15, UI_CARD_ALT);
    draw_text_right(content_x + content_w - 20,
                    display_y + display_h - 54, calc_display, UI_TEXT,
                    cstrlen(calc_display) > 9 ? 3 : 4);

    for (int i = 0; i < 16; i++) {
        int kx = content_x + (i % 4) * (key_w + 8);
        int ky = keys_y + (i / 4) * (key_h + 8);
        uint32_t fill = (i % 4 == 3) ? UI_ACCENT : (i < 3 ? UI_CARD_ALT : UI_CARD);
        flat_panel(kx, ky, key_w, key_h, fill, fill == UI_CARD ? UI_BORDER : fill);
        if (calc_op && keys[i][0] == calc_op && keys[i][1] == 0) {
            circle_stroke(kx + key_w / 2, ky + key_h / 2,
                          key_h / 2 - 3, 2, 0x276f49);
        }
        draw_text_center(kx + key_w / 2, ky + 20, keys[i],
                         i % 4 == 3 ? 0xffffff : UI_TEXT, 2);
    }

    flat_panel(content_x, keys_y + 4 * (key_h + 8),
               key_w * 2 + 8, key_h, UI_CARD, UI_BORDER);
    draw_text_center(content_x + key_w + 4,
                     keys_y + 4 * (key_h + 8) + 20, keys[16], UI_TEXT, 2);
    flat_panel(content_x + 2 * (key_w + 8), keys_y + 4 * (key_h + 8),
               key_w, key_h, UI_CARD, UI_BORDER);
    draw_text_center(content_x + 2 * (key_w + 8) + key_w / 2,
                     keys_y + 4 * (key_h + 8) + 20, keys[17], UI_TEXT, 2);
    flat_panel(content_x + 3 * (key_w + 8), keys_y + 4 * (key_h + 8),
               key_w, key_h, UI_ACCENT, UI_ACCENT);
    draw_text_center(content_x + 3 * (key_w + 8) + key_w / 2,
                     keys_y + 4 * (key_h + 8) + 20, keys[18], 0xffffff, 2);
}

static void draw_about(int x, int y, int w, int h) {
    static char cores_label[24];
    static char ram_label[24];
    static char resolution[24];
    int cx = x + w / 2;
    int stat_x = cx - 192;
    int stat_w = 186;

    copy_cstr(cores_label, sizeof(cores_label),
              u64_to_dec(cpu_info.core_count ? cpu_info.core_count : 1));
    append_cstr(cores_label, sizeof(cores_label), " Cores");
    copy_cstr(ram_label, sizeof(ram_label),
              u64_to_dec(mem_info.total / (1024ULL * 1024ULL)));
    append_cstr(ram_label, sizeof(ram_label), " MiB RAM");
    copy_cstr(resolution, sizeof(resolution), u64_to_dec(gfx.width));
    append_cstr(resolution, sizeof(resolution), "x");
    append_cstr(resolution, sizeof(resolution), u64_to_dec(gfx.height));

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    round_rect(cx - 48, y + 60, 96, 42, 20, UI_ACCENT);
    alpha_round_rect(cx - 48, y + 60, 96, 42, 20, 0xb78d24, 105, 1);
    draw_text_center(cx, y + 67, "A", 0xffffff, 4);
    draw_text_center(cx, y + 122, "AOS", UI_TEXT, 3);
    draw_text_center(cx, y + 151, "Version 1.0 - Forest", UI_MUTED, 1);
    draw_text_center(cx, y + 171,
                     "A calm, nature-inspired desktop environment.", UI_MUTED, 1);
    draw_text_center(cx, y + 185,
                     "Native MUI for useful, affordable hardware.", UI_MUTED, 1);

    flat_panel(stat_x, y + 210, stat_w, 68, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(stat_x + stat_w / 2, y + 229, UI_SYM_CPU, UI_ACCENT, 18);
    draw_text_center(stat_x + stat_w / 2, y + 252,
                     cores_label, UI_TEXT, 1);
    flat_panel(stat_x + stat_w + 12, y + 210,
               stat_w, 68, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(stat_x + stat_w + 12 + stat_w / 2,
                   y + 229, UI_SYM_MEMORY, UI_ACCENT, 18);
    draw_text_center(stat_x + stat_w + 12 + stat_w / 2, y + 252,
                     ram_label, UI_TEXT, 1);
    flat_panel(stat_x, y + 290, stat_w, 68, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(stat_x + stat_w / 2, y + 309, UI_SYM_DISK, UI_ACCENT, 18);
    draw_text_center(stat_x + stat_w / 2, y + 332,
                     "512 GB SSD", UI_TEXT, 1);
    flat_panel(stat_x + stat_w + 12, y + 290,
               stat_w, 68, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(stat_x + stat_w + 12 + stat_w / 2,
                   y + 309, UI_SYM_MONITOR, UI_ACCENT, 18);
    draw_text_center(stat_x + stat_w + 12 + stat_w / 2, y + 332,
                     resolution, UI_TEXT, 1);

    flat_panel(stat_x, y + 382, stat_w * 2 + 12, 54, UI_CARD, UI_BORDER);
    draw_text(stat_x + 16, y + 402, "OS Name", UI_MUTED, 1);
    draw_text_right(stat_x + stat_w * 2 - 4, y + 402,
                    "AOS 1.0 Forest", UI_TEXT, 1);
    draw_text_center(cx, y + h - 28,
                     "2026 AOS Project - Native MUI", UI_MUTED, 1);
}

static void draw_calendar(int x, int y, int w, int h) {
    static char heading[32];
    static char selected_date[32];
    const char* week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int aside_w = 256;
    int content_w = w - aside_w;
    int grid_x = x + 20;
    int grid_y = y + 120;
    int grid_w = content_w - 40;
    int cell_w = grid_w / 7;
    int cell_h = (h - 150) / 6;
    int first_day = date_weekday(calendar_year, calendar_month, 1);
    int days = month_days(calendar_year, calendar_month);

    if (cell_h < 40) cell_h = 40;
    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    rect(x + content_w, y + 36, aside_w - 1, h - 37, UI_CARD_ALT);
    rect(x + content_w, y + 36, 1, h - 37, UI_BORDER);

    copy_cstr(heading, sizeof(heading), full_month_name((uint8_t)calendar_month));
    append_cstr(heading, sizeof(heading), " ");
    append_cstr(heading, sizeof(heading), u64_to_dec((uint64_t)calendar_year));
    draw_text(x + 22, y + 61, heading, UI_TEXT, 3);
    flat_panel(x + content_w - 158, y + 54, 34, 30, UI_CARD, UI_BORDER);
    stroke_line(x + content_w - 137, y + 62,
                x + content_w - 145, y + 69, 1, UI_TEXT);
    stroke_line(x + content_w - 145, y + 69,
                x + content_w - 137, y + 76, 1, UI_TEXT);
    flat_panel(x + content_w - 118, y + 54, 70, 30, UI_CARD, UI_BORDER);
    draw_text_center(x + content_w - 83, y + 65, "Today", UI_TEXT, 1);
    flat_panel(x + content_w - 42, y + 54, 34, 30, UI_CARD, UI_BORDER);
    stroke_line(x + content_w - 29, y + 62,
                x + content_w - 21, y + 69, 1, UI_TEXT);
    stroke_line(x + content_w - 21, y + 69,
                x + content_w - 29, y + 76, 1, UI_TEXT);

    for (int i = 0; i < 7; i++) draw_text_center(grid_x + i * cell_w + cell_w / 2, y + 100, week[i], UI_MUTED, 1);
    for (int day = 1; day <= days; day++) {
        int idx = first_day + day - 1;
        int cx = grid_x + (idx % 7) * cell_w;
        int cy = grid_y + (idx / 7) * cell_h;
        int is_today = calendar_year == (int)time_info.year &&
                       calendar_month == (int)time_info.month &&
                       day == (int)time_info.day;
        uint32_t fill = is_today ? UI_ACCENT :
                        (day == calendar_selected ? UI_CARD_ALT : UI_CARD);
        round_rect(cx + 2, cy + 2, cell_w - 4, cell_h - 4, 8, fill);
        draw_text_center(cx + cell_w / 2, cy + 12, u64_to_dec(day),
                         is_today ? 0xffffff : UI_TEXT, 1);
        if (day == 10 || day == 15 || day == 22) {
            icon_dot(cx + cell_w / 2, cy + cell_h - 10,
                     24, 12, 12, 2, is_today ? 0xffffff : UI_ACCENT);
        }
    }

    copy_cstr(selected_date, sizeof(selected_date),
              full_month_name((uint8_t)calendar_month));
    append_cstr(selected_date, sizeof(selected_date), " ");
    append_cstr(selected_date, sizeof(selected_date),
                u64_to_dec((uint64_t)calendar_selected));
    draw_text(x + content_w + 20, y + 62, selected_date, UI_TEXT, 2);
    draw_text(x + content_w + 20, y + 86,
              (calendar_selected == 10 || calendar_selected == 15 ||
               calendar_selected == 22) ? "1 event" : "0 events",
              UI_MUTED, 1);
    if (calendar_selected == 10 || calendar_selected == 15 ||
        calendar_selected == 22) {
        flat_panel(x + content_w + 16, y + 112,
                   aside_w - 32, 76, UI_CARD, UI_BORDER);
        round_rect(x + content_w + 28, y + 124, 4, 52, 2,
                   calendar_selected == 15 ? 0xb78d24 : UI_ACCENT);
        draw_text(x + content_w + 44, y + 128,
                  calendar_selected == 10 ? "Team sync" :
                  (calendar_selected == 15 ? "Trail hike" :
                   "AOS release review"), UI_TEXT, 1);
        draw_text(x + content_w + 44, y + 150,
                  calendar_selected == 10 ? "09:00" :
                  (calendar_selected == 15 ? "11:30" : "14:00"),
                  UI_MUTED, 1);
    } else {
        draw_text(x + content_w + 20, y + 126,
                  "No events scheduled.", UI_MUTED, 1);
    }
}

static void draw_clocks(int x, int y, int w, int h) {
    static char local_time[12];
    static char date_label[48];
    char zone_time[6];
    const char* cities[] = {"Local", "London", "New York", "Tokyo", "Sydney"};
    const int offsets[] = {0, -330, -630, 210, 270};

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    shifted_time(local_time, 0, 1);
    draw_text_center(x + w / 2, y + 67, local_time, UI_TEXT, 5);
    copy_cstr(date_label, sizeof(date_label), weekday_name(time_info.weekday));
    append_cstr(date_label, sizeof(date_label), ", ");
    append_cstr(date_label, sizeof(date_label),
                full_month_name(time_info.month));
    append_cstr(date_label, sizeof(date_label), " ");
    append_cstr(date_label, sizeof(date_label), u64_to_dec(time_info.day));
    append_cstr(date_label, sizeof(date_label), ", ");
    append_cstr(date_label, sizeof(date_label), u64_to_dec(time_info.year));
    draw_text_center(x + w / 2, y + 128, date_label, UI_MUTED, 1);
    rect(x + 1, y + 164, w - 2, 1, UI_BORDER);

    draw_text(x + 22, y + 184, "WORLD CLOCK", UI_MUTED, 1);
    flat_panel(x + 18, y + 204, w - 36, 218, UI_CARD, UI_BORDER);
    for (int i = 0; i < 5; i++) {
        int row_y = y + 205 + i * 43;
        shifted_time(zone_time, offsets[i], 0);
        if (i != 0) rect(x + 34, row_y, w - 68, 1, UI_BORDER);
        draw_text(x + 36, row_y + 17, cities[i], UI_TEXT, 1);
        draw_text_right(x + w - 36, row_y + 13, zone_time, UI_TEXT, 2);
    }
}

static void draw_photos(int x, int y, int w, int h) {
    int image_y = y + 36;
    int footer_h = 58;
    int image_h = h - 36 - footer_h;
    int nav_y = image_y + image_h / 2;

    rect(x + 1, image_y, w - 2, image_h, 0x17231d);
    draw_wallpaper_rect(x + 1, image_y, w - 2, image_h);
    if (photo_index) {
        alpha_rect(x + 1, image_y, w - 2, image_h, 0x174d31, 42, 1);
    }

    alpha_round_rect(x + 14, nav_y - 18, 36, 36, 18, 0x000000, 120, 1);
    stroke_line(x + 36, nav_y - 7, x + 27, nav_y, 2, 0xffffff);
    stroke_line(x + 27, nav_y, x + 36, nav_y + 7, 2, 0xffffff);
    alpha_round_rect(x + w - 50, nav_y - 18, 36, 36, 18,
                     0x000000, 120, 1);
    stroke_line(x + w - 36, nav_y - 7, x + w - 27, nav_y, 2, 0xffffff);
    stroke_line(x + w - 27, nav_y, x + w - 36, nav_y + 7, 2, 0xffffff);

    rect(x + 1, y + h - footer_h, w - 2, footer_h - 1, 0x101b15);
    rect(x + 1, y + h - footer_h, w - 2, 1, 0x314039);
    draw_text(x + 18, y + h - 43,
              photo_index ? "Green Valley" : "Mountain Lake", 0xffffff, 2);
    draw_text(x + 18, y + h - 20,
              photo_index ? "2.8 MB - 1920x1080" : "3.2 MB - 1920x1080",
              0x9aaaa0, 1);
    draw_ui_symbol(x + w - 84, y + h - 29,
                   UI_SYM_SEARCH, 0xc7d0c8, 16);
    icon_dot(x + w - 52, y + h - 29, 24, 12, 12, 3,
             photo_index ? 0x69766f : 0xffffff);
    icon_dot(x + w - 30, y + h - 29, 24, 12, 12, 3,
             photo_index ? 0xffffff : 0x69766f);
}

static void draw_disks(int x, int y, int w, int h) {
    const char* legend_names[] = {
        "System", "Applications", "Documents", "Media", "Free",
    };
    const int legend_values[] = {42, 28, 14, 56, 172};
    const uint32_t legend_colors[] = {
        UI_ACCENT, 0xc16b34, 0x527fa5, 0xb84b90, 0xd8e0d3,
    };
    const char* volume_names[] = {"AOS Root", "Forest Backup", "USB Drive"};
    const char* volume_subs[] = {
        "/ - SSD", "/mnt/backup - HDD", "/media/usb - Removable",
    };
    const char* volume_usage[] = {
        "340 GB / 512 GB", "210 GB / 1 TB", "8 GB / 32 GB",
    };
    int bx = x + 20;
    int bw = w - 40;
    int bar_x = bx + 18;
    int bar_w = bw - 36;
    int offset = 0;

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    flat_panel(bx, y + 54, bw, 132, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(bx + 28, y + 77, UI_SYM_DISK, UI_ACCENT, 18);
    draw_text(bx + 44, y + 73, "AOS Root - 512 GB SSD", UI_TEXT, 2);

    round_rect(bar_x, y + 104, bar_w, 14, 7, 0xd8e0d3);
    for (int i = 0; i < 5; i++) {
        int segment = bar_w * legend_values[i] / 312;
        rect(bar_x + offset, y + 104, segment, 14, legend_colors[i]);
        offset += segment;
    }
    for (int i = 0; i < 5; i++) {
        int lx = bar_x + i * (bar_w / 5);
        round_rect(lx, y + 139, 10, 10, 2, legend_colors[i]);
        draw_text(lx + 15, y + 141, legend_names[i], UI_TEXT, 1);
        draw_text(lx + 15, y + 157, u64_to_dec((uint64_t)legend_values[i]),
                  UI_MUTED, 1);
        draw_text(lx + 31, y + 157, "GB", UI_MUTED, 1);
    }

    draw_ui_symbol(bx + 9, y + 219, UI_SYM_DISK, UI_ACCENT, 16);
    draw_text(bx + 22, y + 215, "Volumes", UI_TEXT, 2);
    flat_panel(bx, y + 240, bw, 207, UI_CARD, UI_BORDER);
    for (int i = 0; i < 3; i++) {
        int row_y = y + 241 + i * 68;
        if (i != 0) rect(bx + 14, row_y, bw - 28, 1, UI_BORDER);
        alpha_round_rect(bx + 16, row_y + 14, 40, 40, 10,
                         UI_ACCENT, 35, 1);
        draw_ui_symbol(bx + 36, row_y + 34, UI_SYM_DISK, UI_ACCENT, 18);
        draw_text(bx + 68, row_y + 18, volume_names[i], UI_TEXT, 1);
        draw_text(bx + 68, row_y + 38, volume_subs[i], UI_MUTED, 1);
        draw_text_right(bx + bw - 18, row_y + 18,
                        volume_usage[i], UI_TEXT, 1);
        draw_text_right(bx + bw - 18, row_y + 38,
                        "Good", UI_ACCENT, 1);
    }
}

static int software_parse_record(char* line,
                                 struct mui_package_entry* package) {
    char* fields[7];
    char* cursor = line;

    for (int i = 0; i < 7; i++) {
        char* separator = cursor;
        fields[i] = cursor;
        while (*separator && *separator != '|') separator++;
        if (*separator != '|') return 0;
        *separator = 0;
        cursor = separator + 1;
    }
    if (!fields[0][0]) return 0;

    copy_cstr(package->id, sizeof(package->id), fields[0]);
    copy_cstr(package->name, sizeof(package->name),
              fields[1][0] ? fields[1] : fields[0]);
    copy_cstr(package->version, sizeof(package->version), fields[2]);
    copy_cstr(package->kind, sizeof(package->kind), fields[3]);
    copy_cstr(package->compatibility, sizeof(package->compatibility),
              fields[4]);
    copy_cstr(package->entry, sizeof(package->entry), fields[5]);
    copy_cstr(package->description, sizeof(package->description), fields[6]);
    return 1;
}

static int software_refresh_packages(void) {
    long fd;
    long total = 0;

    software_package_count = 0;
    fd = syscall4(SYS_OPENAT, AT_FDCWD,
                  (long)"/var/lib/uni/installed.db", O_RDONLY, 0);
    if (fd < 0) return 0;

    while (total + 1 < (long)sizeof(software_db_buffer)) {
        long got = syscall3(SYS_READ, fd,
                            (long)(software_db_buffer + total),
                            (long)(sizeof(software_db_buffer) - 1 -
                                   (uint64_t)total));
        if (got < 0) {
            syscall1(SYS_CLOSE, fd);
            return -1;
        }
        if (got == 0) break;
        total += got;
    }
    syscall1(SYS_CLOSE, fd);
    software_db_buffer[total] = 0;

    {
        uint64_t offset = 0;
        while (software_db_buffer[offset] &&
               software_package_count < MAX_SOFTWARE_PACKAGES) {
            char* line;
            while (software_db_buffer[offset] == '\n' ||
                   software_db_buffer[offset] == '\r') {
                offset++;
            }
            if (!software_db_buffer[offset]) break;
            line = &software_db_buffer[offset];
            while (software_db_buffer[offset] &&
                   software_db_buffer[offset] != '\n' &&
                   software_db_buffer[offset] != '\r') {
                offset++;
            }
            if (software_db_buffer[offset]) software_db_buffer[offset++] = 0;
            if (software_parse_record(
                    line, &software_packages[software_package_count])) {
                software_package_count++;
            }
        }
    }

    for (int i = 0; i < software_package_count; i++) {
        for (int j = i + 1; j < software_package_count; j++) {
            if (cstrcmp(software_packages[j].name,
                        software_packages[i].name) < 0) {
                struct mui_package_entry temporary = software_packages[i];
                software_packages[i] = software_packages[j];
                software_packages[j] = temporary;
            }
        }
    }
    return 0;
}

static int software_matches_category(
    const struct mui_package_entry* package) {
    if (software_category == 0) return 1;
    if (software_category == 1) {
        return streq(package->compatibility, "aos-native");
    }
    if (software_category == 2) {
        return starts_with(package->compatibility, "linux-");
    }
    return streq(package->kind, "appimage");
}

static int software_filtered_count(void) {
    int count = 0;
    for (int i = 0; i < software_package_count; i++) {
        if (software_matches_category(&software_packages[i])) count++;
    }
    return count;
}

static struct mui_package_entry* software_package_at(int visible_index) {
    int visible = 0;
    for (int i = 0; i < software_package_count; i++) {
        if (!software_matches_category(&software_packages[i])) continue;
        if (visible == visible_index) return &software_packages[i];
        visible++;
    }
    return (struct mui_package_entry*)0;
}

static int software_start_uni(int action, const char* command,
                              const char* argument, const char* label) {
    int output_pipe[2] = {-1, -1};
    char* argv[4];
    long pid;

    if (software_child_pid > 0) {
        copy_cstr(software_status, sizeof(software_status),
                  "Another package action is still running");
        return -1;
    }
    if (syscall1(SYS_PIPE, (long)output_pipe) < 0) {
        copy_cstr(software_status, sizeof(software_status),
                  "Could not create package output pipe");
        return -1;
    }

    argv[0] = (char*)"uni";
    argv[1] = (char*)command;
    argv[2] = (char*)argument;
    argv[3] = (char*)0;
    pid = syscall1(SYS_FORK, 0);
    if (pid < 0) {
        syscall1(SYS_CLOSE, output_pipe[0]);
        syscall1(SYS_CLOSE, output_pipe[1]);
        copy_cstr(software_status, sizeof(software_status),
                  "Could not start Uni");
        return -1;
    }
    if (pid == 0) {
        syscall1(SYS_CLOSE, output_pipe[0]);
        syscall2(SYS_DUP2, output_pipe[1], 1);
        syscall2(SYS_DUP2, output_pipe[1], 2);
        syscall1(SYS_CLOSE, output_pipe[1]);
        syscall3(SYS_EXECVE, (long)"/uni", (long)argv, 0);
        syscall3(SYS_EXECVE, (long)"/uni.elf", (long)argv, 0);
        write_cstr("MUI Software: Uni is unavailable\n");
        exit_code(127);
    }

    syscall1(SYS_CLOSE, output_pipe[1]);
    software_child_pid = (int)pid;
    software_child_stdout = output_pipe[0];
    software_action = action;
    software_output_length = 0;
    software_output_line[0] = 0;
    copy_cstr(software_action_target, sizeof(software_action_target),
              label ? label : argument);
    copy_cstr(software_status, sizeof(software_status),
              action == SOFTWARE_ACTION_INSTALL ? "Installing " :
              action == SOFTWARE_ACTION_REMOVE ? "Removing " :
                                                  "Launching ");
    append_cstr(software_status, sizeof(software_status),
                software_action_target);
    append_cstr(software_status, sizeof(software_status), "...");
    return 0;
}

static void software_feed_output(const char* bytes, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        char c = bytes[i];
        if (c == '\r') continue;
        if (c == '\n') {
            if (software_output_length > 0) {
                software_output_line[software_output_length] = 0;
                copy_cstr(software_status, sizeof(software_status),
                          software_output_line);
                software_output_length = 0;
            }
            continue;
        }
        if (software_output_length + 1 <
            (int)sizeof(software_output_line)) {
            software_output_line[software_output_length++] = c;
        }
    }
}

static int software_pump(void) {
    struct linux_pollfd poll_fd;
    char output[256];
    int status = 0;
    long waited;
    int redraw = 0;

    if (software_child_pid <= 0) return 0;
    if (software_child_stdout >= 0) {
        poll_fd.fd = software_child_stdout;
        poll_fd.events = POLLIN;
        poll_fd.revents = 0;
        if (syscall3(SYS_POLL, (long)&poll_fd, 1, 0) > 0) {
            if (poll_fd.revents & POLLIN) {
                long got = syscall3(SYS_READ, software_child_stdout,
                                    (long)output, sizeof(output));
                if (got > 0) {
                    software_feed_output(output, (uint64_t)got);
                    redraw = 1;
                }
            }
            if (poll_fd.revents & POLLHUP) {
                for (;;) {
                    long got = syscall3(SYS_READ, software_child_stdout,
                                        (long)output, sizeof(output));
                    if (got <= 0) break;
                    software_feed_output(output, (uint64_t)got);
                }
                syscall1(SYS_CLOSE, software_child_stdout);
                software_child_stdout = -1;
                redraw = 1;
            }
        }
    }

    waited = syscall4(SYS_WAIT4, software_child_pid,
                      (long)&status, WNOHANG, 0);
    if (waited == software_child_pid) {
        int action = software_action;
        int exit_status = (status >> 8) & 0xff;
        char target[sizeof(software_action_target)];

        copy_cstr(target, sizeof(target), software_action_target);
        if (software_child_stdout >= 0) {
            for (;;) {
                long got = syscall3(SYS_READ, software_child_stdout,
                                    (long)output, sizeof(output));
                if (got <= 0) break;
                software_feed_output(output, (uint64_t)got);
            }
            syscall1(SYS_CLOSE, software_child_stdout);
            software_child_stdout = -1;
        }
        software_child_pid = 0;
        software_action = SOFTWARE_ACTION_NONE;
        software_action_target[0] = 0;
        if (action == SOFTWARE_ACTION_INSTALL ||
            action == SOFTWARE_ACTION_REMOVE) {
            software_refresh_packages();
        }
        if (exit_status == 0) {
            copy_cstr(software_status, sizeof(software_status),
                      action == SOFTWARE_ACTION_INSTALL ? "Installed " :
                      action == SOFTWARE_ACTION_REMOVE ? "Removed " :
                                                         "Finished ");
            append_cstr(software_status, sizeof(software_status), target);
        } else if (!software_status[0]) {
            copy_cstr(software_status, sizeof(software_status),
                      "Package action failed");
        }
        redraw = 1;
    }
    return redraw;
}

static void software_stop_child(void) {
    int status = 0;

    if (software_child_pid <= 0) return;
    syscall3(AOS_SYS_PROCESS_KILL, software_child_pid, 0, 0);
    syscall4(SYS_WAIT4, software_child_pid, (long)&status, 0, 0);
    if (software_child_stdout >= 0) {
        syscall1(SYS_CLOSE, software_child_stdout);
    }
    software_child_pid = 0;
    software_child_stdout = -1;
    software_action = SOFTWARE_ACTION_NONE;
}

static void draw_software(int x, int y, int w, int h) {
    static const char* categories[] = {
        "All", "AOS native", "Linux", "AppImage"
    };
    static const int category_widths[] = {52, 92, 62, 82};
    int filtered = software_filtered_count();
    int col_w = (w - 52) / 2;
    int rows = (h - 205) / 92;
    int capacity;
    int maximum_scroll;
    int category_x = x + 22;

    if (rows < 1) rows = 1;
    capacity = rows * 2;
    maximum_scroll = filtered - capacity;
    if (maximum_scroll < 0) maximum_scroll = 0;
    if (software_scroll > maximum_scroll) software_scroll = maximum_scroll;
    if (software_scroll < 0) software_scroll = 0;

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    rect(x + 1, y + 36, w - 2, 116, UI_CARD);
    rect(x + 1, y + 151, w - 2, 1, UI_BORDER);
    draw_text(x + 22, y + 55, "Installed applications", UI_TEXT, 2);
    draw_text(x + 22, y + 80, u64_to_dec((uint64_t)software_package_count),
              UI_ACCENT, 1);
    draw_text(x + 35, y + 80,
              software_package_count == 1 ? " package managed by Uni" :
                                            " packages managed by Uni",
              UI_MUTED, 1);

    flat_panel(x + w - 222, y + 51, 108, 34, UI_CARD_ALT, UI_BORDER);
    draw_ui_symbol(x + w - 204, y + 68, UI_SYM_DOWNLOAD, UI_ACCENT, 15);
    draw_text(x + w - 191, y + 64, "Downloads", UI_TEXT, 1);
    flat_panel(x + w - 104, y + 51, 82, 34, UI_CARD_ALT, UI_BORDER);
    draw_text_center(x + w - 63, y + 64, "Refresh", UI_TEXT, 1);

    for (int i = 0; i < 4; i++) {
        round_rect(category_x, y + 102, category_widths[i], 28, 14,
                   i == software_category ? UI_ACCENT : UI_CARD_ALT);
        draw_text_center(category_x + category_widths[i] / 2, y + 113,
                         categories[i],
                         i == software_category ? 0xffffff : UI_TEXT, 1);
        category_x += category_widths[i] + 8;
    }
    if (software_status[0]) {
        draw_text_ellipsized(x + 332, y + 112, software_status,
                             w - 354, UI_MUTED, 1);
    }

    if (filtered == 0) {
        draw_tile(x + w / 2 - 28, y + 210, 56, 10, 0);
        draw_text_center(x + w / 2, y + 284,
                         software_package_count == 0
                             ? "No applications installed yet"
                             : "No applications match this filter",
                         UI_TEXT, 2);
        draw_text_center(x + w / 2, y + 314,
                         "Open a .deb, .ainstall, or .AppImage from Files.",
                         UI_MUTED, 1);
    } else {
        for (int slot = 0; slot < capacity; slot++) {
            struct mui_package_entry* package =
                software_package_at(software_scroll + slot);
            int col = slot % 2;
            int row = slot / 2;
            int cx = x + 18 + col * (col_w + 16);
            int cy = y + 164 + row * 92;
            const char* compatibility;
            int can_run;

            if (!package) break;
            can_run = streq(package->compatibility, "aos-native");
            compatibility =
                can_run ? "AOS native" :
                streq(package->compatibility, "linux-static")
                    ? "Linux static" : "Linux runtime needed";
            flat_panel(cx, cy, col_w, 82, UI_CARD_ALT, UI_BORDER);
            draw_tile(cx + 10, cy + 13, 50, 10, 0);
            draw_text_ellipsized(cx + 72, cy + 13, package->name,
                                 col_w - 184, UI_TEXT, 1);
            draw_text(cx + 72, cy + 33,
                      package->version[0] ? package->version : "Unversioned",
                      UI_MUTED, 1);
            draw_text_ellipsized(cx + 72, cy + 54,
                                 package->description[0]
                                     ? package->description
                                     : compatibility,
                                 col_w - 184, UI_MUTED, 1);

            round_rect(cx + col_w - 108, cy + 13, 62, 28, 8,
                       can_run ? UI_ACCENT : UI_BORDER);
            draw_text_center(cx + col_w - 77, cy + 24,
                             can_run ? "Open" : "Runtime",
                             can_run ? 0xffffff : UI_MUTED, 1);
            flat_panel(cx + col_w - 39, cy + 13, 29, 28,
                       UI_CARD, UI_BORDER);
            draw_ui_symbol(cx + col_w - 24, cy + 27,
                           UI_SYM_TRASH, UI_WARNING, 14);
            draw_text_ellipsized(cx + col_w - 108, cy + 55,
                                 compatibility, 98,
                                 can_run ? UI_ACCENT : UI_WARNING, 1);
        }
    }

    if (filtered > capacity) {
        draw_text_center(x + w / 2, y + h - 19,
                         u64_to_dec((uint64_t)(software_scroll + 1)),
                         UI_MUTED, 1);
        round_rect(x + w / 2 - 62, y + h - 31, 32, 24, 8,
                   software_scroll > 0 ? UI_CARD_ALT : UI_BORDER);
        draw_text_center(x + w / 2 - 46, y + h - 23, "<", UI_TEXT, 1);
        round_rect(x + w / 2 + 30, y + h - 31, 32, 24, 8,
                   software_scroll < maximum_scroll
                       ? UI_CARD_ALT : UI_BORDER);
        draw_text_center(x + w / 2 + 46, y + h - 23, ">", UI_TEXT, 1);
    }
}

static void install_set_defaults(void) {
    for (uint64_t i = 0; i < sizeof(install_config); i++)
        ((uint8_t*)&install_config)[i] = 0;
    for (uint64_t i = 0; i < sizeof(install_status); i++)
        ((uint8_t*)&install_status)[i] = 0;
    install_config.version = AOS_INSTALL_ABI_VERSION;
    install_config.partition_mode = AOS_INSTALL_PARTITION_AUTO;
    install_config.account_admin = 1;
    install_config.driver_mode = AOS_INSTALL_DRIVERS_STABLE;
    install_config.driver_scan_mask = 0x0f;
    install_config.app_profile = AOS_INSTALL_APPS_FULL;
    install_config.app_mask = 0xff;
    copy_cstr(install_config.username, sizeof(install_config.username),
              "explorer");
    copy_cstr(install_config.hostname, sizeof(install_config.hostname),
              "aos");
    copy_cstr(install_config.language, sizeof(install_config.language),
              "English");
    copy_cstr(install_config.region, sizeof(install_config.region),
              "India");
    copy_cstr(install_config.timezone, sizeof(install_config.timezone),
              "Asia/Kolkata");
    copy_cstr(install_config.keyboard, sizeof(install_config.keyboard),
              "US English");
    install_page = 0;
    install_focus = 0;
    install_scan_pci = -1;
    install_scan_drivers = -1;
    install_keyboard_test[0] = 0;
    install_notice[0] = 0;
}

static void install_refresh_status(void) {
    syscall2(AOS_SYS_INSTALLER, AOS_INSTALL_QUERY,
             (long)&install_status);
}

static void install_refresh_disks(void) {
    struct aos_blkdev_user disk;
    install_disk_count = 0;
    for (int i = 0; i < 16 && install_disk_count < 8; i++) {
        if (syscall2(AOS_SYS_BLKDEV_INFO, i, (long)&disk) < 0) break;
        if (!disk.has_ops || disk.read_only) continue;
        install_disks[install_disk_count++] = disk;
    }
    if (install_config.target_blkdev_id == 0 && install_status.target_name[0])
        install_config.target_blkdev_id = install_status.target_blkdev_id;
}

static int install_count_service(long service) {
    uint8_t info[256];
    int count = 0;
    while (count < 128 && syscall2(service, count, (long)info) >= 0) count++;
    return count;
}

static const struct aos_blkdev_user* install_selected_disk(void) {
    for (int i = 0; i < install_disk_count; i++) {
        if (install_disks[i].id == install_config.target_blkdev_id)
            return &install_disks[i];
    }
    return install_disk_count ? &install_disks[0] : 0;
}

static const char* install_secret(const char* value) {
    static char hidden[65];
    uint64_t n = cstrlen(value);
    if (n >= sizeof(hidden)) n = sizeof(hidden) - 1;
    for (uint64_t i = 0; i < n; i++) hidden[i] = '*';
    hidden[n] = 0;
    return hidden;
}

static void draw_install_field(int x, int y, int w, const char* label,
                               const char* value, int secret, int focus) {
    draw_text(x, y, label, UI_MUTED, 1);
    alpha_round_rect(x, y + 20, w, 38, 8,
                     focus == install_focus ? UI_ACCENT : UI_BORDER,
                     255, 1);
    alpha_round_rect(x + 1, y + 21, w - 2, 36, 7,
                     UI_CARD, 255, 1);
    draw_text(x + 12, y + 34,
              value && value[0] ? (secret ? install_secret(value) : value)
                                : "Not set",
              value && value[0] ? UI_TEXT : UI_MUTED, 1);
    if (focus == install_focus) {
        int tw = text_width(value && value[0]
                                ? (secret ? install_secret(value) : value)
                                : "", 1);
        rect(x + 13 + tw, y + 31, 1, 16, UI_ACCENT);
    }
}

static void draw_install_choice(int x, int y, int w, const char* title,
                                const char* subtitle, int selected) {
    uint32_t fill = selected ? rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 6)
                             : UI_CARD;
    alpha_round_rect(x, y, w, 58, 8,
                     selected ? UI_ACCENT : UI_BORDER, 255, 1);
    alpha_round_rect(x + 1, y + 1, w - 2, 56, 7, fill, 255, 1);
    circle_stroke(x + 20, y + 29, 8, 2,
                  selected ? UI_ACCENT : UI_MUTED);
    if (selected) disc(x + 20, y + 29, 4, UI_ACCENT);
    draw_text(x + 38, y + 15, title, UI_TEXT, 1);
    if (subtitle) draw_text(x + 38, y + 35, subtitle, UI_MUTED, 1);
}

static void draw_install_button(int x, int y, int w, const char* label,
                                int primary, int enabled) {
    uint32_t fill = primary ? UI_ACCENT : UI_CARD;
    uint32_t text = primary ? 0xffffff : UI_TEXT;
    if (!enabled) {
        fill = UI_CARD_ALT;
        text = UI_MUTED;
    }
    alpha_round_rect(x, y, w, 36, 8,
                     primary && enabled ? UI_ACCENT : UI_BORDER, 255, 1);
    alpha_round_rect(x + 1, y + 1, w - 2, 34, 7, fill, 255, 1);
    draw_text_center(x + w / 2, y + 13, label, text, 1);
}

static const char* install_page_title(int page) {
    static const char* titles[] = {
        "Welcome", "User account", "Partitioning", "Language",
        "Country / region", "Timezone", "Wi-Fi setup",
        "Device scan and drivers", "Additional apps", "Keyboard layout",
        "Hostname", "Review and install"
    };
    if (page < 0 || page > 11) return "Installation progress";
    return titles[page];
}

static const char* install_step_label(int step) {
    switch (step) {
        case AOS_INSTALL_STEP_PREPARING: return "Preparing installer";
        case AOS_INSTALL_STEP_CHECKING_DISK: return "Checking target disk";
        case AOS_INSTALL_STEP_PARTITIONING: return "Creating partitions";
        case AOS_INSTALL_STEP_FORMATTING: return "Formatting filesystem";
        case AOS_INSTALL_STEP_COPYING_KERNEL: return "Copying kernel";
        case AOS_INSTALL_STEP_COPYING_INITRD: return "Copying initrd";
        case AOS_INSTALL_STEP_COPYING_USERSPACE: return "Copying userspace";
        case AOS_INSTALL_STEP_WRITING_CONFIG: return "Writing configuration";
        case AOS_INSTALL_STEP_INSTALLING_BOOTLOADER: return "Installing GRUB";
        case AOS_INSTALL_STEP_CREATING_LOG: return "Creating install log";
        case AOS_INSTALL_STEP_FINISHING: return "Verifying installation";
        case AOS_INSTALL_STEP_COMPLETE: return "Installation complete";
        case AOS_INSTALL_STEP_ERROR: return "Installation failed";
        default: return "Ready";
    }
}

static void draw_install_nav(int x, int y, int h) {
    static const char* labels[] = {
        "Account", "Disk", "Language", "Region", "Timezone", "Wi-Fi",
        "Drivers", "Apps", "Keyboard", "Hostname", "Install"
    };
    rect(x + 1, y + 36, 204, h - 37, UI_CARD_ALT);
    rect(x + 204, y + 36, 1, h - 37, UI_BORDER);
    alpha_round_rect(x + 18, y + 53, 42, 42, 10, UI_ACCENT, 255, 1);
    draw_app_symbol(x + 39, y + 74, 12, 0xffffff, 22);
    draw_text(x + 72, y + 57, "Install AOS", UI_TEXT, 2);
    draw_text(x + 72, y + 78, "Live installer", UI_MUTED, 1);
    for (int i = 0; i < 11; i++) {
        int row = y + 108 + i * 35;
        int selected = install_page == i + 1 ||
                       (install_page == 12 &&
                        install_status.step == (uint32_t)(i + 1));
        if (selected)
            round_rect(x + 10, row - 4, 184, 31, 7,
                       rgb_lerp(UI_CARD_ALT, UI_ACCENT, 1, 5));
        if ((install_page > i + 1 && install_page < 12) ||
            (install_page >= 12 &&
             install_status.step > (uint32_t)(i + 1))) {
            disc(x + 25, row + 11, 8, UI_ACCENT);
            draw_ui_symbol(x + 25, row + 11, UI_SYM_CHECK, 0xffffff, 11);
        } else {
            circle_stroke(x + 25, row + 11, 8, 1,
                          selected ? UI_ACCENT : UI_MUTED);
            draw_text_center(x + 25, row + 7, u64_to_dec(i + 1),
                             selected ? UI_ACCENT : UI_MUTED, 1);
        }
        draw_text(x + 43, row + 7, labels[i],
                  selected ? UI_TEXT : UI_MUTED, 1);
    }
}

static void draw_install_summary_line(int x, int y, const char* label,
                                      const char* value) {
    draw_text(x, y, label, UI_MUTED, 1);
    draw_text(x + 132, y, value, UI_TEXT, 1);
}

static void draw_installer(int x, int y, int w, int h) {
    const struct aos_blkdev_user* disk = install_selected_disk();
    int cx = x + 229;
    int cw = w - 253;
    int footer_y = y + h - 54;
    int half = (cw - 12) / 2;
    const char* languages[] = {"English", "Hindi", "Other"};
    const char* regions[] = {"India", "United States", "United Kingdom", "Other"};
    const char* zones[] = {"Asia/Kolkata", "UTC", "America/New_York", "Europe/London"};
    const char* keyboards[] = {"US English", "UK English", "Indian English", "Hindi / InScript"};
    const char* profiles[] = {"Minimal", "Basic tools", "Full system", "Custom"};

    rect(x + 1, y + 36, w - 2, h - 37, UI_CARD);
    draw_install_nav(x, y, h);
    if (install_page == 0) {
        alpha_round_rect(cx, y + 65, 58, 58, 14, UI_ACCENT, 255, 1);
        draw_app_symbol(cx + 29, y + 94, 12, 0xffffff, 30);
        draw_text(cx, y + 148, "Install AOS 1.0 Forest", UI_TEXT, 3);
        draw_text(cx, y + 184,
                  "Configure this computer, then install a bootable AOS system.",
                  UI_MUTED, 1);
        draw_text(cx, y + 207,
                  "The selected disk is changed only after explicit confirmation.",
                  UI_MUTED, 1);
        flat_panel(cx, y + 246, cw, 92, UI_CARD_ALT, UI_BORDER);
        draw_ui_symbol(cx + 24, y + 270, UI_SYM_DISK, UI_ACCENT, 20);
        draw_text(cx + 48, y + 260,
                  disk ? disk->name : "No writable hardware disk", UI_TEXT, 2);
        if (disk) {
            draw_text(cx + 48, y + 287,
                      u64_to_dec(disk->size / (1024ULL * 1024ULL)),
                      UI_MUTED, 1);
            draw_text(cx + 86, y + 287, "MiB available", UI_MUTED, 1);
        }
        draw_text(cx, y + 374,
                  !install_status.live_mode
                      ? "This system is already installed. Boot the AOS live media to reinstall."
                      : !install_status.payload_ready
                            ? "The complete live installation payload is not loaded."
                            : !disk ? "Connect a writable disk before continuing."
                                    : "Ready to configure the installation.",
                  (!install_status.live_mode || !install_status.payload_ready || !disk)
                      ? UI_WARNING : UI_ACCENT, 1);
        draw_install_button(cx + cw - 150, footer_y, 150, "Begin setup", 1,
                            install_status.live_mode &&
                            install_status.payload_ready && disk);
        return;
    }

    draw_text(cx, y + 59,
              install_page == 12 ? "Installation progress" :
              install_page == 13 ? "Installation complete" :
              install_page_title(install_page), UI_TEXT, 3);
    if (install_page >= 1 && install_page <= 11) {
        draw_text_right(cx + cw, y + 66,
                        u64_to_dec((uint64_t)install_page), UI_ACCENT, 2);
        draw_text_right(cx + cw, y + 86, "of 11", UI_MUTED, 1);
    }

    if (install_page == 1) {
        draw_text(cx, y + 98, "Create the first local account.", UI_MUTED, 1);
        draw_install_field(cx, y + 130, cw, "Username",
                           install_config.username, 0, 1);
        draw_install_field(cx, y + 205, half, "Password",
                           install_config.password, 1, 2);
        draw_install_field(cx + half + 12, y + 205, half,
                           "Confirm password",
                           install_config.password_confirm, 1, 3);
        draw_install_choice(cx, y + 296, half, "Administrator",
                            "Can manage this system", install_config.account_admin);
        draw_install_choice(cx + half + 12, y + 296, half, "Standard user",
                            "Everyday access", !install_config.account_admin);
        draw_settings_switch(cx, y + 382, install_config.auto_login);
        draw_text(cx + 48, y + 379, "Log in automatically after boot", UI_TEXT, 1);
    } else if (install_page == 2) {
        draw_text(cx, y + 98, "Choose the hardware disk AOS will erase and use.", UI_MUTED, 1);
        flat_panel(cx, y + 130, cw, 86, UI_CARD_ALT, UI_BORDER);
        draw_ui_symbol(cx + 28, y + 173, UI_SYM_DISK, UI_ACCENT, 24);
        draw_text(cx + 54, y + 151, disk ? disk->name : "No disk", UI_TEXT, 2);
        draw_text(cx + 54, y + 181,
                  disk ? u64_to_dec(disk->size / (1024ULL * 1024ULL)) : "0",
                  UI_MUTED, 1);
        draw_text(cx + 94, y + 181, "MiB - click to change", UI_MUTED, 1);
        draw_install_choice(cx, y + 242, cw, "Automatic partitioning",
                            "Creates FAT boot and AOSFS system partitions",
                            install_config.partition_mode == AOS_INSTALL_PARTITION_AUTO);
        draw_install_choice(cx, y + 310, cw, "Manual partitioning",
                            "Use the Disks/partition command and validate the layout",
                            install_config.partition_mode == AOS_INSTALL_PARTITION_MANUAL);
        draw_text(cx, y + 397,
                  "Automatic mode is the supported bootable layout in this release.",
                  UI_WARNING, 1);
    } else if (install_page == 3) {
        draw_text(cx, y + 98, "Choose the language used by AOS.", UI_MUTED, 1);
        for (int i = 0; i < 3; i++)
            draw_install_choice(cx, y + 132 + i * 70, cw, languages[i], 0,
                                streq(install_config.language, languages[i]));
    } else if (install_page == 4) {
        draw_text(cx, y + 98, "Set formats and regional defaults.", UI_MUTED, 1);
        for (int i = 0; i < 4; i++)
            draw_install_choice(cx + (i % 2) * (half + 12),
                                y + 132 + (i / 2) * 70, half,
                                regions[i], 0,
                                streq(install_config.region, regions[i]));
    } else if (install_page == 5) {
        draw_text(cx, y + 98, "Set the system clock timezone.", UI_MUTED, 1);
        for (int i = 0; i < 4; i++)
            draw_install_choice(cx + (i % 2) * (half + 12),
                                y + 132 + (i / 2) * 70, half,
                                zones[i], 0,
                                streq(install_config.timezone, zones[i]));
    } else if (install_page == 6) {
        draw_settings_switch(cx, y + 108, install_config.wifi_enabled);
        draw_text(cx + 48, y + 105, "Save a Wi-Fi profile for first boot", UI_TEXT, 1);
        if (install_config.wifi_enabled) {
            draw_install_field(cx, y + 145, half, "Network name (SSID)",
                               install_config.wifi_ssid, 0, 4);
            draw_install_field(cx + half + 12, y + 145, half, "Password",
                               install_config.wifi_password, 1, 5);
            draw_install_choice(cx, y + 226, half, "DHCP", "Automatic address",
                                !install_config.wifi_static);
            draw_install_choice(cx + half + 12, y + 226, half, "Static",
                                "Manual address", install_config.wifi_static);
            if (install_config.wifi_static) {
                draw_install_field(cx, y + 304, half, "IP address",
                                   install_config.wifi_ip, 0, 6);
                draw_install_field(cx + half + 12, y + 304, half, "Gateway",
                                   install_config.wifi_gateway, 0, 7);
                draw_install_field(cx, y + 379, cw, "DNS server",
                                   install_config.wifi_dns, 0, 8);
            }
        } else {
            draw_text(cx, y + 180,
                      "Wi-Fi setup is skipped. Networking can be configured after boot.",
                      UI_MUTED, 1);
        }
    } else if (install_page == 7) {
        draw_text(cx, y + 98, "Scan hardware and choose a driver policy.", UI_MUTED, 1);
        flat_panel(cx, y + 130, half, 82, UI_CARD_ALT, UI_BORDER);
        draw_ui_symbol(cx + 24, y + 156, UI_SYM_CPU, UI_ACCENT, 18);
        draw_text(cx + 48, y + 148, "PCI devices", UI_TEXT, 1);
        draw_text(cx + 48, y + 176,
                  install_scan_pci < 0 ? "Not scanned" : u64_to_dec(install_scan_pci),
                  UI_MUTED, 1);
        flat_panel(cx + half + 12, y + 130, half, 82, UI_CARD_ALT, UI_BORDER);
        draw_ui_symbol(cx + half + 36, y + 156, UI_SYM_CHECK, UI_ACCENT, 18);
        draw_text(cx + half + 60, y + 148, "Available drivers", UI_TEXT, 1);
        draw_text(cx + half + 60, y + 176,
                  install_scan_drivers < 0 ? "Not scanned" : u64_to_dec(install_scan_drivers),
                  UI_MUTED, 1);
        draw_install_button(cx, y + 226, 132, "Scan devices", 1, 1);
        draw_install_choice(cx, y + 282, cw, "Stable drivers only",
                            "Recommended for hardware safety",
                            install_config.driver_mode == AOS_INSTALL_DRIVERS_STABLE);
        draw_install_choice(cx, y + 350, cw, "Include experimental drivers",
                            "May support newer devices",
                            install_config.driver_mode == AOS_INSTALL_DRIVERS_EXPERIMENTAL);
    } else if (install_page == 8) {
        draw_text(cx, y + 98, "Choose the applications available after installation.", UI_MUTED, 1);
        for (int i = 0; i < 4; i++)
            draw_install_choice(cx + (i % 2) * (half + 12),
                                y + 132 + (i / 2) * 70, half,
                                profiles[i],
                                i == 0 ? "Core system" :
                                i == 1 ? "Files, editor, terminal" :
                                i == 2 ? "All bundled AOS tools" :
                                         "Selected application mask",
                                install_config.app_profile == i);
        draw_text(cx, y + 300,
                  "afetch  aosctl  acur  Text Editor  Files  Network  Monitor  Dev tools",
                  UI_MUTED, 1);
    } else if (install_page == 9) {
        draw_text(cx, y + 98, "Choose and test the keyboard layout.", UI_MUTED, 1);
        for (int i = 0; i < 4; i++)
            draw_install_choice(cx + (i % 2) * (half + 12),
                                y + 132 + (i / 2) * 70, half,
                                keyboards[i], 0,
                                streq(install_config.keyboard, keyboards[i]));
        draw_install_field(cx, y + 298, cw, "Test keyboard input",
                           install_keyboard_test, 0, 9);
    } else if (install_page == 10) {
        draw_text(cx, y + 98, "Name this computer on local networks.", UI_MUTED, 1);
        draw_install_field(cx, y + 142, cw, "Hostname",
                           install_config.hostname, 0, 10);
        draw_text(cx, y + 224,
                  "Use lowercase letters, digits, and hyphens. Examples: aos, aos-laptop",
                  UI_MUTED, 1);
    } else if (install_page == 11) {
        draw_text(cx, y + 98, "Review the final configuration before erasing the disk.", UI_MUTED, 1);
        draw_install_summary_line(cx, y + 132, "Username", install_config.username);
        draw_install_summary_line(cx, y + 156, "Hostname", install_config.hostname);
        draw_install_summary_line(cx, y + 180, "Language", install_config.language);
        draw_install_summary_line(cx, y + 204, "Region", install_config.region);
        draw_install_summary_line(cx, y + 228, "Timezone", install_config.timezone);
        draw_install_summary_line(cx, y + 252, "Keyboard", install_config.keyboard);
        draw_install_summary_line(cx, y + 276, "Wi-Fi",
                                  install_config.wifi_enabled ? "Profile saved" : "Skipped");
        draw_install_summary_line(cx, y + 300, "Target", disk ? disk->name : "None");
        draw_install_summary_line(cx, y + 324, "Partitioning",
                                  install_config.partition_mode == AOS_INSTALL_PARTITION_AUTO
                                      ? "Automatic" : "Manual");
        draw_text(cx, y + 361,
                  "All data on the selected disk will be erased. Type AOS to confirm.",
                  UI_WARNING, 1);
        draw_install_field(cx, y + 385, cw, "Confirmation",
                           install_config.confirmation, 0, 11);
    } else if (install_page == 12) {
        draw_text(cx, y + 99, install_status.message,
                  install_status.step == AOS_INSTALL_STEP_ERROR ? UI_WARNING : UI_MUTED, 1);
        draw_bar(cx, y + 132, cw, 10, install_status.progress_percent,
                 install_status.step == AOS_INSTALL_STEP_ERROR ? UI_WARNING : UI_ACCENT);
        draw_text_right(cx + cw, y + 151,
                        u64_to_dec(install_status.progress_percent), UI_TEXT, 1);
        draw_text(cx + cw - 18, y + 151, "%", UI_TEXT, 1);
        for (int i = 1; i <= 11; i++) {
            int row = y + 183 + (i - 1) * 27;
            int done = install_status.step > (uint32_t)i;
            int current = install_status.step == (uint32_t)i;
            if (done) {
                disc(cx + 9, row + 7, 7, UI_ACCENT);
                draw_ui_symbol(cx + 9, row + 7, UI_SYM_CHECK, 0xffffff, 10);
            } else {
                circle_stroke(cx + 9, row + 7, 7, 1,
                              current ? UI_ACCENT : UI_BORDER);
            }
            draw_text(cx + 28, row + 3, install_step_label(i),
                      current ? UI_TEXT : UI_MUTED, 1);
        }
        if (install_status.step == AOS_INSTALL_STEP_ERROR)
            draw_install_button(cx + cw - 130, footer_y, 130, "Back", 0, 1);
    } else if (install_page == 13) {
        disc(cx + cw / 2, y + 174, 46, UI_ACCENT);
        draw_ui_symbol(cx + cw / 2, y + 174, UI_SYM_CHECK, 0xffffff, 46);
        draw_text_center(cx + cw / 2, y + 242, "AOS is installed", UI_TEXT, 3);
        draw_text_center(cx + cw / 2, y + 280,
                         "Remove the live media before restarting.", UI_MUTED, 1);
        draw_text_center(cx + cw / 2, y + 304,
                         "GRUB is ready for legacy BIOS and UEFI.", UI_MUTED, 1);
        draw_install_button(cx + cw / 2 - 76, y + 348, 152,
                            "Restart now", 1, 1);
    }

    if (install_page >= 1 && install_page <= 11) {
        draw_install_button(cx, footer_y, 92, "Back", 0, 1);
        if (install_page < 11) {
            draw_install_button(cx + cw - 112, footer_y, 112,
                                "Continue", 1, 1);
        } else {
            draw_install_button(cx + cw - 174, footer_y, 174,
                                "Erase disk & install", 1,
                                streq(install_config.confirmation, "AOS") && disk &&
                                install_config.partition_mode == AOS_INSTALL_PARTITION_AUTO);
        }
    }
    if (install_notice[0] && install_page != 12) {
        draw_text(cx + 104, footer_y + 13, install_notice, UI_WARNING, 1);
    }
}

static int install_pump(void) {
    if (!install_status.running) return 0;
    syscall2(AOS_SYS_INSTALLER, AOS_INSTALL_ADVANCE, 0);
    install_refresh_status();
    if (install_status.step == AOS_INSTALL_STEP_COMPLETE) install_page = 13;
    return 1;
}

static void draw_generic_app(int x, int y, int w, int h) {
    int app = active_app;
    if (app < 0 || app >= APP_COUNT) app = 0;
    panel(x + 30, y + 66, w - 60, h - 112, UI_CARD, UI_BORDER);
    draw_text(x + 54, y + 96, apps[app].name, UI_TEXT, 3);
    draw_text(x + 54, y + 142, "Native MUI app is available from the overview and dock.", UI_MUTED, 1);
}

static void active_window_bounds(int* out_x, int* out_y, int* out_w, int* out_h) {
    int sw = (int)gfx.width;
    int sh = (int)gfx.height;
    int dock_top = sh - DOCK_BOTTOM - DOCK_HEIGHT;
    int app = active_app >= 0 && active_app < APP_COUNT ? active_app : 0;
    int w = app_default_width[app];
    int h = app_default_height[app];
    int max_w = sw - 24;
    int max_h = dock_top - TOPBAR_HEIGHT - 20;
    int x = (sw - w) / 2;
    int y;

    if (max_w < 300) max_w = sw;
    if (max_h < 220) max_h = sh - TOPBAR_HEIGHT;
    if (w > max_w) w = max_w;
    if (h > max_h) h = max_h;
    y = TOPBAR_HEIGHT + (dock_top - TOPBAR_HEIGHT - h) / 2;
    if (y < TOPBAR_HEIGHT + 4) y = TOPBAR_HEIGHT + 4;

    if (window_maximized) {
        x = 8;
        y = TOPBAR_HEIGHT + 4;
        w = sw - 16;
        h = dock_top - y - 8;
    } else if (window_has_position) {
        x = window_x;
        y = window_y;
        if (x < 8) x = 8;
        if (y < 38) y = 38;
        if (x + w > sw - 8) x = sw - w - 8;
        if (y + h > sh - 88) y = sh - h - 88;
        if (x < 8) x = 8;
        if (y < 38) y = 38;
    }

    *out_x = x;
    *out_y = y;
    *out_w = w;
    *out_h = h;
}

static void remember_window_position(void) {
    int x;
    int y;
    int w;
    int h;

    active_window_bounds(&x, &y, &w, &h);
    (void)w;
    (void)h;
    window_x = x;
    window_y = y;
    window_has_position = 1;
}

static void draw_active_window(void) {
    int x;
    int y;
    int w;
    int h;

    active_window_bounds(&x, &y, &w, &h);
    draw_window_shell(x, y, w, h, apps[active_app].name);
    switch (active_app) {
        case 0: draw_files(x, y, w, h); break;
        case 1: draw_settings(x, y, w, h); break;
        case 2: draw_terminal(x, y, w, h); break;
        case 3: draw_editor(x, y, w, h); break;
        case 4: draw_system_monitor(x, y, w, h); break;
        case 5: draw_calculator(x, y, w, h); break;
        case 6: draw_calendar(x, y, w, h); break;
        case 7: draw_clocks(x, y, w, h); break;
        case 8: draw_photos(x, y, w, h); break;
        case 9: draw_disks(x, y, w, h); break;
        case 10: draw_software(x, y, w, h); break;
        case 11: draw_about(x, y, w, h); break;
        case 12: draw_installer(x, y, w, h); break;
        default: draw_generic_app(x, y, w, h); break;
    }
}

static void sync_cursor(void) {
    syscall5(AOS_SYS_GFX_CURSOR, pointer_x, pointer_y, pointer_buttons, 1, 0);
}

static void present_cursor(void) {
    sync_cursor();
    syscall1(AOS_SYS_GFX_PRESENT, 0);
}

static void draw_overview(void) {
    int w = (int)gfx.width;
    int x0 = w / 2 - 220;
    int y0 = 66;
    if (x0 < 20) x0 = 20;
    alpha_rect(0, TOPBAR_HEIGHT, (int)gfx.width,
               (int)gfx.height - TOPBAR_HEIGHT, 0x16231d, 210, 1);
    alpha_rect(0, TOPBAR_HEIGHT, (int)gfx.width, 110, 0x756d5b, 54, 1);
    alpha_round_rect(x0 + 5, y0 + 7, 440, 46, 14, 0x000000, 72, 8);
    alpha_round_rect(x0, y0, 440, 46, 14, 0xffffff, 70, 1);
    alpha_round_rect(x0 + 1, y0 + 1, 438, 44, 13, 0x5f5b50, 172, 6);
    rect(x0 + 20, y0 + 16, 10, 10, 0xd5d8cf);
    rect(x0 + 30, y0 + 25, 8, 2, 0xd5d8cf);
    draw_text(x0 + 52, y0 + 17, "Type to search apps...", 0xd9e2d6, 1);
    int grid_w = w > 760 ? 760 : w - 48;
    int start_x = (w - grid_w) / 2;
    int cols = grid_w / 124;
    int visible = 0;
    if (cols < 2) cols = 2;
    for (int i = 0; i < APP_COUNT; i++) {
        int gx;
        int gy;
        if (!app_is_available(i)) continue;
        gx = start_x + (visible % cols) * 124 + 26;
        gy = y0 + 104 + (visible / cols) * 112;
        if (i == active_app || hover_overview_app == i) {
            alpha_round_rect(gx - 12, gy - 12, 88, 104, 12, hover_overview_app == i ? 0x6f745f : 0x59614f, 150, 6);
            alpha_round_rect(gx - 11, gy - 11, 86, 102, 11, 0xffffff, 24, 1);
        }
        draw_tile(gx, gy, 64, i, i == active_app || hover_overview_app == i);
        draw_text_center(gx + 32, gy + 80, apps[i].name, UI_PANEL_TEXT, 1);
        visible++;
    }
}

static void open_app(int app) {
    if (app < 0 || !app_is_available(app)) return;
    if (app >= APP_COUNT) app = APP_COUNT - 1;
    active_app = app;
    window_open = 1;
    overview_open = 0;
    power_menu_open = 0;
    context_menu_open = 0;
    context_menu_kind = CONTEXT_KIND_NONE;
    window_dragging = 0;
    settings_slider_drag = 0;
    window_has_position = 0;
    window_maximized = 0;
    if (app == 0) files_refresh();
    if (app == 10) software_refresh_packages();
    if (app == 12) {
        install_refresh_status();
        install_refresh_disks();
        if (install_status.running) install_page = 12;
        if (install_status.step == AOS_INSTALL_STEP_COMPLETE) install_page = 13;
    }
}

static int overview_app_at(int px, int py) {
    int w = (int)gfx.width;
    int x0 = w / 2 - 220;
    int y0 = 66;
    int grid_w;
    int start_x;
    int cols;
    int visible = 0;

    if (x0 < 20) x0 = 20;
    grid_w = w > 760 ? 760 : w - 48;
    start_x = (w - grid_w) / 2;
    cols = grid_w / 124;
    if (cols < 2) cols = 2;

    for (int i = 0; i < APP_COUNT; i++) {
        int gx;
        int gy;
        if (!app_is_available(i)) continue;
        gx = start_x + (visible % cols) * 124 + 26;
        gy = y0 + 104 + (visible / cols) * 112;
        if (point_in(px, py, gx - 12, gy - 12, 88, 108)) return i;
        visible++;
    }
    return -1;
}

static int desktop_app_at(int px, int py) {
    int y = 54;
    int x = 32;
    for (int i = 0; i < APP_COUNT; i++) {
        if (!apps[i].desktop || !app_is_available(i)) continue;
        if (point_in(px, py, x - 16, y - 8, 84, 84)) return i;
        y += 86;
    }
    return -1;
}

static int dock_app_at(int px, int py, int* all_apps) {
    int w = (int)gfx.width;
    int h = (int)gfx.height;
    int pinned = 0;
    int dock_w;
    int dock_y;
    int x;
    int cx;

    *all_apps = 0;
    for (int i = 0; i < APP_COUNT; i++)
        if (apps[i].pinned && app_is_available(i)) pinned++;
    dock_w = pinned * DOCK_ITEM_STEP + 88;
    dock_y = h - DOCK_BOTTOM - DOCK_HEIGHT;
    x = (w - dock_w) / 2;
    cx = x + 10;
    for (int i = 0; i < APP_COUNT; i++) {
        if (!apps[i].pinned || !app_is_available(i)) continue;
        if (point_in(px, py, cx - 4, dock_y - 6, DOCK_ITEM_STEP, DOCK_HEIGHT + 6)) {
            return i;
        }
        cx += DOCK_ITEM_STEP;
    }
    cx += 18;
    if (point_in(px, py, cx - 4, dock_y - 6, DOCK_ITEM_STEP, DOCK_HEIGHT + 6)) {
        *all_apps = 1;
    }
    return -1;
}

static void update_hover(int x, int y) {
    int wx;
    int wy;
    int ww;
    int wh;

    hover_desktop_app = -1;
    hover_dock_app = -1;
    hover_overview_app = -1;
    hover_all_apps = 0;
    hover_close = 0;
    hover_power_action = POWER_ACTION_NONE;
    hover_context_action = CONTEXT_ACTION_NONE;
    hover_topbar_target = topbar_target_at(x, y);
    hover_activities =
        hover_topbar_target == TOPBAR_TARGET_ACTIVITIES;

    if (power_menu_open) {
        hover_power_action = power_action_at(x, y);
        return;
    }
    if (context_menu_open) {
        hover_context_action = context_action_at(x, y);
        return;
    }

    hover_dock_app = dock_app_at(x, y, &hover_all_apps);

    if (window_open) {
        active_window_bounds(&wx, &wy, &ww, &wh);
        hover_close = point_in(x, y, wx + ww - 32, wy + 4, 28, 30);
        if (point_in(x, y, wx, wy, ww, wh)) {
            return;
        }
    }

    if (overview_open) {
        hover_overview_app = overview_app_at(x, y);
    } else {
        hover_desktop_app = desktop_app_at(x, y);
    }
}

static void move_window_to_pointer(int px, int py) {
    int x;
    int y;
    int w;
    int h;

    if (!window_open || window_maximized) return;
    if (!window_has_position) remember_window_position();

    window_x = px - window_drag_offset_x;
    window_y = py - window_drag_offset_y;
    active_window_bounds(&x, &y, &w, &h);
    (void)w;
    (void)h;
    window_x = x;
    window_y = y;
}

static void calc_set_int(int value) {
    char tmp[24];
    int neg = value < 0;
    uint32_t v = neg ? (uint32_t)(-value) : (uint32_t)value;
    char* p = &tmp[23];
    *p = 0;
    if (v == 0) {
        *--p = '0';
    } else {
        while (v) {
            *--p = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    if (neg) *--p = '-';
    copy_cstr(calc_display, sizeof(calc_display), p);
}

static void calc_finish_pending(void) {
    int current = dec_to_int(calc_display);
    int result = calc_operand;
    if (calc_op == '+') result = calc_operand + current;
    else if (calc_op == '-') result = calc_operand - current;
    else if (calc_op == '*') result = calc_operand * current;
    else if (calc_op == '/' && current != 0) result = calc_operand / current;
    calc_set_int(result);
    calc_value = result;
    calc_op = 0;
    calc_fresh = 1;
}

static void calc_press(const char* key) {
    if (!key || !key[0]) return;
    if (streq(key, "C")) {
        copy_cstr(calc_display, sizeof(calc_display), "0");
        calc_value = 0;
        calc_operand = 0;
        calc_op = 0;
        calc_fresh = 1;
        return;
    }
    if (streq(key, "+/-")) {
        calc_set_int(-dec_to_int(calc_display));
        calc_fresh = 0;
        return;
    }
    if (streq(key, "%")) {
        calc_set_int(dec_to_int(calc_display) / 100);
        calc_fresh = 0;
        return;
    }
    if (streq(key, ".")) {
        int has_decimal = 0;
        for (uint64_t i = 0; calc_display[i]; i++) {
            if (calc_display[i] == '.') has_decimal = 1;
        }
        if (calc_fresh) {
            copy_cstr(calc_display, sizeof(calc_display), "0.");
            calc_fresh = 0;
        } else if (!has_decimal) {
            append_char(calc_display, sizeof(calc_display), '.');
        }
        return;
    }
    if (key[0] >= '0' && key[0] <= '9' && key[1] == 0) {
        if (calc_fresh || streq(calc_display, "0")) {
            calc_display[0] = key[0];
            calc_display[1] = 0;
        } else {
            append_char(calc_display, sizeof(calc_display), key[0]);
        }
        calc_fresh = 0;
        return;
    }
    if (streq(key, "=")) {
        if (calc_op) calc_finish_pending();
        return;
    }
    if (key[0] == '+' || key[0] == '-' || key[0] == '*' || key[0] == '/') {
        if (calc_op && !calc_fresh) calc_finish_pending();
        calc_operand = dec_to_int(calc_display);
        calc_op = key[0];
        calc_fresh = 1;
    }
}

static void terminal_add_line(const char* s) {
    int capacity = (int)(sizeof(terminal_last) / sizeof(terminal_last[0]));

    if (terminal_last_count < capacity) {
        copy_cstr(terminal_last[terminal_last_count++], sizeof(terminal_last[0]), s);
    } else {
        for (int i = 1; i < capacity; i++) {
            copy_cstr(terminal_last[i - 1], sizeof(terminal_last[0]),
                      terminal_last[i]);
        }
        copy_cstr(terminal_last[capacity - 1], sizeof(terminal_last[0]), s);
    }
}

static void terminal_reset_stream(void) {
    terminal_stream_line[0] = 0;
    terminal_stream_len = 0;
    terminal_stream_cursor = 0;
}

static void terminal_flush_stream(void) {
    if (terminal_stream_len > 0) {
        terminal_add_line(terminal_stream_line);
    }
    terminal_reset_stream();
}

static void terminal_feed_char(char c) {
    if (terminal_escape_state == 1) {
        if (c == '[') {
            terminal_escape_state = 2;
            terminal_escape_value = 0;
        } else {
            terminal_escape_state = 0;
        }
        return;
    }
    if (terminal_escape_state == 2) {
        if (c >= '0' && c <= '9') {
            terminal_escape_value =
                terminal_escape_value * 10 + (c - '0');
            return;
        }
        if (c == ';' || c == '?') return;
        if (c >= 0x40 && c <= 0x7e) {
            if (c == 'J' && terminal_escape_value == 2) {
                terminal_last_count = 0;
                terminal_reset_stream();
            }
            terminal_escape_state = 0;
        }
        return;
    }
    if ((unsigned char)c == 27) {
        terminal_escape_state = 1;
        return;
    }
    if (c == '\n') {
        terminal_add_line(terminal_stream_line);
        terminal_reset_stream();
        return;
    }
    if (c == '\r') {
        terminal_stream_cursor = 0;
        return;
    }
    if (c == '\b' || c == 127) {
        if (terminal_stream_cursor > 0) {
            terminal_stream_cursor--;
            terminal_stream_len = terminal_stream_cursor;
            terminal_stream_line[terminal_stream_len] = 0;
        }
        return;
    }
    if (c == '\t') {
        int spaces = 4 - (terminal_stream_cursor & 3);
        while (spaces-- > 0) terminal_feed_char(' ');
        return;
    }
    if ((unsigned char)c < 32) return;
    if (terminal_stream_cursor + 1 >= TERMINAL_LINE_CAP) {
        terminal_flush_stream();
    }
    terminal_stream_line[terminal_stream_cursor++] = c;
    if (terminal_stream_cursor > terminal_stream_len) {
        terminal_stream_len = terminal_stream_cursor;
    }
    terminal_stream_line[terminal_stream_len] = 0;
}

static void terminal_feed_bytes(const char* data, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        terminal_feed_char(data[i]);
    }
}

static void terminal_update_cwd(void) {
    if (syscall2(SYS_GETCWD, (long)terminal_cwd,
                 sizeof(terminal_cwd)) < 0) {
        copy_cstr(terminal_cwd, sizeof(terminal_cwd), "/");
    }
}

static void terminal_store_history(const char* command) {
    if (!settings_remember_history || !command || !command[0]) return;
    if (terminal_history_count > 0 &&
        streq(terminal_history[terminal_history_count - 1], command)) {
        terminal_history_position = terminal_history_count;
        return;
    }
    if (terminal_history_count < TERMINAL_HISTORY_CAP) {
        copy_cstr(terminal_history[terminal_history_count++],
                  sizeof(terminal_history[0]), command);
    } else {
        for (int i = 1; i < TERMINAL_HISTORY_CAP; i++) {
            copy_cstr(terminal_history[i - 1],
                      sizeof(terminal_history[0]), terminal_history[i]);
        }
        copy_cstr(terminal_history[TERMINAL_HISTORY_CAP - 1],
                  sizeof(terminal_history[0]), command);
    }
    terminal_history_position = terminal_history_count;
}

static void terminal_recall_history(int direction) {
    if (terminal_history_count == 0) return;
    if (direction < 0) {
        if (terminal_history_position > 0) terminal_history_position--;
    } else {
        if (terminal_history_position < terminal_history_count) {
            terminal_history_position++;
        }
    }
    if (terminal_history_position >= terminal_history_count) {
        terminal_input[0] = 0;
        terminal_input_len = 0;
    } else {
        copy_cstr(terminal_input, sizeof(terminal_input),
                  terminal_history[terminal_history_position]);
        terminal_input_len = (int)cstrlen(terminal_input);
    }
}

static void terminal_complete_command(void) {
    const char* match = 0;
    int matches = 0;

    for (int i = 0; terminal_input[i]; i++) {
        if (terminal_input[i] == ' ' || terminal_input[i] == '\t') return;
    }
    for (int i = 0; i < TERMINAL_COMMAND_COUNT; i++) {
        if (starts_with(terminal_commands[i], terminal_input)) {
            match = terminal_commands[i];
            matches++;
        }
    }
    if (matches == 1 && match) {
        copy_cstr(terminal_input, sizeof(terminal_input), match);
        terminal_input_len = (int)cstrlen(terminal_input);
        if (terminal_input_len + 1 < (int)sizeof(terminal_input)) {
            terminal_input[terminal_input_len++] = ' ';
            terminal_input[terminal_input_len] = 0;
        }
    } else if (matches > 1) {
        char line[TERMINAL_LINE_CAP];
        copy_cstr(line, sizeof(line), "Matches:");
        for (int i = 0; i < TERMINAL_COMMAND_COUNT; i++) {
            if (!starts_with(terminal_commands[i], terminal_input)) continue;
            if (cstrlen(line) + cstrlen(terminal_commands[i]) + 2 >=
                sizeof(line)) {
                terminal_add_line(line);
                copy_cstr(line, sizeof(line), "  ");
            } else {
                append_cstr(line, sizeof(line), " ");
            }
            append_cstr(line, sizeof(line), terminal_commands[i]);
        }
        terminal_add_line(line);
    }
}

static int terminal_parse_command(char* command, char** argv, int capacity) {
    char* src = command;
    char* dst = command;
    int argc = 0;

    while (*src) {
        char quote = 0;

        while (*src == ' ' || *src == '\t') src++;
        if (!*src || argc + 1 >= capacity) break;
        argv[argc++] = dst;
        while (*src) {
            char c = *src++;
            if (quote) {
                if (c == quote) {
                    quote = 0;
                } else if (c == '\\' && quote == '"' && *src) {
                    *dst++ = *src++;
                } else {
                    *dst++ = c;
                }
            } else if (c == '\'' || c == '"') {
                quote = c;
            } else if (c == '\\' && *src) {
                *dst++ = *src++;
            } else if (c == ' ' || c == '\t') {
                break;
            } else {
                *dst++ = c;
            }
        }
        *dst++ = 0;
    }
    argv[argc] = 0;
    return argc;
}

static void terminal_exec_child(char** argv) {
    char path[320];

    if (argv[0][0] == '/') {
        syscall3(SYS_EXECVE, (long)argv[0], (long)argv, 0);
    } else {
        copy_cstr(path, sizeof(path), "/");
        append_cstr(path, sizeof(path), argv[0]);
        syscall3(SYS_EXECVE, (long)path, (long)argv, 0);

        if (!starts_with(argv[0] + (cstrlen(argv[0]) > 4
                                       ? cstrlen(argv[0]) - 4 : 0),
                         ".elf")) {
            append_cstr(path, sizeof(path), ".elf");
            syscall3(SYS_EXECVE, (long)path, (long)argv, 0);
        }
    }
    write_cstr("aosh: ");
    write_cstr(argv[0]);
    write_cstr(": command not found\n");
    exit_code(127);
}

static int terminal_spawn(char** argv) {
    int input_pipe[2] = {-1, -1};
    int output_pipe[2] = {-1, -1};
    long pid;

    if (!argv || !argv[0] || terminal_child_pid > 0) return -1;
    if (syscall1(SYS_PIPE, (long)input_pipe) < 0) {
        terminal_add_line("aosh: could not create stdin pipe");
        return -1;
    }
    if (syscall1(SYS_PIPE, (long)output_pipe) < 0) {
        syscall1(SYS_CLOSE, input_pipe[0]);
        syscall1(SYS_CLOSE, input_pipe[1]);
        terminal_add_line("aosh: could not create output pipe");
        return -1;
    }

    pid = syscall1(SYS_FORK, 0);
    if (pid < 0) {
        syscall1(SYS_CLOSE, input_pipe[0]);
        syscall1(SYS_CLOSE, input_pipe[1]);
        syscall1(SYS_CLOSE, output_pipe[0]);
        syscall1(SYS_CLOSE, output_pipe[1]);
        terminal_add_line("aosh: fork failed");
        return -1;
    }
    if (pid == 0) {
        syscall1(SYS_CLOSE, input_pipe[1]);
        syscall1(SYS_CLOSE, output_pipe[0]);
        syscall2(SYS_DUP2, input_pipe[0], 0);
        syscall2(SYS_DUP2, output_pipe[1], 1);
        syscall2(SYS_DUP2, output_pipe[1], 2);
        syscall1(SYS_CLOSE, input_pipe[0]);
        syscall1(SYS_CLOSE, output_pipe[1]);
        terminal_exec_child(argv);
    }

    syscall1(SYS_CLOSE, input_pipe[0]);
    syscall1(SYS_CLOSE, output_pipe[1]);
    terminal_child_pid = (int)pid;
    terminal_child_stdin = input_pipe[1];
    terminal_child_stdout = output_pipe[0];
    copy_cstr(terminal_active_command, sizeof(terminal_active_command),
              argv[0]);
    terminal_reset_stream();
    return 0;
}

static int terminal_pump(void) {
    struct linux_pollfd pfd;
    char buffer[256];
    int redraw = 0;

    if (terminal_child_pid <= 0 || terminal_child_stdout < 0) return 0;
    pfd.fd = terminal_child_stdout;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (syscall3(SYS_POLL, (long)&pfd, 1, 0) <= 0) return 0;

    if (pfd.revents & POLLIN) {
        long got = syscall3(SYS_READ, terminal_child_stdout,
                            (long)buffer, sizeof(buffer));
        if (got > 0) {
            terminal_feed_bytes(buffer, (uint64_t)got);
            redraw = 1;
        }
    }
    if (pfd.revents & POLLHUP) {
        int status = 0;
        int exit_status;
        char line[48];

        for (;;) {
            long got = syscall3(SYS_READ, terminal_child_stdout,
                                (long)buffer, sizeof(buffer));
            if (got <= 0) break;
            terminal_feed_bytes(buffer, (uint64_t)got);
        }
        terminal_flush_stream();
        syscall1(SYS_CLOSE, terminal_child_stdout);
        terminal_child_stdout = -1;
        if (terminal_child_stdin >= 0) {
            syscall1(SYS_CLOSE, terminal_child_stdin);
            terminal_child_stdin = -1;
        }
        syscall4(SYS_WAIT4, terminal_child_pid, (long)&status, 0, 0);
        exit_status = (status >> 8) & 0xff;
        if (exit_status != 0) {
            copy_cstr(line, sizeof(line), "[exit ");
            append_cstr(line, sizeof(line),
                        u64_to_dec((uint64_t)exit_status));
            append_cstr(line, sizeof(line), "]");
            terminal_add_line(line);
        }
        terminal_child_pid = 0;
        terminal_active_command[0] = 0;
        terminal_update_cwd();
        redraw = 1;
    }
    return redraw;
}

static void terminal_send_char(char c) {
    if (terminal_child_stdin < 0) return;
    if (syscall3(SYS_WRITE, terminal_child_stdin, (long)&c, 1) <= 0) {
        syscall1(SYS_CLOSE, terminal_child_stdin);
        terminal_child_stdin = -1;
    }
}

static void terminal_close_stdin(void) {
    if (terminal_child_stdin >= 0) {
        syscall1(SYS_CLOSE, terminal_child_stdin);
        terminal_child_stdin = -1;
    }
}

static void terminal_stop_child(void) {
    if (terminal_child_pid <= 0) return;
    terminal_flush_stream();
    terminal_add_line("^C");
    syscall3(AOS_SYS_PROCESS_KILL, terminal_child_pid, 0, 0);
    terminal_close_stdin();
}

static void terminal_print_help(void) {
    terminal_add_line("All bundled AOS commands run here with live output.");
    terminal_add_line("Shell: sh ash test [ printf env grep find sed awk");
    terminal_add_line("Files: pwd ls cat head tail stat touch mkdir rmdir rm cp mv");
    terminal_add_line("System: ps mem uptime uname whoami id date dmesg kill pause resume");
    terminal_add_line("Storage: mount umount mounts df du partitions tar gzip");
    terminal_add_line("Network: net ip wifi bluetooth ping ping6 route neigh dhcp tcp");
    terminal_add_line("Network tools: curl acur wget https gethost netcache netstat");
    terminal_add_line("Hardware: lspci drivers firmware usb display settings vsys asence");
    terminal_add_line("Builtins: help clear cd history files exit");
    terminal_add_line("Use sh for pipelines/redirection. Tab completes commands.");
}

static void terminal_run(void) {
    char command[TERMINAL_INPUT_CAP];
    char parse_buffer[TERMINAL_INPUT_CAP];
    char prompt[192];
    char display_line[TERMINAL_LINE_CAP];
    char* argv[TERMINAL_MAX_ARGS];
    int argc;

    copy_cstr(command, sizeof(command), terminal_input);
    terminal_make_prompt(prompt, sizeof(prompt));
    copy_cstr(display_line, sizeof(display_line), prompt);
    append_cstr(display_line, sizeof(display_line), command);
    terminal_add_line(display_line);
    terminal_store_history(command);
    terminal_input[0] = 0;
    terminal_input_len = 0;
    terminal_history_position = terminal_history_count;

    copy_cstr(parse_buffer, sizeof(parse_buffer), command);
    argc = terminal_parse_command(parse_buffer, argv, TERMINAL_MAX_ARGS);
    if (argc == 0) return;

    if (streq(argv[0], "clear")) {
        terminal_last_count = 0;
        terminal_reset_stream();
    } else if (streq(argv[0], "help")) {
        terminal_print_help();
    } else if (streq(argv[0], "history")) {
        for (int i = 0; i < terminal_history_count; i++) {
            char line[TERMINAL_LINE_CAP];
            copy_cstr(line, sizeof(line), u64_to_dec((uint64_t)(i + 1)));
            append_cstr(line, sizeof(line), "  ");
            append_cstr(line, sizeof(line), terminal_history[i]);
            terminal_add_line(line);
        }
    } else if (streq(argv[0], "cd")) {
        char expanded[256];
        const char* path = argc > 1 ? argv[1] : AOS_MAIN_HOME;
        if (streq(path, "~")) {
            path = AOS_MAIN_HOME;
        } else if (starts_with(path, "~/")) {
            copy_cstr(expanded, sizeof(expanded), AOS_MAIN_HOME "/");
            append_cstr(expanded, sizeof(expanded), path + 2);
            path = expanded;
        }
        if (argc > 2) {
            terminal_add_line("cd: too many arguments");
        } else if (syscall1(SYS_CHDIR, (long)path) < 0) {
            terminal_add_line("cd: directory not found");
        } else {
            terminal_update_cwd();
        }
    } else if (streq(argv[0], "files")) {
        open_app(0);
    } else if (streq(argv[0], "exit")) {
        window_open = 0;
    } else {
        if (streq(argv[0], "neofetch")) argv[0] = "afetch";
        terminal_spawn(argv);
    }
}

static void calendar_shift_month(int delta) {
    calendar_month += delta;
    if (calendar_month < 1) {
        calendar_month = 12;
        calendar_year--;
    } else if (calendar_month > 12) {
        calendar_month = 1;
        calendar_year++;
    }
    if (calendar_selected > month_days(calendar_year, calendar_month)) {
        calendar_selected = month_days(calendar_year, calendar_month);
    }
}

static int files_item_at(int px, int py, int wx, int wy, int ww, int wh) {
    int sidebar_w = 208;
    int content_x = wx + sidebar_w;
    int content_w = ww - sidebar_w;
    int visible_capacity = files_visible_capacity(ww, wh);

    if (files_view == 0) {
        int cols = content_w >= 560 ? 5 : 4;
        int cell_w = (content_w - 24) / cols;
        for (int slot = 0;
             slot < visible_capacity &&
             files_scroll + slot < file_entry_count;
             slot++) {
            int fx = content_x + 12 + (slot % cols) * cell_w;
            int fy = wy + 100 + (slot / cols) * 100;
            if (point_in(px, py, fx, fy, cell_w, 84)) {
                return files_scroll + slot;
            }
        }
    } else {
        for (int slot = 0;
             slot < visible_capacity &&
             files_scroll + slot < file_entry_count;
             slot++) {
            int ry = wy + 105 + slot * 38;
            if (point_in(px, py, content_x + 12, ry,
                         content_w - 24, 34)) {
                return files_scroll + slot;
            }
        }
    }
    return -1;
}

static void files_select_name(const char* name) {
    files_selected = -1;
    for (int i = 0; i < file_entry_count; i++) {
        if (streq(file_entries[i].name, name)) {
            files_selected = i;
            return;
        }
    }
}

static int copy_regular_file(const char* source, const char* destination) {
    char buffer[1024];
    long source_fd;
    long destination_fd;
    int failed = 0;

    source_fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)source,
                         O_RDONLY, 0);
    if (source_fd < 0) return -1;
    destination_fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)destination,
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destination_fd < 0) {
        syscall1(SYS_CLOSE, source_fd);
        return -1;
    }

    for (;;) {
        long got = syscall3(SYS_READ, source_fd, (long)buffer,
                            sizeof(buffer));
        if (got < 0) {
            failed = 1;
            break;
        }
        if (got == 0) break;
        if (write_fd_all((int)destination_fd, buffer,
                         (uint64_t)got) != 0) {
            failed = 1;
            break;
        }
    }
    syscall1(SYS_CLOSE, source_fd);
    syscall1(SYS_CLOSE, destination_fd);
    if (failed) {
        syscall3(SYS_UNLINKAT, AT_FDCWD, (long)destination, 0);
        return -1;
    }
    return 0;
}

static int files_path_is_package(const char* path) {
    return ends_with(path, ".deb") ||
           ends_with(path, ".ainstall") ||
           ends_with(path, ".AppImage") ||
           ends_with(path, ".appimage");
}

static int files_open_selected(void) {
    char path[256];

    if (files_selected < 0 || files_selected >= file_entry_count ||
        file_entry_path(files_selected, path, sizeof(path)) != 0) {
        return -1;
    }
    if (file_entries[files_selected].is_dir) {
        return files_set_path(path);
    }
    if (files_path_is_package(path)) {
        if (software_start_uni(SOFTWARE_ACTION_INSTALL, "install", path,
                               file_entries[files_selected].name) == 0) {
            open_app(10);
            return 0;
        }
        copy_cstr(files_status, sizeof(files_status),
                  "Could not start package installer");
        return -1;
    }
    if (editor_load_file(path, file_entries[files_selected].name) == 0) {
        open_app(3);
        return 0;
    }
    copy_cstr(files_status, sizeof(files_status), "Could not open file");
    return -1;
}

static int files_duplicate_selected(void) {
    char source[256];
    char destination[256];
    char name[96];
    char base[96];

    if (files_selected < 0 || files_selected >= file_entry_count ||
        file_entries[files_selected].is_dir ||
        file_entry_path(files_selected, source, sizeof(source)) != 0) {
        return -1;
    }
    copy_cstr(base, sizeof(base), "Copy of ");
    append_cstr(base, sizeof(base), file_entries[files_selected].name);
    if (choose_unique_path(files_current_path, base, "",
                           name, sizeof(name),
                           destination, sizeof(destination)) != 0 ||
        copy_regular_file(source, destination) != 0) {
        copy_cstr(files_status, sizeof(files_status),
                  "Could not duplicate file");
        return -1;
    }
    files_refresh();
    files_select_name(name);
    copy_cstr(files_status, sizeof(files_status), "Created ");
    append_cstr(files_status, sizeof(files_status), name);
    return 0;
}

static int files_move_selected_to_trash(void) {
    char source[256];
    char destination[256];
    char name[96];

    if (files_selected < 0 || files_selected >= file_entry_count ||
        file_entries[files_selected].is_dir ||
        file_entry_path(files_selected, source, sizeof(source)) != 0) {
        return -1;
    }
    if (choose_unique_path(AOS_MAIN_HOME "/Trash",
                           file_entries[files_selected].name, "",
                           name, sizeof(name),
                           destination, sizeof(destination)) != 0 ||
        copy_regular_file(source, destination) != 0) {
        copy_cstr(files_status, sizeof(files_status),
                  "Could not move file to Trash");
        return -1;
    }
    if (syscall3(SYS_UNLINKAT, AT_FDCWD, (long)source, 0) < 0) {
        syscall3(SYS_UNLINKAT, AT_FDCWD, (long)destination, 0);
        copy_cstr(files_status, sizeof(files_status),
                  "Could not move file to Trash");
        return -1;
    }
    files_refresh();
    copy_cstr(files_status, sizeof(files_status), "Moved to Trash");
    return 0;
}

static int files_delete_selected(void) {
    char path[256];
    long flags;

    if (files_selected < 0 || files_selected >= file_entry_count ||
        file_entry_path(files_selected, path, sizeof(path)) != 0) {
        return -1;
    }
    flags = file_entries[files_selected].is_dir ? AT_REMOVEDIR : 0;
    if (syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, flags) < 0) {
        copy_cstr(files_status, sizeof(files_status),
                  file_entries[files_selected].is_dir
                      ? "Folder must be empty"
                      : "Could not delete file");
        return -1;
    }
    files_refresh();
    copy_cstr(files_status, sizeof(files_status), "Deleted permanently");
    return 0;
}

static void files_ensure_selected_visible(int window_width,
                                          int window_height) {
    int capacity = files_visible_capacity(window_width, window_height);
    int maximum = file_entry_count - capacity;

    if (capacity < 1 || files_selected < 0) return;
    if (maximum < 0) maximum = 0;
    if (files_selected < files_scroll) {
        files_scroll = files_selected;
    } else if (files_selected >= files_scroll + capacity) {
        files_scroll = files_selected - capacity + 1;
    }
    if (files_view == 0) {
        int columns = window_width - 208 >= 560 ? 5 : 4;
        files_scroll = (files_scroll / columns) * columns;
    }
    if (files_scroll > maximum) files_scroll = maximum;
    if (files_scroll < 0) files_scroll = 0;
}

static void files_move_selection(int delta, int window_width,
                                 int window_height) {
    if (file_entry_count <= 0) {
        files_selected = -1;
        return;
    }
    if (files_selected < 0) {
        files_selected = delta < 0 ? file_entry_count - 1 : 0;
    } else {
        files_selected += delta;
        if (files_selected < 0) files_selected = 0;
        if (files_selected >= file_entry_count) {
            files_selected = file_entry_count - 1;
        }
    }
    files_ensure_selected_visible(window_width, window_height);
}

static void execute_context_action(int action) {
    char name[64];
    char path[256];
    const char* directory;

    context_menu_open = 0;
    context_menu_kind = CONTEXT_KIND_NONE;
    hover_context_action = CONTEXT_ACTION_NONE;

    if (action == CONTEXT_ACTION_OPEN_FILE) {
        if (files_open_selected() != 0) {
            copy_cstr(files_status, sizeof(files_status),
                      "Could not open item");
        }
    } else if (action == CONTEXT_ACTION_NEW_FOLDER) {
        directory = files_current_path;
        if (!directory ||
            choose_unique_path(directory, "New Folder", "",
                               name, sizeof(name),
                               path, sizeof(path)) != 0 ||
            syscall3(SYS_MKDIRAT, AT_FDCWD, (long)path, 0755) < 0) {
            copy_cstr(files_status, sizeof(files_status),
                      "Could not create folder");
        } else {
            files_refresh();
            files_select_name(name);
            copy_cstr(files_status, sizeof(files_status), "Created ");
            append_cstr(files_status, sizeof(files_status), name);
        }
    } else if (action == CONTEXT_ACTION_NEW_TEXT_FILE) {
        directory = files_current_path;
        editor_new_document(directory, "New Text File");
        if (editor_save_file() == 0) {
            open_app(3);
        } else {
            copy_cstr(files_status, sizeof(files_status),
                      "Could not create file");
        }
    } else if (action == CONTEXT_ACTION_NEW_DOCUMENT) {
        editor_new_document(AOS_MAIN_HOME, "Untitled");
        open_app(3);
    } else if (action == CONTEXT_ACTION_SAVE) {
        editor_save_file();
    } else if (action == CONTEXT_ACTION_OPEN_FILES) {
        open_app(0);
    } else if (action == CONTEXT_ACTION_OPEN_TERMINAL) {
        open_app(2);
    } else if (action == CONTEXT_ACTION_OPEN_TERMINAL_HERE) {
        if (syscall1(SYS_CHDIR, (long)files_current_path) < 0) {
            copy_cstr(files_status, sizeof(files_status),
                      "Could not open Terminal here");
        } else {
            terminal_update_cwd();
            open_app(2);
        }
    } else if (action == CONTEXT_ACTION_REFRESH) {
        files_refresh();
        fetch_system();
        copy_cstr(files_status, sizeof(files_status), "Folder refreshed");
    } else if (action == CONTEXT_ACTION_CLEAR_DOCUMENT) {
        editor_clear_content();
    } else if (action == CONTEXT_ACTION_DUPLICATE) {
        files_duplicate_selected();
    } else if (action == CONTEXT_ACTION_MOVE_TO_TRASH) {
        files_move_selected_to_trash();
    } else if (action == CONTEXT_ACTION_DELETE_PERMANENTLY) {
        files_delete_selected();
    }
}

static void handle_right_click(int x, int y) {
    int wx;
    int wy;
    int ww;
    int wh;

    power_menu_open = 0;
    window_dragging = 0;
    settings_slider_drag = 0;
    if (y < TOPBAR_HEIGHT || overview_open) {
        context_menu_open = 0;
        return;
    }

    if (window_open) {
        active_window_bounds(&wx, &wy, &ww, &wh);
        if (point_in(x, y, wx, wy, ww, wh)) {
            if (active_app == 0 &&
                point_in(x, y, wx + 208, wy + 92,
                         ww - 208, wh - 122)) {
                int selected = files_item_at(x, y, wx, wy, ww, wh);
                files_selected = selected;
                open_context_menu(CONTEXT_KIND_FILES, x, y);
            } else if (active_app == 3 &&
                       point_in(x, y, wx + 1, wy + 36,
                                ww - 2, wh - 37)) {
                open_context_menu(CONTEXT_KIND_EDITOR, x, y);
            } else {
                context_menu_open = 0;
            }
            return;
        }
    }

    if (y >= (int)gfx.height - DOCK_BOTTOM - DOCK_HEIGHT - 8) {
        context_menu_open = 0;
        return;
    }
    open_context_menu(CONTEXT_KIND_DESKTOP, x, y);
}

static int settings_slider_value_at(int px, int x, int w) {
    int value;
    if (px <= x) return 0;
    if (px >= x + w) return 100;
    value = (px - x) * 100 / w;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    return value;
}

static void settings_update_slider(int px, int wx, int ww) {
    int content_x;
    int card_w;
    int slider_x;

    settings_content_geometry(wx, ww, &content_x, &card_w);
    slider_x = content_x + card_w - 176;
    if (settings_slider_drag == 1) {
        settings_volume = settings_slider_value_at(px, slider_x, 144);
    } else if (settings_slider_drag == 2) {
        settings_input_volume = settings_slider_value_at(px, slider_x, 144);
    } else if (settings_slider_drag == 3) {
        settings_brightness = clamp_int(
            settings_slider_value_at(px, slider_x, 144), 10, 100);
    }
    settings_config_dirty = 1;
}

static void handle_settings_click(int px, int py, int wx, int wy, int ww) {
    int content_x;
    int card_w;

    for (int i = 0; i < 10; i++) {
        int ry = wy + 96 + i * 39;
        if (point_in(px, py, wx + 12, ry, 216, 36)) {
            settings_section = i;
            settings_slider_drag = 0;
            return;
        }
    }

    settings_content_geometry(wx, ww, &content_x, &card_w);
    if (settings_section == 3) {
        int theme_x = content_x + card_w - 126;
        int swatch_x = content_x + card_w - 126;
        if (point_in(px, py, theme_x, wy + 145, 56, 30)) {
            settings_theme_dark = 0;
            settings_commit("Light theme applied");
            return;
        }
        if (point_in(px, py, theme_x + 62, wy + 145, 56, 30)) {
            settings_theme_dark = 1;
            settings_commit("Dark theme applied");
            return;
        }
        for (int i = 0; i < 4; i++) {
            int cx = swatch_x + 12 + i * 34;
            if (point_in(px, py, cx - 15, wy + 209, 30, 30)) {
                settings_accent = i;
                settings_commit("Accent color applied");
                return;
            }
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 294, 52, 36)) {
            settings_animations = !settings_animations;
            settings_commit(settings_animations ? "Animations enabled"
                                                : "Animations disabled");
            return;
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 358, 52, 36)) {
            settings_transparency = !settings_transparency;
            settings_commit(settings_transparency ? "Transparency enabled"
                                                  : "Transparency disabled");
            return;
        }
    } else if (settings_section == 4) {
        int slider_x = content_x + card_w - 176;
        if (point_in(px, py, slider_x - 8, wy + 142, 160, 36)) {
            settings_slider_drag = 1;
            settings_update_slider(px, wx, ww);
            return;
        }
        if (point_in(px, py, slider_x - 8, wy + 206, 160, 36)) {
            settings_slider_drag = 2;
            settings_update_slider(px, wx, ww);
            return;
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 270, 52, 36)) {
            settings_system_sounds = !settings_system_sounds;
            settings_commit(settings_system_sounds ? "System sounds enabled"
                                                   : "System sounds disabled");
            return;
        }
    } else if (settings_section == 2) {
        int slider_x = content_x + card_w - 176;
        if (point_in(px, py, slider_x - 8, wy + 206, 160, 36)) {
            settings_slider_drag = 3;
            settings_update_slider(px, wx, ww);
            return;
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 270, 52, 36)) {
            settings_night_light = !settings_night_light;
            settings_commit(settings_night_light ? "Night Light enabled"
                                                 : "Night Light disabled");
            return;
        }
    } else if (settings_section == 0) {
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 142, 52, 36)) {
            settings_wifi = !settings_wifi;
            return;
        }
    } else if (settings_section == 5) {
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 142, 52, 36)) {
            settings_notifications = !settings_notifications;
            settings_commit(settings_notifications
                                ? "Notifications enabled"
                                : "Notifications disabled");
            return;
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 206, 52, 36)) {
            settings_do_not_disturb = !settings_do_not_disturb;
            settings_commit(settings_do_not_disturb
                                ? "Do Not Disturb enabled"
                                : "Do Not Disturb disabled");
            return;
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 270, 52, 36)) {
            settings_notification_banners = !settings_notification_banners;
            settings_commit(settings_notification_banners
                                ? "Banner previews enabled"
                                : "Banner previews disabled");
            return;
        }
    } else if (settings_section == 6) {
        int choice_w = (card_w - 32) / 3;
        int choice_x = content_x + 12;
        for (int i = 0; i < 3; i++) {
            if (point_in(px, py, choice_x + i * (choice_w + 4),
                         wy + 176, choice_w, 30)) {
                settings_power_mode = i;
                settings_commit(i == 0 ? "Power Saver applied"
                                       : i == 2 ? "Performance mode applied"
                                                : "Balanced mode applied");
                return;
            }
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 230, 52, 36)) {
            settings_reduce_motion_saver = !settings_reduce_motion_saver;
            settings_commit(settings_reduce_motion_saver
                                ? "Saver motion reduction enabled"
                                : "Saver motion reduction disabled");
            return;
        }
    } else if (settings_section == 7) {
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 142, 52, 36)) {
            settings_remember_history = !settings_remember_history;
            if (!settings_remember_history) {
                terminal_history_count = 0;
                terminal_history_position = 0;
            }
            settings_commit(settings_remember_history
                                ? "Command history enabled"
                                : "Command history disabled");
            return;
        }
        if (point_in(px, py, content_x + card_w - 94,
                     wy + 209, 76, 30)) {
            terminal_history_count = 0;
            terminal_history_position = 0;
            terminal_input[0] = 0;
            terminal_input_len = 0;
            settings_notify("Command history cleared");
            return;
        }
    } else if (settings_section == 8) {
        if (point_in(px, py, content_x + card_w - 94,
                     wy + 209, 76, 30)) {
            copy_cstr(terminal_input, sizeof(terminal_input), "ip addr");
            terminal_input_len = (int)cstrlen(terminal_input);
            open_app(2);
            terminal_run();
            return;
        }
    } else if (settings_section == 9) {
        if (point_in(px, py, content_x + card_w - 94,
                     wy + 209, 76, 30)) {
            files_set_path(AOS_MAIN_HOME);
            open_app(0);
            return;
        }
    } else {
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 142, 52, 36)) {
            settings_feature_enabled[settings_section] =
                !settings_feature_enabled[settings_section];
            return;
        }
        if (point_in(px, py, content_x + card_w - 60,
                     wy + 270, 52, 36)) {
            settings_auto_updates[settings_section] =
                !settings_auto_updates[settings_section];
            return;
        }
    }
}

static int install_try_begin(void) {
    const struct aos_blkdev_user* disk = install_selected_disk();

    if (cstrlen(install_config.password) < 8U ||
        !streq(install_config.password, install_config.password_confirm)) {
        copy_cstr(install_notice, sizeof(install_notice),
                  "Password must be at least 8 characters and match");
        install_page = 1;
        install_focus = 2;
        return -1;
    }
    if (!streq(install_config.confirmation, "AOS")) {
        copy_cstr(install_notice, sizeof(install_notice),
                  "Type AOS exactly to confirm disk erasure");
        install_focus = 11;
        return -1;
    }
    if (!disk) {
        copy_cstr(install_notice, sizeof(install_notice),
                  "No writable installation disk is available");
        return -1;
    }
    if (install_config.partition_mode != AOS_INSTALL_PARTITION_AUTO) {
        copy_cstr(install_notice, sizeof(install_notice),
                  "Select Automatic partitioning to install");
        return -1;
    }

    install_config.target_blkdev_id = disk->id;
    if (syscall2(AOS_SYS_INSTALLER, AOS_INSTALL_BEGIN,
                 (long)&install_config) < 0) {
        install_refresh_status();
        copy_cstr(install_notice, sizeof(install_notice),
                  install_status.message);
        return -1;
    }

    install_refresh_status();
    install_page = 12;
    install_focus = 0;
    install_notice[0] = 0;
    return 0;
}

static void install_handle_click(int px, int py, int wx, int wy, int ww,
                                 int wh) {
    int cx = wx + 229;
    int cw = ww - 253;
    int half = (cw - 12) / 2;
    int footer_y = wy + wh - 54;
    const char* languages[] = {"English", "Hindi", "Other"};
    const char* regions[] = {"India", "United States", "United Kingdom", "Other"};
    const char* zones[] = {"Asia/Kolkata", "UTC", "America/New_York", "Europe/London"};
    const char* keyboards[] = {"US English", "UK English", "Indian English", "Hindi / InScript"};

    install_notice[0] = 0;
    if (install_page == 0) {
        if (point_in(px, py, cx + cw - 150, footer_y, 150, 36) &&
            install_status.live_mode && install_status.payload_ready &&
            install_selected_disk()) {
            install_page = 1;
            install_focus = 1;
        }
        return;
    }
    if (install_page == 1) {
        if (point_in(px, py, cx, wy + 150, cw, 38)) install_focus = 1;
        else if (point_in(px, py, cx, wy + 225, half, 38)) install_focus = 2;
        else if (point_in(px, py, cx + half + 12, wy + 225, half, 38)) install_focus = 3;
        else if (point_in(px, py, cx, wy + 296, half, 58)) install_config.account_admin = 1;
        else if (point_in(px, py, cx + half + 12, wy + 296, half, 58)) install_config.account_admin = 0;
        else if (point_in(px, py, cx, wy + 376, cw, 36)) install_config.auto_login = !install_config.auto_login;
    } else if (install_page == 2) {
        if (point_in(px, py, cx, wy + 130, cw, 86) && install_disk_count > 0) {
            int selected = 0;
            for (int i = 0; i < install_disk_count; i++)
                if (install_disks[i].id == install_config.target_blkdev_id) selected = i;
            selected = (selected + 1) % install_disk_count;
            install_config.target_blkdev_id = install_disks[selected].id;
        } else if (point_in(px, py, cx, wy + 242, cw, 58)) {
            install_config.partition_mode = AOS_INSTALL_PARTITION_AUTO;
        } else if (point_in(px, py, cx, wy + 310, cw, 58)) {
            install_config.partition_mode = AOS_INSTALL_PARTITION_MANUAL;
            copy_cstr(install_notice, sizeof(install_notice),
                      "Manual layout needs the partition command");
        }
    } else if (install_page == 3) {
        for (int i = 0; i < 3; i++)
            if (point_in(px, py, cx, wy + 132 + i * 70, cw, 58))
                copy_cstr(install_config.language,
                          sizeof(install_config.language), languages[i]);
    } else if (install_page == 4) {
        for (int i = 0; i < 4; i++)
            if (point_in(px, py, cx + (i % 2) * (half + 12),
                         wy + 132 + (i / 2) * 70, half, 58))
                copy_cstr(install_config.region,
                          sizeof(install_config.region), regions[i]);
    } else if (install_page == 5) {
        for (int i = 0; i < 4; i++)
            if (point_in(px, py, cx + (i % 2) * (half + 12),
                         wy + 132 + (i / 2) * 70, half, 58))
                copy_cstr(install_config.timezone,
                          sizeof(install_config.timezone), zones[i]);
    } else if (install_page == 6) {
        if (point_in(px, py, cx, wy + 100, cw, 38)) {
            install_config.wifi_enabled = !install_config.wifi_enabled;
            install_focus = 0;
        } else if (install_config.wifi_enabled &&
                   point_in(px, py, cx, wy + 165, half, 38)) install_focus = 4;
        else if (install_config.wifi_enabled &&
                 point_in(px, py, cx + half + 12, wy + 165, half, 38)) install_focus = 5;
        else if (install_config.wifi_enabled &&
                 point_in(px, py, cx, wy + 226, half, 58)) install_config.wifi_static = 0;
        else if (install_config.wifi_enabled &&
                 point_in(px, py, cx + half + 12, wy + 226, half, 58)) install_config.wifi_static = 1;
        else if (install_config.wifi_static &&
                 point_in(px, py, cx, wy + 324, half, 38)) install_focus = 6;
        else if (install_config.wifi_static &&
                 point_in(px, py, cx + half + 12, wy + 324, half, 38)) install_focus = 7;
        else if (install_config.wifi_static &&
                 point_in(px, py, cx, wy + 399, cw, 38)) install_focus = 8;
    } else if (install_page == 7) {
        if (point_in(px, py, cx, wy + 226, 132, 36)) {
            install_scan_pci = install_count_service(AOS_SYS_PCI_INFO);
            install_scan_drivers = install_count_service(AOS_SYS_DRIVER_INFO);
            install_config.driver_scan_mask = 0x0f;
        } else if (point_in(px, py, cx, wy + 282, cw, 58)) {
            install_config.driver_mode = AOS_INSTALL_DRIVERS_STABLE;
        } else if (point_in(px, py, cx, wy + 350, cw, 58)) {
            install_config.driver_mode = AOS_INSTALL_DRIVERS_EXPERIMENTAL;
        }
    } else if (install_page == 8) {
        for (int i = 0; i < 4; i++) {
            if (point_in(px, py, cx + (i % 2) * (half + 12),
                         wy + 132 + (i / 2) * 70, half, 58)) {
                install_config.app_profile = (uint8_t)i;
                install_config.app_mask = i == AOS_INSTALL_APPS_MINIMAL ? 0 :
                                          i == AOS_INSTALL_APPS_BASIC ? 0x1f : 0xff;
            }
        }
    } else if (install_page == 9) {
        for (int i = 0; i < 4; i++)
            if (point_in(px, py, cx + (i % 2) * (half + 12),
                         wy + 132 + (i / 2) * 70, half, 58))
                copy_cstr(install_config.keyboard,
                          sizeof(install_config.keyboard), keyboards[i]);
        if (point_in(px, py, cx, wy + 318, cw, 38)) install_focus = 9;
    } else if (install_page == 10) {
        if (point_in(px, py, cx, wy + 162, cw, 38)) install_focus = 10;
    } else if (install_page == 11) {
        if (point_in(px, py, cx, wy + 405, cw, 38)) install_focus = 11;
        if (point_in(px, py, cx + cw - 174, footer_y, 174, 36)) {
            install_try_begin();
            return;
        }
    } else if (install_page == 12) {
        if (install_status.step == AOS_INSTALL_STEP_ERROR &&
            point_in(px, py, cx + cw - 130, footer_y, 130, 36)) {
            install_page = 11;
            copy_cstr(install_notice, sizeof(install_notice),
                      install_status.message);
        }
        return;
    } else if (install_page == 13) {
        if (point_in(px, py, cx + cw / 2 - 76, wy + 348, 152, 36))
            syscall1(AOS_SYS_RESTART, 0);
        return;
    }

    if (install_page >= 1 && install_page <= 11) {
        if (point_in(px, py, cx, footer_y, 92, 36)) {
            install_page--;
            install_focus = 0;
            return;
        }
        if (install_page < 11 &&
            point_in(px, py, cx + cw - 112, footer_y, 112, 36)) {
            if (install_page == 1 &&
                (!install_config.username[0] ||
                 cstrlen(install_config.password) < 8U ||
                 !streq(install_config.password,
                        install_config.password_confirm))) {
                copy_cstr(install_notice, sizeof(install_notice),
                          "Username required; password needs 8 characters and must match");
                return;
            }
            if (install_page == 6 && install_config.wifi_enabled &&
                !install_config.wifi_ssid[0]) {
                copy_cstr(install_notice, sizeof(install_notice),
                          "Enter a Wi-Fi name or skip Wi-Fi");
                return;
            }
            if (install_page == 10 && !install_config.hostname[0]) {
                copy_cstr(install_notice, sizeof(install_notice),
                          "Hostname is required");
                return;
            }
            install_page++;
            install_focus = 0;
        }
    }
}

static void handle_app_click(int px, int py, int wx, int wy, int ww, int wh) {
    if (active_app == 1) {
        handle_settings_click(px, py, wx, wy, ww);
    } else if (active_app == 0) {
        for (int i = 0; i < 8; i++) {
            int ry = wy + 45 + i * 38;
            if (point_in(px, py, wx + 10, ry, 188, 34)) {
                files_set_path(files_place_path(i));
                return;
            }
        }
        if (point_in(px, py, wx + 10, wy + 363, 188, 38)) {
            files_set_path("/");
            return;
        }
        if (point_in(px, py, wx + 218, wy + 47, 32, 34)) {
            files_set_path(AOS_MAIN_HOME);
            return;
        }
        if (point_in(px, py, wx + 254, wy + 47, 32, 34)) {
            files_go_parent();
            return;
        }
        if (point_in(px, py, wx + ww - 84, wy + 48, 34, 34)) {
            files_view = 0;
            files_scroll = 0;
            return;
        }
        if (point_in(px, py, wx + ww - 50, wy + 48, 34, 34)) {
            files_view = 1;
            files_scroll = 0;
            return;
        }
        {
            int selected = files_item_at(px, py, wx, wy, ww, wh);
            if (selected >= 0) {
                struct aos_uptime_info now;
                uint64_t threshold;
                int double_click = 0;

                if (syscall3(AOS_SYS_UPTIME_INFO, (long)&now, 0, 0) >= 0) {
                    threshold = now.frequency ? now.frequency / 2 : 50;
                    if (threshold < 1) threshold = 1;
                    double_click =
                        selected == files_last_clicked &&
                        now.ticks >= files_last_click_ticks &&
                        now.ticks - files_last_click_ticks <= threshold;
                    files_last_click_ticks = now.ticks;
                }
                files_selected = selected;
                files_last_clicked = selected;
                if (double_click) files_open_selected();
            } else {
                files_selected = -1;
                files_last_clicked = -1;
            }
        }
    } else if (active_app == 3) {
        if (point_in(px, py, wx + ww - 84, wy + 43, 68, 30)) {
            editor_save_file();
        }
    } else if (active_app == 5) {
        const char* keys[] = {"C", "+/-", "%", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", ".", "="};
        int content_x = wx + 17;
        int content_w = ww - 34;
        int key_w = (content_w - 24) / 4;
        int key_h = 56;
        int display_y = wy + 52;
        int display_h = wh - 36 - 32 - 12 - (key_h * 5 + 32);
        int keys_y;
        if (display_h < 84) display_h = 84;
        keys_y = display_y + display_h + 12;
        for (int i = 0; i < 16; i++) {
            int kx = content_x + (i % 4) * (key_w + 8);
            int ky = keys_y + (i / 4) * (key_h + 8);
            if (point_in(px, py, kx, ky, key_w, key_h)) calc_press(keys[i]);
        }
        if (point_in(px, py, content_x, keys_y + 4 * (key_h + 8),
                     key_w * 2 + 8, key_h)) {
            calc_press(keys[16]);
        }
        if (point_in(px, py, content_x + 2 * (key_w + 8),
                     keys_y + 4 * (key_h + 8), key_w, key_h)) {
            calc_press(keys[17]);
        }
        if (point_in(px, py, content_x + 3 * (key_w + 8),
                     keys_y + 4 * (key_h + 8), key_w, key_h)) {
            calc_press(keys[18]);
        }
    } else if (active_app == 6) {
        int aside_w = 256;
        int content_w = ww - aside_w;
        int grid_x = wx + 20;
        int grid_y = wy + 120;
        int cell_w = (content_w - 40) / 7;
        int cell_h = (wh - 150) / 6;
        int first_day = date_weekday(calendar_year, calendar_month, 1);
        int days = month_days(calendar_year, calendar_month);
        if (cell_h < 40) cell_h = 40;
        if (point_in(px, py, wx + content_w - 158, wy + 54, 34, 30)) {
            calendar_shift_month(-1);
            return;
        }
        if (point_in(px, py, wx + content_w - 118, wy + 54, 70, 30)) {
            calendar_year = time_info.year;
            calendar_month = time_info.month;
            calendar_selected = time_info.day;
            return;
        }
        if (point_in(px, py, wx + content_w - 42, wy + 54, 34, 30)) {
            calendar_shift_month(1);
            return;
        }
        for (int day = 1; day <= days; day++) {
            int idx = first_day + day - 1;
            int cx = grid_x + (idx % 7) * cell_w;
            int cy = grid_y + (idx / 7) * cell_h;
            if (point_in(px, py, cx, cy, cell_w, cell_h)) calendar_selected = day;
        }
    } else if (active_app == 8) {
        int image_h = wh - 36 - 58;
        int nav_y = wy + 36 + image_h / 2;
        if (point_in(px, py, wx + 14, nav_y - 18, 36, 36) ||
            point_in(px, py, wx + ww - 50, nav_y - 18, 36, 36)) {
            photo_index = photo_index ? 0 : 1;
        }
    } else if (active_app == 10) {
        static const int category_widths[] = {52, 92, 62, 82};
        int category_x = wx + 22;
        int col_w = (ww - 52) / 2;
        int rows = (wh - 205) / 92;
        int capacity;
        int filtered = software_filtered_count();
        int maximum_scroll;

        if (rows < 1) rows = 1;
        capacity = rows * 2;
        maximum_scroll = filtered - capacity;
        if (maximum_scroll < 0) maximum_scroll = 0;

        if (point_in(px, py, wx + ww - 222, wy + 51, 108, 34)) {
            files_set_path(AOS_MAIN_HOME "/Downloads");
            open_app(0);
            return;
        }
        if (point_in(px, py, wx + ww - 104, wy + 51, 82, 34)) {
            if (software_refresh_packages() == 0) {
                copy_cstr(software_status, sizeof(software_status),
                          "Package list refreshed");
            } else {
                copy_cstr(software_status, sizeof(software_status),
                          "Could not read Uni package database");
            }
            software_scroll = 0;
            return;
        }
        for (int i = 0; i < 4; i++) {
            if (point_in(px, py, category_x, wy + 102,
                         category_widths[i], 28)) {
                software_category = i;
                software_scroll = 0;
                return;
            }
            category_x += category_widths[i] + 8;
        }
        for (int slot = 0; slot < capacity; slot++) {
            struct mui_package_entry* package =
                software_package_at(software_scroll + slot);
            int col = slot % 2;
            int row = slot / 2;
            int cx = wx + 18 + col * (col_w + 16);
            int cy = wy + 164 + row * 92;

            if (!package) break;
            if (point_in(px, py, cx + col_w - 108, cy + 13,
                         62, 28)) {
                if (streq(package->compatibility, "aos-native")) {
                    software_start_uni(SOFTWARE_ACTION_RUN, "run",
                                       package->id, package->name);
                } else {
                    copy_cstr(software_status, sizeof(software_status),
                              "This package needs a Linux runtime");
                }
                return;
            }
            if (point_in(px, py, cx + col_w - 39, cy + 13,
                         29, 28)) {
                software_start_uni(SOFTWARE_ACTION_REMOVE, "remove",
                                   package->id, package->name);
                return;
            }
        }
        if (filtered > capacity) {
            if (point_in(px, py, wx + ww / 2 - 62,
                         wy + wh - 31, 32, 24)) {
                software_scroll -= 2;
                if (software_scroll < 0) software_scroll = 0;
                return;
            }
            if (point_in(px, py, wx + ww / 2 + 30,
                         wy + wh - 31, 32, 24)) {
                software_scroll += 2;
                if (software_scroll > maximum_scroll) {
                    software_scroll = maximum_scroll;
                }
                return;
            }
        }
    } else if (active_app == 12) {
        install_handle_click(px, py, wx, wy, ww, wh);
    }
}

static void clear_login_password(void) {
    volatile char* password = session_request.password;
    for (uint64_t i = 0; i < sizeof(session_request.password); i++) {
        password[i] = 0;
    }
}

static int session_refresh_status(void) {
    return syscall3(AOS_SYS_SESSION, AOS_SESSION_QUERY, 0,
                    (long)&session_status) < 0 ? -1 : 0;
}

static void initialize_user_workspace(void) {
    ensure_mui_user_layout();
    settings_load_config();
    if (syscall1(SYS_CHDIR, (long)AOS_MAIN_HOME) < 0) {
        syscall1(SYS_CHDIR, (long)"/");
    }
    terminal_update_cwd();
    files_set_path(AOS_MAIN_HOME);
    software_refresh_packages();
    if (editor_load_file(AOS_MAIN_HOME "/welcome.txt", "welcome.txt") != 0) {
        editor_saved = 0;
        editor_save_failed = 0;
    }
    fetch_system();
    install_refresh_status();
    install_refresh_disks();
}

static int attempt_session_login(void) {
    long rc;
    session_request.version = AOS_SESSION_ABI_VERSION;
    copy_cstr(session_request.username, sizeof(session_request.username),
              session_status.username);
    rc = syscall3(AOS_SYS_SESSION, AOS_SESSION_LOGIN,
                  (long)&session_request, (long)&session_status);
    clear_login_password();
    if (rc < 0 || !session_status.authenticated) {
        copy_cstr(login_notice, sizeof(login_notice), session_status.message);
        return -1;
    }
    login_active = 0;
    login_password_focused = 0;
    login_notice[0] = 0;
    window_open = 0;
    overview_open = 0;
    active_app = 0;
    initialize_user_workspace();
    return 0;
}

static int logout_user_session(void) {
    long rc;
    if (terminal_child_pid > 0) terminal_stop_child();
    if (software_child_pid > 0) software_stop_child();
    if (settings_config_dirty) settings_save_config();
    rc = syscall3(AOS_SYS_SESSION, AOS_SESSION_LOGOUT, 0,
                  (long)&session_status);
    if (rc < 0) return -1;
    clear_login_password();
    login_active = 1;
    login_password_focused = 1;
    login_button_hovered = 0;
    copy_cstr(login_notice, sizeof(login_notice), "Signed out");
    window_open = 0;
    overview_open = 0;
    context_menu_open = 0;
    power_menu_open = 0;
    terminal_history_count = 0;
    terminal_history_position = 0;
    terminal_last_count = 0;
    terminal_input_len = 0;
    terminal_input[0] = 0;
    files_selected = -1;
    files_status[0] = 0;
    fetch_system();
    return 0;
}

static void handle_click(int x, int y) {
    int app;
    int all_apps;
    int target;

    if (context_menu_open) {
        int action = context_action_at(x, y);
        if (action != CONTEXT_ACTION_NONE) {
            execute_context_action(action);
        } else {
            context_menu_open = 0;
            context_menu_kind = CONTEXT_KIND_NONE;
        }
        return;
    }

    if (power_menu_open) {
        int action = power_action_at(x, y);
        if (action == POWER_ACTION_RESTART) {
            power_action_failed = 0;
            syscall1(AOS_SYS_RESTART, 0);
            power_action_failed = 1;
        } else if (action == POWER_ACTION_SHUTDOWN) {
            power_action_failed = 0;
            syscall1(AOS_SYS_SHUTDOWN, 0);
            power_action_failed = 1;
        } else if (action == POWER_ACTION_LOGOUT) {
            power_action_failed = logout_user_session() != 0;
        } else {
            power_menu_open = 0;
            power_action_failed = 0;
        }
        return;
    }

    target = topbar_target_at(x, y);
    if (target == TOPBAR_TARGET_ACTIVITIES) {
        overview_open = !overview_open;
        return;
    }
    if (target == TOPBAR_TARGET_CLOCK) {
        open_app(6);
        return;
    }
    if (target == TOPBAR_TARGET_SEARCH) {
        overview_open = 1;
        return;
    }
    if (target == TOPBAR_TARGET_WIFI) {
        settings_section = 0;
        open_app(1);
        return;
    }
    if (target == TOPBAR_TARGET_VOLUME) {
        settings_section = 4;
        open_app(1);
        return;
    }
    if (target == TOPBAR_TARGET_BATTERY) {
        settings_section = 6;
        open_app(1);
        return;
    }
    if (target == TOPBAR_TARGET_POWER) {
        power_menu_open = 1;
        power_action_failed = 0;
        window_dragging = 0;
        return;
    }

    app = dock_app_at(x, y, &all_apps);
    if (all_apps) {
        overview_open = !overview_open;
        return;
    }
    if (app >= 0) {
        open_app(app);
        return;
    }

    if (overview_open) {
        app = overview_app_at(x, y);
        if (app >= 0) {
            open_app(app);
        }
        return;
    }

    if (window_open) {
        int wx;
        int wy;
        int ww;
        int wh;
        active_window_bounds(&wx, &wy, &ww, &wh);
        if (point_in(x, y, wx + ww - 32, wy + 4, 28, 30)) {
            window_open = 0;
            window_dragging = 0;
            settings_slider_drag = 0;
            return;
        }
        if (point_in(x, y, wx + ww - 62, wy + 4, 28, 30)) {
            if (!window_maximized) remember_window_position();
            window_maximized = !window_maximized;
            window_dragging = 0;
            settings_slider_drag = 0;
            return;
        }
        if (point_in(x, y, wx + ww - 92, wy + 4, 28, 30)) {
            window_open = 0;
            window_dragging = 0;
            settings_slider_drag = 0;
            return;
        }
        if (point_in(x, y, wx, wy, ww, 36)) {
            if (!window_maximized) {
                remember_window_position();
                window_drag_offset_x = x - window_x;
                window_drag_offset_y = y - window_y;
                window_dragging = 1;
                settings_slider_drag = 0;
            }
            return;
        }
        if (point_in(x, y, wx, wy, ww, wh)) {
            handle_app_click(x, y, wx, wy, ww, wh);
            return;
        }
    }

    app = desktop_app_at(x, y);
    if (app >= 0) {
        open_app(app);
    }
}

static void login_panel_bounds(int* x, int* y, int* w, int* h) {
    *w = (int)gfx.width < 460 ? (int)gfx.width - 28 : 420;
    *h = (int)gfx.height < 560 ? (int)gfx.height - TOPBAR_HEIGHT - 28 : 390;
    if (*w < 300) *w = 300;
    if (*h < 330) *h = 330;
    *x = ((int)gfx.width - *w) / 2;
    *y = TOPBAR_HEIGHT + ((int)gfx.height - TOPBAR_HEIGHT - *h) / 2;
    if (*y < TOPBAR_HEIGHT + 10) *y = TOPBAR_HEIGHT + 10;
}

static void draw_login_topbar(void) {
    int w = (int)gfx.width;
    char hh[3];
    char mm[3];
    char date[40];
    uint8_t display_hour = (uint8_t)(time_info.hour % 12);
    if (display_hour == 0) display_hour = 12;
    two_digit(hh, display_hour);
    two_digit(mm, time_info.minute);
    copy_cstr(date, sizeof(date), weekday_name(time_info.weekday));
    append_cstr(date, sizeof(date), ", ");
    append_cstr(date, sizeof(date), month_name(time_info.month));
    append_cstr(date, sizeof(date), " ");
    append_cstr(date, sizeof(date), u64_to_dec(time_info.day ? time_info.day : 1));
    append_cstr(date, sizeof(date), "  ");
    append_cstr(date, sizeof(date), hh);
    append_cstr(date, sizeof(date), ":");
    append_cstr(date, sizeof(date), mm);
    append_cstr(date, sizeof(date), time_info.hour >= 12 ? " PM" : " AM");

    alpha_rect(0, 0, w, TOPBAR_HEIGHT, 0x172019, 190, 1);
    alpha_rect(0, TOPBAR_HEIGHT - 1, w, 1, 0xffffff, 18, 1);
    round_rect(22, 10, 16, 16, 3, UI_ACCENT);
    draw_text(27, 11, "A", UI_PANEL_TEXT, 1);
    draw_text(46, 11, "AOS", UI_PANEL_TEXT, 1);
    draw_text_center(w / 2, 11, date, UI_PANEL_TEXT, 1);
    if (power_menu_open || point_in(pointer_x, pointer_y, w - 40, 4, 36, 28)) {
        alpha_round_rect(w - 40, 4, 36, 28, 8, 0xffffff,
                         power_menu_open ? 42 : 28, 1);
    }
    draw_power_icon(w - 22, 18, UI_PANEL_TEXT);
}

static void draw_login_screen(void) {
    int x, y, w, h;
    int field_x;
    int field_y;
    int field_w;
    const char* hidden = install_secret(session_request.password);

    draw_wallpaper();
    alpha_rect(0, TOPBAR_HEIGHT, (int)gfx.width,
               (int)gfx.height - TOPBAR_HEIGHT, 0x0b1710, 100, 1);
    draw_login_topbar();
    login_panel_bounds(&x, &y, &w, &h);
    glass_panel(x, y, w, h, 18, UI_GLASS_LIGHT,
                settings_transparency ? 242 : 255, UI_BORDER);

    disc(x + w / 2, y + 64, 35, UI_ACCENT);
    draw_text_center(x + w / 2, y + 42, "A", 0xffffff, 4);
    draw_text_center(x + w / 2, y + 112,
                     session_status.username[0]
                         ? session_status.username : "AOS User",
                     UI_TEXT, 2);
    draw_text_center(x + w / 2, y + 139,
                     session_status.administrator
                         ? "Administrator account" : "Standard account",
                     UI_MUTED, 1);

    field_x = x + 34;
    field_y = y + 176;
    field_w = w - 68;
    draw_text(field_x, field_y, "Password", UI_MUTED, 1);
    alpha_round_rect(field_x, field_y + 24, field_w, 48, 10,
                     login_password_focused ? UI_ACCENT : UI_BORDER, 255, 1);
    alpha_round_rect(field_x + 1, field_y + 25, field_w - 2, 46, 9,
                     UI_CARD, 255, 1);
    draw_text(field_x + 14, field_y + 40,
              hidden[0] ? hidden : "Enter your password",
              hidden[0] ? UI_TEXT : UI_MUTED, 1);
    if (login_password_focused) {
        int cursor = field_x + 15 + text_width(hidden, 1);
        rect(cursor, field_y + 37, 1, 20, UI_ACCENT);
    }

    round_rect(field_x, field_y + 88, field_w, 42, 10,
               login_button_hovered ? rgb_lerp(UI_ACCENT, 0xffffff, 1, 8)
                                    : UI_ACCENT);
    draw_text_center(x + w / 2, field_y + 101, "Sign In", 0xffffff, 1);

    if (session_status.retry_after_seconds) {
        char wait[64];
        copy_cstr(wait, sizeof(wait), "Try again in ");
        append_cstr(wait, sizeof(wait),
                    u64_to_dec(session_status.retry_after_seconds));
        append_cstr(wait, sizeof(wait), " seconds");
        draw_text_center(x + w / 2, field_y + 147, wait, UI_WARNING, 1);
    } else if (login_notice[0]) {
        draw_text_center_ellipsized(x + w / 2, field_y + 147,
                                    login_notice, w - 48, UI_WARNING, 1);
    } else {
        draw_text_center(x + w / 2, field_y + 147,
                         "Your files stay local on this computer",
                         UI_MUTED, 1);
    }
    draw_text_center(x + w / 2, y + h - 30,
                     "AOS 1.0 Forest", UI_MUTED, 1);
}

static void render(void) {
    if (login_active) {
        draw_login_screen();
        draw_power_menu();
        apply_display_settings();
        sync_cursor();
        syscall1(AOS_SYS_GFX_PRESENT, 0);
        return;
    }
    draw_wallpaper();
    if (settings_theme_dark) {
        alpha_rect(0, 0, (int)gfx.width, (int)gfx.height,
                   0x07100b, 46, 1);
    }
    draw_topbar();
    if (overview_open) {
        draw_overview();
    } else {
        draw_desktop_icons();
        if (window_open) draw_active_window();
        draw_dock();
    }
    draw_context_menu();
    draw_settings_notice();
    draw_power_menu();
    apply_display_settings();
    sync_cursor();
    syscall1(AOS_SYS_GFX_PRESENT, 0);
}

static void select_next(int dir) {
    for (int attempts = 0; attempts < APP_COUNT; attempts++) {
        active_app += dir;
        if (active_app < 0) active_app = APP_COUNT - 1;
        if (active_app >= APP_COUNT) active_app = 0;
        if (app_is_available(active_app)) return;
    }
    active_app = 0;
}

static int input_event_is_pointer(const struct aos_input_event* event) {
    if (!event) return 0;
    return event->source == AOS_INPUT_SOURCE_MOUSE ||
           event->key == KEY_POINTER;
}

static char* install_focused_value(uint64_t* capacity) {
    if (!capacity) return 0;
    switch (install_focus) {
        case 1: *capacity = sizeof(install_config.username); return install_config.username;
        case 2: *capacity = sizeof(install_config.password); return install_config.password;
        case 3: *capacity = sizeof(install_config.password_confirm); return install_config.password_confirm;
        case 4: *capacity = sizeof(install_config.wifi_ssid); return install_config.wifi_ssid;
        case 5: *capacity = sizeof(install_config.wifi_password); return install_config.wifi_password;
        case 6: *capacity = sizeof(install_config.wifi_ip); return install_config.wifi_ip;
        case 7: *capacity = sizeof(install_config.wifi_gateway); return install_config.wifi_gateway;
        case 8: *capacity = sizeof(install_config.wifi_dns); return install_config.wifi_dns;
        case 9: *capacity = sizeof(install_keyboard_test); return install_keyboard_test;
        case 10: *capacity = sizeof(install_config.hostname); return install_config.hostname;
        case 11: *capacity = sizeof(install_config.confirmation); return install_config.confirmation;
        default: *capacity = 0; return 0;
    }
}

static int install_handle_key(uint32_t key) {
    uint64_t capacity = 0;
    char* value = install_focused_value(&capacity);
    uint64_t len;
    char c;
    if ((!value || capacity == 0) && install_page == 11 &&
        (key == '\n' || key == '\r')) {
        install_try_begin();
        return 1;
    }
    if (!value || capacity == 0) return 0;
    len = cstrlen(value);
    if (key == '\b' || key == 127) {
        if (len) value[len - 1] = 0;
        install_notice[0] = 0;
        return 1;
    }
    if ((key == '\n' || key == '\r') && install_focus == 11) {
        install_focus = 0;
        install_try_begin();
        return 1;
    }
    if (key == '\n' || key == '\r' || key == 27) {
        install_focus = 0;
        return 1;
    }
    if (key < 32 || key > 126 || len + 1 >= capacity) return 1;
    c = (char)key;
    if (install_focus == 1 &&
        !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) return 1;
    if (install_focus == 10) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return 1;
    }
    if (install_focus == 11 &&
        !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return 1;
    append_char(value, capacity, c);
    install_notice[0] = 0;
    return 1;
}

static int handle_login_input(void) {
    struct aos_input_event ev;
    int result = 0;
    while (syscall3(AOS_SYS_INPUT_POLL, (long)&ev, 0, 0) > 0) {
        int panel_x, panel_y, panel_w, panel_h;
        login_panel_bounds(&panel_x, &panel_y, &panel_w, &panel_h);
        (void)panel_h;
        if (input_event_is_pointer(&ev)) {
            uint32_t previous = pointer_buttons;
            int old_x = pointer_x;
            int old_y = pointer_y;
            int old_button = login_button_hovered;
            int old_power = hover_power_action;
            pointer_x = ev.pointer_x;
            pointer_y = ev.pointer_y;
            pointer_buttons = ev.pointer_buttons;
            login_button_hovered = point_in(pointer_x, pointer_y,
                                            panel_x + 34, panel_y + 264,
                                            panel_w - 68, 42);
            hover_power_action = power_menu_open
                                     ? power_action_at(pointer_x, pointer_y)
                                     : POWER_ACTION_NONE;
            if (pointer_x != old_x || pointer_y != old_y ||
                pointer_buttons != previous) result |= INPUT_RESULT_POINTER;
            if (old_button != login_button_hovered ||
                old_power != hover_power_action) result |= INPUT_RESULT_REDRAW;
            if ((pointer_buttons & AOS_POINTER_LEFT) &&
                !(previous & AOS_POINTER_LEFT)) {
                if (power_menu_open) {
                    int action = power_action_at(pointer_x, pointer_y);
                    if (action == POWER_ACTION_RESTART) {
                        syscall1(AOS_SYS_RESTART, 0);
                    } else if (action == POWER_ACTION_SHUTDOWN) {
                        syscall1(AOS_SYS_SHUTDOWN, 0);
                    } else {
                        power_menu_open = 0;
                    }
                } else if (point_in(pointer_x, pointer_y,
                                    (int)gfx.width - 40, 4, 36, 28)) {
                    power_menu_open = 1;
                    power_action_failed = 0;
                } else if (point_in(pointer_x, pointer_y,
                                    panel_x + 34, panel_y + 200,
                                    panel_w - 68, 48)) {
                    login_password_focused = 1;
                } else if (login_button_hovered) {
                    attempt_session_login();
                }
                result |= INPUT_RESULT_REDRAW;
            }
            continue;
        }

        uint32_t key = ev.ascii ? ev.ascii : ev.key;
        uint64_t length = cstrlen(session_request.password);
        if (key == 27 && power_menu_open) {
            power_menu_open = 0;
            result |= INPUT_RESULT_REDRAW;
        } else if (key == '\n' || key == '\r') {
            attempt_session_login();
            result |= INPUT_RESULT_REDRAW;
        } else if ((key == '\b' || key == 127) && length > 0) {
            session_request.password[length - 1U] = 0;
            login_notice[0] = 0;
            result |= INPUT_RESULT_REDRAW;
        } else if (key >= 32 && key <= 126 &&
                   length + 1U < sizeof(session_request.password)) {
            session_request.password[length] = (char)key;
            session_request.password[length + 1U] = 0;
            login_notice[0] = 0;
            login_password_focused = 1;
            result |= INPUT_RESULT_REDRAW;
        }
    }
    return result;
}

static int handle_input(void) {
    struct aos_input_event ev;
    long rc;
    int result = 0;
    if (login_active) return handle_login_input();
    while ((rc = syscall3(AOS_SYS_INPUT_POLL, (long)&ev, 0, 0)) > 0) {
        if (input_event_is_pointer(&ev)) {
            uint32_t previous = pointer_buttons;
            int old_x = pointer_x;
            int old_y = pointer_y;
            int old_hover_desktop = hover_desktop_app;
            int old_hover_dock = hover_dock_app;
            int old_hover_overview = hover_overview_app;
            int old_hover_all = hover_all_apps;
            int old_hover_activities = hover_activities;
            int old_hover_topbar = hover_topbar_target;
            int old_hover_power_action = hover_power_action;
            int old_hover_context_action = hover_context_action;
            int old_hover_close = hover_close;
            int old_dragging = window_dragging;
            pointer_x = ev.pointer_x;
            pointer_y = ev.pointer_y;
            pointer_buttons = ev.pointer_buttons;
            if (pointer_x != old_x || pointer_y != old_y || pointer_buttons != previous) {
                result |= INPUT_RESULT_POINTER;
            }
            if ((pointer_buttons & AOS_POINTER_RIGHT) &&
                !(previous & AOS_POINTER_RIGHT)) {
                handle_right_click(pointer_x, pointer_y);
                result |= INPUT_RESULT_REDRAW;
            } else if ((pointer_buttons & AOS_POINTER_LEFT) &&
                       !(previous & AOS_POINTER_LEFT)) {
                handle_click(pointer_x, pointer_y);
                result |= INPUT_RESULT_REDRAW;
            } else if ((pointer_buttons & AOS_POINTER_LEFT) && window_dragging) {
                move_window_to_pointer(pointer_x, pointer_y);
                result |= INPUT_RESULT_REDRAW;
            } else if ((pointer_buttons & AOS_POINTER_LEFT) &&
                       settings_slider_drag && window_open &&
                       active_app == 1) {
                int wx;
                int wy;
                int ww;
                int wh;
                active_window_bounds(&wx, &wy, &ww, &wh);
                (void)wy;
                (void)wh;
                settings_update_slider(pointer_x, wx, ww);
                result |= INPUT_RESULT_REDRAW;
            } else if (!(pointer_buttons & AOS_POINTER_LEFT) && (previous & AOS_POINTER_LEFT)) {
                int completed_slider = settings_slider_drag;
                window_dragging = 0;
                settings_slider_drag = 0;
                if (completed_slider == 1) {
                    settings_commit("Output volume saved");
                    result |= INPUT_RESULT_REDRAW;
                } else if (completed_slider == 2) {
                    settings_commit("Input volume saved");
                    result |= INPUT_RESULT_REDRAW;
                } else if (completed_slider == 3) {
                    settings_commit("Display brightness saved");
                    result |= INPUT_RESULT_REDRAW;
                }
            }
            update_hover(pointer_x, pointer_y);
            if (old_hover_desktop != hover_desktop_app ||
                old_hover_dock != hover_dock_app ||
                old_hover_overview != hover_overview_app ||
                old_hover_all != hover_all_apps ||
                old_hover_activities != hover_activities ||
                old_hover_topbar != hover_topbar_target ||
                old_hover_power_action != hover_power_action ||
                old_hover_context_action != hover_context_action ||
                old_hover_close != hover_close ||
                old_dragging != window_dragging) {
                result |= INPUT_RESULT_REDRAW;
            }
            continue;
        }
        uint32_t key = ev.ascii ? ev.ascii : ev.key;
        if ((ev.flags & AOS_INPUT_FLAG_CTRL) &&
            (ev.flags & AOS_INPUT_FLAG_ALT) &&
            (key == 'l' || key == 'L' || key == 12)) {
            logout_user_session();
            result |= INPUT_RESULT_REDRAW;
            continue;
        }
        if (key == 27 && context_menu_open) {
            context_menu_open = 0;
            context_menu_kind = CONTEXT_KIND_NONE;
            result |= INPUT_RESULT_REDRAW;
            continue;
        }
        if (key == 27 && power_menu_open) {
            power_menu_open = 0;
            power_action_failed = 0;
            result |= INPUT_RESULT_REDRAW;
            continue;
        }
        if (window_open && active_app == 12 && install_handle_key(key)) {
            result |= INPUT_RESULT_REDRAW;
            continue;
        }
        if (window_open && active_app == 0) {
            int wx;
            int wy;
            int ww;
            int wh;
            int columns;

            active_window_bounds(&wx, &wy, &ww, &wh);
            (void)wx;
            (void)wy;
            columns = ww - 208 >= 560 ? 5 : 4;
            if (key == '\n' || key == '\r') {
                files_open_selected();
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == '\b' || key == 127) {
                files_go_parent();
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == KEY_LEFT) {
                files_move_selection(-1, ww, wh);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == KEY_RIGHT) {
                files_move_selection(1, ww, wh);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == KEY_UP) {
                files_move_selection(files_view == 0 ? -columns : -1,
                                     ww, wh);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == KEY_DOWN) {
                files_move_selection(files_view == 0 ? columns : 1,
                                     ww, wh);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == 'r' || key == 'R') {
                files_refresh();
                copy_cstr(files_status, sizeof(files_status),
                          "Folder refreshed");
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
        }
        if (window_open && active_app == 2) {
            if (terminal_child_pid > 0) {
                if ((ev.flags & AOS_INPUT_FLAG_CTRL) &&
                    (key == 'c' || key == 'C' || key == 3)) {
                    terminal_stop_child();
                } else if ((ev.flags & AOS_INPUT_FLAG_CTRL) &&
                           (key == 'd' || key == 'D' || key == 4)) {
                    terminal_close_stdin();
                } else if (key == '\n' || key == '\r') {
                    terminal_send_char('\n');
                } else if (key == '\b' || key == 127) {
                    terminal_send_char('\b');
                } else if (key == KEY_UP || key == KEY_DOWN ||
                           key == KEY_LEFT || key == KEY_RIGHT) {
                    terminal_send_char((char)key);
                } else if (key >= 32 && key <= 126) {
                    terminal_send_char((char)key);
                }
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if ((ev.flags & AOS_INPUT_FLAG_CTRL) &&
                (key == 'l' || key == 'L' || key == 12)) {
                terminal_last_count = 0;
                terminal_reset_stream();
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if ((ev.flags & AOS_INPUT_FLAG_CTRL) &&
                (key == 'c' || key == 'C' || key == 3)) {
                terminal_input[0] = 0;
                terminal_input_len = 0;
                terminal_history_position = terminal_history_count;
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == KEY_UP) {
                terminal_recall_history(-1);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == KEY_DOWN) {
                terminal_recall_history(1);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == '\t') {
                terminal_complete_command();
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == '\n' || key == '\r') {
                terminal_run();
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if ((key == '\b' || key == 127) && terminal_input_len > 0) {
                terminal_input[--terminal_input_len] = 0;
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key >= 32 && key <= 126 && terminal_input_len + 1 < (int)sizeof(terminal_input)) {
                terminal_input[terminal_input_len++] = (char)key;
                terminal_input[terminal_input_len] = 0;
                terminal_history_position = terminal_history_count;
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
        }
        if (window_open && active_app == 3) {
            if ((ev.flags & AOS_INPUT_FLAG_PRESSED) &&
                (ev.flags & AOS_INPUT_FLAG_CTRL) &&
                (key == 's' || key == 'S' || key == 0x13)) {
                editor_save_file();
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == '\n' || key == '\r') {
                if (editor_lines < (int)(sizeof(editor_text) / sizeof(editor_text[0]))) {
                    editor_text[editor_lines][0] = 0;
                    editor_lines++;
                    editor_saved = 0;
                    editor_save_failed = 0;
                }
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if ((key == '\b' || key == 127) && editor_lines > 0) {
                uint64_t n = cstrlen(editor_text[editor_lines - 1]);
                if (n > 0) {
                    editor_text[editor_lines - 1][n - 1] = 0;
                } else if (editor_lines > 1) {
                    editor_lines--;
                }
                editor_saved = 0;
                editor_save_failed = 0;
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key >= 32 && key <= 126 && editor_lines > 0) {
                append_char(editor_text[editor_lines - 1], sizeof(editor_text[0]), (char)key);
                editor_saved = 0;
                editor_save_failed = 0;
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
        }
        if (window_open && active_app == 5) {
            char k[2];
            if ((key >= '0' && key <= '9') || key == '+' || key == '-' ||
                key == '*' || key == '/' || key == '=' || key == '.' ||
                key == '%') {
                k[0] = (char)key;
                k[1] = 0;
                calc_press(k);
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == '\n' || key == '\r') {
                calc_press("=");
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
            if (key == 'c' || key == 'C') {
                calc_press("C");
                result |= INPUT_RESULT_REDRAW;
                continue;
            }
        }
        if (ev.flags & AOS_INPUT_FLAG_CTRL) {
            result |= INPUT_RESULT_REDRAW;
            continue;
        }
        if ((key == 'q' || key == 'Q') && !install_status.running)
            return result | INPUT_RESULT_QUIT;
        result |= INPUT_RESULT_REDRAW;
        if (key == '\t' || key == KEY_RIGHT || key == 'd' || key == 'D') select_next(1);
        else if ((key == KEY_LEFT &&
                  !(ev.flags & AOS_INPUT_FLAG_CTRL)) ||
                 key == 'a' || key == 'A') {
            select_next(-1);
        }
        else if (key == ' ') overview_open = !overview_open;
        else if (key == '\n' || key == '\r') {
            open_app(active_app);
        } else if (key == 27) {
            overview_open = 0;
        } else if (key >= '1' && key <= '9') {
            int app = (int)(key - '1');
            if (app >= APP_COUNT) app = APP_COUNT - 1;
            open_app(app);
        } else if (key == '0') {
            open_app(11);
        } else if (key == 'w' || key == 'W' || key == KEY_UP) {
            overview_open = 1;
        } else if (key == 'x' || key == 'X') {
            window_open = 0;
        }
    }
    return result;
}

static void wait_once_key(void) {
    struct aos_input_event ev;

    for (;;) {
        while (syscall3(AOS_SYS_INPUT_POLL, (long)&ev, 0, 0) > 0) {
            if (input_event_is_pointer(&ev)) {
                pointer_x = ev.pointer_x;
                pointer_y = ev.pointer_y;
                pointer_buttons = ev.pointer_buttons;
                if (pointer_buttons & AOS_POINTER_LEFT) return;
                present_cursor();
                continue;
            }
            if (ev.ascii || ev.key) return;
        }
        if (window_dragging || settings_slider_drag) {
            sleep_interactive_frame();
        } else {
            sleep_frame();
        }
    }
}

static void usage(void) {
    write_cstr("usage: mui [--once|--help]\n");
    write_cstr("input: click icons/dock/windows, drag title bars, or use Space Activities, Tab/Arrows select, Enter open, X close, Q quit\n");
    write_cstr("mui --once renders one desktop frame and waits for any key before returning\n");
}

void aos_main(uint64_t argc, char** argv) {
    int once = 0;
    for (uint64_t i = 1; i < argc; i++) {
        if (streq(argv[i], "--help")) {
            usage();
            exit_code(0);
        } else if (streq(argv[i], "--once")) {
            once = 1;
        } else {
            usage();
            exit_code(1);
        }
    }

    write_cstr("MUI: AOS nature desktop starting\n");
    write_cstr("MUI input: click icons and dock, drag title bars, or use Space Activities, Tab select, Enter open, X close, Q quit\n");

    if (syscall3(AOS_SYS_GFX_INFO, (long)&gfx, 0, 0) < 0 || !gfx.ready) {
        write_cstr("mui: graphics framebuffer unavailable\n");
        exit_code(1);
    }

    active_app = 0;
    window_open = 0;
    overview_open = 0;
    once_mode = once;
    pointer_x = (int)gfx.width / 2;
    pointer_y = (int)gfx.height / 2;
    pointer_buttons = 0;
    hover_desktop_app = -1;
    hover_dock_app = -1;
    hover_overview_app = -1;
    hover_all_apps = 0;
    hover_activities = 0;
    hover_topbar_target = TOPBAR_TARGET_NONE;
    hover_power_action = POWER_ACTION_NONE;
    hover_context_action = CONTEXT_ACTION_NONE;
    hover_close = 0;
    power_menu_open = 0;
    power_action_failed = 0;
    context_menu_open = 0;
    context_menu_kind = CONTEXT_KIND_NONE;
    context_menu_x = 0;
    context_menu_y = 0;
    window_dragging = 0;
    window_drag_offset_x = 0;
    window_drag_offset_y = 0;
    window_has_position = 0;
    window_maximized = 0;
    window_x = 0;
    window_y = 0;
    files_place = 0;
    files_view = 0;
    files_scroll = 0;
    files_last_clicked = -1;
    files_last_click_ticks = 0;
    copy_cstr(files_current_path, sizeof(files_current_path),
              AOS_MAIN_HOME);
    file_entry_count = 0;
    files_selected = -1;
    files_status[0] = 0;
    settings_section = 3;
    settings_theme_dark = 0;
    settings_accent = 0;
    settings_animations = 1;
    settings_transparency = 1;
    settings_wifi = 1;
    settings_volume = 65;
    settings_input_volume = 40;
    settings_system_sounds = 1;
    settings_brightness = 80;
    settings_night_light = 1;
    settings_notifications = 1;
    settings_do_not_disturb = 0;
    settings_notification_banners = 1;
    settings_power_mode = 1;
    settings_reduce_motion_saver = 1;
    settings_remember_history = 1;
    settings_slider_drag = 0;
    settings_config_dirty = 0;
    settings_save_failed = 0;
    settings_notice_frames = 0;
    settings_notice_until_second = 0;
    settings_notice[0] = 0;
    for (int i = 0; i < 10; i++) {
        settings_feature_enabled[i] = 1;
        settings_auto_updates[i] = 1;
    }
    calendar_selected = 0;
    calendar_month = 0;
    calendar_year = 0;
    photo_index = 0;
    software_category = 0;
    software_scroll = 0;
    software_package_count = 0;
    software_child_pid = 0;
    software_child_stdout = -1;
    software_action = SOFTWARE_ACTION_NONE;
    software_action_target[0] = 0;
    software_status[0] = 0;
    software_output_line[0] = 0;
    software_output_length = 0;
    install_set_defaults();
    for (uint64_t i = 0; i < sizeof(session_status); i++)
        ((uint8_t*)&session_status)[i] = 0;
    for (uint64_t i = 0; i < sizeof(session_request); i++)
        ((uint8_t*)&session_request)[i] = 0;
    session_request.version = AOS_SESSION_ABI_VERSION;
    login_active = 0;
    login_password_focused = 1;
    login_button_hovered = 0;
    login_notice[0] = 0;
    copy_cstr(calc_display, sizeof(calc_display), "0");
    calc_value = 0;
    calc_operand = 0;
    calc_op = 0;
    calc_fresh = 1;
    copy_cstr(editor_text[0], sizeof(editor_text[0]), "Welcome to AOS Text Editor");
    copy_cstr(editor_text[1], sizeof(editor_text[1]), "");
    copy_cstr(editor_text[2], sizeof(editor_text[2]), "A clean place to write, jot notes, and draft ideas.");
    copy_cstr(editor_text[3], sizeof(editor_text[3]), "");
    copy_cstr(editor_text[4], sizeof(editor_text[4]), "- Everything here runs natively in AOS");
    copy_cstr(editor_text[5], sizeof(editor_text[5]), "- This is the MUI desktop environment");
    copy_cstr(editor_text[6], sizeof(editor_text[6]), "- Try Files, Terminal and Settings apps too");
    copy_cstr(editor_text[7], sizeof(editor_text[7]), "");
    copy_cstr(editor_text[8], sizeof(editor_text[8]), "Happy exploring");
    editor_lines = 9;
    editor_saved = 0;
    editor_save_failed = 0;
    copy_cstr(editor_filename, sizeof(editor_filename), "welcome.txt");
    copy_cstr(editor_path, sizeof(editor_path),
              AOS_MAIN_HOME "/welcome.txt");
    terminal_input[0] = 0;
    terminal_input_len = 0;
    terminal_stream_line[0] = 0;
    terminal_stream_len = 0;
    terminal_stream_cursor = 0;
    terminal_escape_state = 0;
    terminal_escape_value = 0;
    terminal_child_pid = 0;
    terminal_child_stdin = -1;
    terminal_child_stdout = -1;
    terminal_active_command[0] = 0;
    terminal_cwd[0] = 0;
    terminal_history_count = 0;
    terminal_history_position = 0;
    terminal_last_count = 1;
    copy_cstr(terminal_last[0], sizeof(terminal_last[0]),
              "AOS Terminal - all bundled commands are available");

    fetch_system();
    if (session_refresh_status() != 0) {
        write_cstr("mui: kernel session service unavailable\n");
        exit_code(1);
    }
    if (session_status.installed && !session_status.authenticated &&
        session_status.autologin) {
        syscall3(AOS_SYS_SESSION, AOS_SESSION_AUTOLOGIN, 0,
                 (long)&session_status);
    }
    login_active = session_status.login_required &&
                   !session_status.authenticated;
    if (!login_active) {
        initialize_user_workspace();
        if (install_status.live_mode && install_status.payload_ready) {
            open_app(12);
            write_cstr("MUI: opening Install AOS for live mode\n");
        }
    } else {
        install_refresh_status();
        write_cstr("MUI: waiting for local account authentication\n");
    }
    write_cstr("MUI: RAM used=");
    write_cstr(u64_to_dec((mem_info.used + 1023ULL) / 1024ULL));
    write_cstr(" KiB free=");
    write_cstr(u64_to_dec(mem_info.free / 1024ULL));
    write_cstr(" KiB total=");
    write_cstr(u64_to_dec(mem_info.total / (1024ULL * 1024ULL)));
    write_cstr(" MiB\n");

    if (once) {
        render();
        wait_once_key();
        syscall5(AOS_SYS_GFX_CURSOR, pointer_x, pointer_y, pointer_buttons, 0, 0);
        syscall1(AOS_SYS_GFX_PRESENT, 0);
        exit_code(0);
    }

    render();
    uint32_t refresh_counter = 0;
    for (;;) {
        int input_result = handle_input();
        uint32_t refresh_interval;

        if (window_open && active_app == 4) {
            refresh_interval = settings_power_mode == 0 ? 24U
                               : settings_power_mode == 2 ? 32U : 25U;
        } else {
            refresh_interval = settings_power_mode == 0 ? 64U
                               : settings_power_mode == 2 ? 160U : 100U;
        }

        if (input_result & INPUT_RESULT_QUIT) break;
        if (terminal_pump()) input_result |= INPUT_RESULT_REDRAW;
        if (software_pump()) input_result |= INPUT_RESULT_REDRAW;
        if (install_pump()) input_result |= INPUT_RESULT_REDRAW;
        if (terminal_child_pid > 0 || software_child_pid > 0) {
            syscall1(AOS_SYS_PROCESS_YIELD, 0);
        }
        if (settings_notice_until_second > 0) {
            settings_notice_frames++;
            if ((settings_notice_frames & 7) == 0) {
                input_result |= INPUT_RESULT_REDRAW;
            }
        }
        refresh_counter++;
        if (refresh_counter >= refresh_interval) {
            fetch_system();
            if (login_active) session_refresh_status();
            refresh_counter = 0;
            input_result |= INPUT_RESULT_REDRAW;
        }
        if (input_result & INPUT_RESULT_REDRAW) {
            render();
        } else if (input_result & INPUT_RESULT_POINTER) {
            present_cursor();
        }
        if (window_dragging || settings_slider_drag) {
            sleep_interactive_frame();
        } else {
            sleep_frame();
        }
    }

    syscall5(AOS_SYS_GFX_CURSOR, pointer_x, pointer_y, pointer_buttons, 0, 0);
    if (terminal_child_pid > 0) terminal_stop_child();
    if (software_child_pid > 0) software_stop_child();
    if (settings_config_dirty) settings_save_config();
    syscall1(AOS_SYS_GFX_CLEAR, 0x05070a);
    syscall1(AOS_SYS_GFX_PRESENT, 0);
    write_cstr("MUI: closed\n");
    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
