/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>
#include <stddef.h>
#include <aos_install.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_BLKDEV_INFO 504
#define AOS_SYS_PCI_INFO 518
#define AOS_SYS_DRIVER_INFO 519
#define AOS_SYS_INSTALLER 562
#define AOS_SYS_RESTART 514

struct aos_blkdev_user {
    uint32_t id;
    uint32_t block_size;
    uint64_t size;
    uint8_t read_only;
    uint8_t has_ops;
    uint8_t reserved[6];
    char name[16];
};

static struct aos_install_config config;
static struct aos_install_status status;
static char line[128];
static char num_buf[32];

static long syscall1(long n, long a) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a) : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long n, long a, long b) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b) : "rcx", "r11", "memory");
    return ret;
}

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "rcx", "r11", "memory");
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
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

static void clear_bytes(void* ptr, uint64_t size) {
    uint8_t* out = (uint8_t*)ptr;
    while (size--) *out++ = 0;
}

static void copy_cstr(char* dst, uint64_t cap, const char* src) {
    uint64_t i = 0;
    if (!dst || cap == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void write_cstr(const char* s) {
    syscall3(SYS_WRITE, 1, (long)s, (long)cstrlen(s));
}

static const char* u64_text(uint64_t value) {
    uint64_t at = sizeof(num_buf) - 1;
    num_buf[at] = 0;
    if (value == 0) num_buf[--at] = '0';
    while (value && at > 0) {
        num_buf[--at] = (char)('0' + value % 10ULL);
        value /= 10ULL;
    }
    return &num_buf[at];
}

static void write_u64(uint64_t value) {
    write_cstr(u64_text(value));
}

static int parse_u32(const char* text, uint32_t* out) {
    uint64_t value = 0;
    uint64_t i = 0;
    if (!text || !text[0] || !out) return -1;
    while (text[i]) {
        if (text[i] < '0' || text[i] > '9') return -1;
        value = value * 10ULL + (uint64_t)(text[i] - '0');
        if (value > 0xFFFFFFFFULL) return -1;
        i++;
    }
    *out = (uint32_t)value;
    return 0;
}

static int read_line(char* out, uint64_t cap) {
    uint64_t len = 0;
    if (!out || cap == 0) return -1;
    for (;;) {
        char c = 0;
        long rc = syscall3(SYS_READ, 0, (long)&c, 1);
        if (rc <= 0) return -1;
        if (c == '\r') continue;
        if (c == '\n') {
            write_cstr("\n");
            out[len] = 0;
            return (int)len;
        }
        if (c == 8 || c == 127) {
            if (len > 0) {
                len--;
                write_cstr("\b \b");
            }
            continue;
        }
        if (c >= 32 && c <= 126 && len + 1 < cap) {
            out[len++] = c;
            syscall3(SYS_WRITE, 1, (long)&c, 1);
        }
    }
}

static int prompt(const char* label, char* out, uint64_t cap, const char* current) {
    write_cstr(label);
    if (current && current[0]) {
        write_cstr(" [");
        write_cstr(current);
        write_cstr("]");
    }
    write_cstr(": ");
    if (read_line(line, sizeof(line)) < 0) return -1;
    if (line[0]) copy_cstr(out, cap, line);
    return 0;
}

static int choose(const char* title, const char* const* options, uint32_t count, uint32_t current) {
    uint32_t choice;
    write_cstr("\n");
    write_cstr(title);
    write_cstr("\n");
    for (uint32_t i = 0; i < count; i++) {
        write_cstr("  [");
        write_u64(i + 1);
        write_cstr("] ");
        write_cstr(options[i]);
        if (i == current) write_cstr("  (selected)");
        write_cstr("\n");
    }
    write_cstr("Choice: ");
    if (read_line(line, sizeof(line)) < 0 || parse_u32(line, &choice) != 0 || choice == 0 || choice > count) {
        write_cstr("Invalid choice; current selection kept.\n");
        return (int)current;
    }
    return (int)(choice - 1);
}

static void defaults(void) {
    clear_bytes(&config, sizeof(config));
    config.version = AOS_INSTALL_ABI_VERSION;
    config.partition_mode = AOS_INSTALL_PARTITION_AUTO;
    config.account_admin = 1;
    config.auto_login = 0;
    config.wifi_enabled = 0;
    config.wifi_static = 0;
    config.driver_mode = AOS_INSTALL_DRIVERS_STABLE;
    config.driver_scan_mask = 0x0FU;
    config.app_profile = AOS_INSTALL_APPS_FULL;
    config.app_mask = 0xFFU;
    copy_cstr(config.username, sizeof(config.username), "explorer");
    copy_cstr(config.hostname, sizeof(config.hostname), "aos");
    copy_cstr(config.language, sizeof(config.language), "English");
    copy_cstr(config.region, sizeof(config.region), "India");
    copy_cstr(config.timezone, sizeof(config.timezone), "Asia/Kolkata");
    copy_cstr(config.keyboard, sizeof(config.keyboard), "US English");
}

static int query_status(void) {
    clear_bytes(&status, sizeof(status));
    return syscall2(AOS_SYS_INSTALLER, AOS_INSTALL_QUERY, (long)&status) < 0 ? -1 : 0;
}

static void print_disk_size(uint64_t bytes) {
    write_u64(bytes / (1024ULL * 1024ULL));
    write_cstr(" MiB");
}

static uint32_t list_disks(void) {
    struct aos_blkdev_user disk;
    uint32_t hardware_count = 0;
    write_cstr("\nDetected writable hardware disks:\n");
    for (uint32_t i = 0; i < 16; i++) {
        clear_bytes(&disk, sizeof(disk));
        if (syscall2(AOS_SYS_BLKDEV_INFO, i, (long)&disk) < 0) break;
        if (!disk.has_ops) continue;
        write_cstr("  id=");
        write_u64(disk.id);
        write_cstr("  ");
        write_cstr(disk.name);
        write_cstr("  ");
        print_disk_size(disk.size);
        if (disk.read_only) write_cstr("  read-only");
        if (disk.id == config.target_blkdev_id) write_cstr("  selected");
        write_cstr("\n");
        hardware_count++;
    }
    if (!hardware_count) write_cstr("  none\n");
    return hardware_count;
}

static void configure_account(void) {
    static const char* const account_types[] = {"Administrator", "Standard user"};
    static const char* const login_modes[] = {"Require password", "Auto-login"};
    char password[64];
    char confirm[64];
    int choice;
    write_cstr("\nAOS INSTALLER - USERNAME AND PASSWORD\n");
    prompt("Username", config.username, sizeof(config.username), config.username);
    password[0] = 0;
    confirm[0] = 0;
    prompt("Password (input is visible; at least 8 characters)", password, sizeof(password), "");
    prompt("Confirm password", confirm, sizeof(confirm), "");
    if (cstrlen(password) >= 8U && streq(password, confirm)) {
        copy_cstr(config.password, sizeof(config.password), password);
        copy_cstr(config.password_confirm, sizeof(config.password_confirm), confirm);
    } else {
        write_cstr("Password needs at least 8 characters and both entries must match; previous password kept.\n");
    }
    choice = choose("Account type", account_types, 2, config.account_admin ? 0 : 1);
    config.account_admin = choice == 0;
    choice = choose("Login mode", login_modes, 2, config.auto_login ? 1 : 0);
    config.auto_login = choice == 1;
    clear_bytes(password, sizeof(password));
    clear_bytes(confirm, sizeof(confirm));
}

static void configure_partitioning(void) {
    static const char* const modes[] = {"Auto-partition selected disk", "Manual partitioning"};
    uint32_t id;
    int choice;
    write_cstr("\nAOS INSTALLER - PARTITIONING\n");
    write_cstr("WARNING: installation erases the selected disk.\n");
    list_disks();
    write_cstr("Target disk id [");
    write_u64(config.target_blkdev_id);
    write_cstr("]: ");
    if (read_line(line, sizeof(line)) >= 0 && line[0] && parse_u32(line, &id) == 0) config.target_blkdev_id = id;
    choice = choose("Partitioning mode", modes, 2, config.partition_mode);
    config.partition_mode = (uint8_t)choice;
    if (config.partition_mode == AOS_INSTALL_PARTITION_MANUAL) {
        write_cstr("Manual mode uses the 'partition' command. The final layout must contain a FAT boot partition followed by an AOSFS root.\n");
        write_cstr("The current bootable installer only commits validated automatic layouts.\n");
    }
}

static void configure_language(void) {
    static const char* const values[] = {"English", "Hindi", "Other / custom"};
    int choice = choose("AOS INSTALLER - LANGUAGE", values, 3, streq(config.language, "Hindi") ? 1 : 0);
    if (choice == 2) prompt("Language", config.language, sizeof(config.language), config.language);
    else copy_cstr(config.language, sizeof(config.language), values[choice]);
}

static void configure_region(void) {
    static const char* const values[] = {"India", "United States", "United Kingdom", "Other / custom"};
    int current = streq(config.region, "United States") ? 1 : streq(config.region, "United Kingdom") ? 2 : 0;
    int choice = choose("AOS INSTALLER - COUNTRY / REGION", values, 4, current);
    if (choice == 3) prompt("Country or region", config.region, sizeof(config.region), config.region);
    else copy_cstr(config.region, sizeof(config.region), values[choice]);
}

static void configure_timezone(void) {
    static const char* const values[] = {"Asia/Kolkata", "UTC", "America/New_York", "Europe/London", "Manual entry"};
    int current = streq(config.timezone, "UTC") ? 1 : streq(config.timezone, "America/New_York") ? 2 : streq(config.timezone, "Europe/London") ? 3 : 0;
    int choice = choose("AOS INSTALLER - TIMEZONE", values, 5, current);
    if (choice == 4) prompt("Timezone", config.timezone, sizeof(config.timezone), config.timezone);
    else copy_cstr(config.timezone, sizeof(config.timezone), values[choice]);
}

static void configure_wifi(void) {
    static const char* const enabled[] = {"Skip Wi-Fi setup", "Save a Wi-Fi profile"};
    static const char* const modes[] = {"DHCP automatically", "Static IP"};
    int choice = choose("AOS INSTALLER - PRE-CONFIGURE WI-FI", enabled, 2, config.wifi_enabled ? 1 : 0);
    config.wifi_enabled = choice == 1;
    if (!config.wifi_enabled) return;
    prompt("Network name / SSID", config.wifi_ssid, sizeof(config.wifi_ssid), config.wifi_ssid);
    prompt("Wi-Fi password (input is visible)", config.wifi_password, sizeof(config.wifi_password), "");
    choice = choose("IP mode", modes, 2, config.wifi_static ? 1 : 0);
    config.wifi_static = choice == 1;
    if (config.wifi_static) {
        prompt("Static IP", config.wifi_ip, sizeof(config.wifi_ip), config.wifi_ip);
        prompt("Gateway", config.wifi_gateway, sizeof(config.wifi_gateway), config.wifi_gateway);
        prompt("DNS", config.wifi_dns, sizeof(config.wifi_dns), config.wifi_dns);
    }
    write_cstr("Profile saved for first-boot networking.\n");
}

static uint32_t count_info_syscall(uint32_t syscall_number) {
    uint8_t info[256];
    uint32_t count = 0;
    while (count < 128) {
        clear_bytes(info, sizeof(info));
        if (syscall2(syscall_number, count, (long)info) < 0) break;
        count++;
    }
    return count;
}

static void configure_drivers(void) {
    static const char* const modes[] = {"Stable drivers only", "Include experimental drivers", "Ask before each driver", "Skip driver installation"};
    int choice;
    write_cstr("\nAOS INSTALLER - DEVICE SCAN AND DRIVERS\n");
    write_cstr("Scanning PCI, storage, network, and USB registries...\n");
    write_cstr("PCI devices: ");
    write_u64(count_info_syscall(AOS_SYS_PCI_INFO));
    write_cstr("\nRegistered drivers: ");
    write_u64(count_info_syscall(AOS_SYS_DRIVER_INFO));
    write_cstr("\n");
    config.driver_scan_mask = 0x0FU;
    choice = choose("Driver installation mode", modes, 4, config.driver_mode);
    config.driver_mode = (uint8_t)choice;
    if (choice == AOS_INSTALL_DRIVERS_SKIP) config.driver_scan_mask = 0;
}

static void configure_apps(void) {
    static const char* const profiles[] = {"Minimal system", "Basic tools", "Full default system", "Custom selection"};
    int choice = choose("AOS INSTALLER - ADDITIONAL APPS", profiles, 4, config.app_profile);
    config.app_profile = (uint8_t)choice;
    if (choice == AOS_INSTALL_APPS_MINIMAL) config.app_mask = 0;
    else if (choice == AOS_INSTALL_APPS_BASIC) config.app_mask = 0x1FU;
    else if (choice == AOS_INSTALL_APPS_FULL) config.app_mask = 0xFFU;
    else {
        write_cstr("App bits: afetch=1 aosctl=2 acur=4 editor=8 files=16 network=32 monitor=64 development=128\nMask [");
        write_u64(config.app_mask);
        write_cstr("]: ");
        uint32_t mask;
        if (read_line(line, sizeof(line)) >= 0 && line[0] && parse_u32(line, &mask) == 0) config.app_mask = mask & 0xFFU;
    }
}

static void configure_keyboard(void) {
    static const char* const values[] = {"US English", "UK English", "Indian English", "Hindi / InScript", "Custom layout"};
    int current = streq(config.keyboard, "UK English") ? 1 : streq(config.keyboard, "Indian English") ? 2 : streq(config.keyboard, "Hindi / InScript") ? 3 : 0;
    int choice = choose("AOS INSTALLER - KEYBOARD LAYOUT", values, 5, current);
    if (choice == 4) prompt("Keyboard layout", config.keyboard, sizeof(config.keyboard), config.keyboard);
    else copy_cstr(config.keyboard, sizeof(config.keyboard), values[choice]);
    write_cstr("Keyboard test (type a line): ");
    read_line(line, sizeof(line));
}

static void configure_hostname(void) {
    write_cstr("\nAOS INSTALLER - HOSTNAME\nUse lowercase letters, digits, and hyphens.\n");
    prompt("Hostname", config.hostname, sizeof(config.hostname), config.hostname);
}

static const char* app_profile_name(void) {
    if (config.app_profile == AOS_INSTALL_APPS_MINIMAL) return "minimal";
    if (config.app_profile == AOS_INSTALL_APPS_BASIC) return "basic";
    if (config.app_profile == AOS_INSTALL_APPS_FULL) return "full";
    return "custom";
}

static void print_summary(void) {
    write_cstr("\nAOS INSTALLER - INSTALL AOS\n\nCurrent configuration:\n  Username: ");
    write_cstr(config.username);
    write_cstr("\n  Hostname: ");
    write_cstr(config.hostname);
    write_cstr("\n  Language: ");
    write_cstr(config.language);
    write_cstr("\n  Country / Region: ");
    write_cstr(config.region);
    write_cstr("\n  Timezone: ");
    write_cstr(config.timezone);
    write_cstr("\n  Keyboard: ");
    write_cstr(config.keyboard);
    write_cstr("\n  Wi-Fi: ");
    write_cstr(config.wifi_enabled ? "profile configured" : "skipped");
    write_cstr("\n  Partition mode: ");
    write_cstr(config.partition_mode == AOS_INSTALL_PARTITION_AUTO ? "automatic" : "manual");
    write_cstr("\n  Target disk id: ");
    write_u64(config.target_blkdev_id);
    write_cstr("\n  Drivers: ");
    write_cstr(config.driver_mode == AOS_INSTALL_DRIVERS_SKIP ? "skipped" : "auto-scanned");
    write_cstr("\n  Additional apps: ");
    write_cstr(app_profile_name());
    write_cstr("\n");
}

static const char* step_name(uint32_t step) {
    switch (step) {
        case AOS_INSTALL_STEP_PREPARING: return "Preparing installer";
        case AOS_INSTALL_STEP_CHECKING_DISK: return "Checking target disk";
        case AOS_INSTALL_STEP_PARTITIONING: return "Creating partitions";
        case AOS_INSTALL_STEP_FORMATTING: return "Formatting filesystem";
        case AOS_INSTALL_STEP_COPYING_KERNEL: return "Copying kernel";
        case AOS_INSTALL_STEP_COPYING_INITRD: return "Copying initrd";
        case AOS_INSTALL_STEP_COPYING_USERSPACE: return "Copying userspace";
        case AOS_INSTALL_STEP_WRITING_CONFIG: return "Writing system configuration";
        case AOS_INSTALL_STEP_INSTALLING_BOOTLOADER: return "Installing bootloader";
        case AOS_INSTALL_STEP_CREATING_LOG: return "Creating install log";
        case AOS_INSTALL_STEP_FINISHING: return "Finishing installation";
        case AOS_INSTALL_STEP_COMPLETE: return "Installation complete";
        case AOS_INSTALL_STEP_ERROR: return "Installation failed";
        default: return "Idle";
    }
}

static int run_install(void) {
    uint32_t last_step = 0xFFFFFFFFU;
    uint32_t last_bucket = 0xFFFFFFFFU;
    long rc;
    rc = syscall2(AOS_SYS_INSTALLER, AOS_INSTALL_BEGIN, (long)&config);
    if (rc < 0) {
        query_status();
        write_cstr("arootinstall: ");
        write_cstr(status.message);
        write_cstr("\n");
        return -1;
    }
    write_cstr("\nINSTALLATION PROGRESS\n");
    for (;;) {
        if (query_status() != 0) return -1;
        uint32_t bucket = status.progress_percent / 5U;
        if (status.step != last_step || bucket != last_bucket) {
            write_cstr("[");
            write_u64(status.progress_percent);
            write_cstr("%] ");
            write_cstr(step_name(status.step));
            write_cstr(" - ");
            write_cstr(status.message);
            write_cstr("\n");
            last_step = status.step;
            last_bucket = bucket;
        }
        if (status.step == AOS_INSTALL_STEP_COMPLETE) return 0;
        if (status.step == AOS_INSTALL_STEP_ERROR) return -1;
        rc = syscall2(AOS_SYS_INSTALLER, AOS_INSTALL_ADVANCE, 0);
        if (rc < 0) {
            query_status();
            write_cstr("arootinstall: ");
            write_cstr(status.message);
            write_cstr("\n");
            return -1;
        }
    }
}

static int confirm_and_install(void) {
    print_summary();
    write_cstr("\nFINAL WARNING: all data on the selected disk will be erased.\nType AOS to start installation: ");
    if (read_line(line, sizeof(line)) < 0) return -1;
    copy_cstr(config.confirmation, sizeof(config.confirmation), line);
    if (!streq(config.confirmation, "AOS")) {
        write_cstr("Confirmation did not match. Nothing was changed.\n");
        return -1;
    }
    if (run_install() != 0) return -1;
    write_cstr("\nAOS installation is complete.\nRemove live media before rebooting.\n");
    write_cstr("[1] Reboot now  [2] Return to shell: ");
    if (read_line(line, sizeof(line)) >= 0 && streq(line, "1")) syscall1(AOS_SYS_RESTART, 0);
    return 0;
}

static void print_main_menu(void) {
    write_cstr("\n============================================================\n");
    write_cstr(" AOS INSTALLER - LIVE MODE\n");
    write_cstr("============================================================\n");
    write_cstr(" 1. Username and password\n 2. Partitioning\n 3. Language\n");
    write_cstr(" 4. Country / region\n 5. Timezone\n 6. Pre-configure Wi-Fi\n");
    write_cstr(" 7. Scan devices and drivers\n 8. Additional apps\n");
    write_cstr(" 9. Keyboard layout\n10. Hostname\n11. Review and INSTALL\n");
    write_cstr(" 0. Exit without changes\nChoice: ");
}

static int interactive(void) {
    uint32_t choice;
    if (query_status() != 0) {
        write_cstr("arootinstall: installer service unavailable\n");
        return 1;
    }
    if (!status.live_mode) {
        write_cstr("arootinstall: installation is available from live mode only\n");
        return 1;
    }
    if (!status.payload_ready) {
        write_cstr("arootinstall: boot payload missing; boot the complete AOS live image\n");
        return 1;
    }
    config.target_blkdev_id = status.target_blkdev_id;
    for (;;) {
        print_main_menu();
        if (read_line(line, sizeof(line)) < 0) return 1;
        if (parse_u32(line, &choice) != 0) continue;
        if (choice == 0) return 0;
        if (choice == 1) configure_account();
        else if (choice == 2) configure_partitioning();
        else if (choice == 3) configure_language();
        else if (choice == 4) configure_region();
        else if (choice == 5) configure_timezone();
        else if (choice == 6) configure_wifi();
        else if (choice == 7) configure_drivers();
        else if (choice == 8) configure_apps();
        else if (choice == 9) configure_keyboard();
        else if (choice == 10) configure_hostname();
        else if (choice == 11) return confirm_and_install() == 0 ? 0 : 1;
    }
}

static void usage(void) {
    write_cstr("usage: arootinstall [--status|--dry-run|--install-defaults --target ID --password PASSWORD --confirm AOS]\n");
    write_cstr("       [--username NAME] [--standard] [--autologin]\n");
    write_cstr("Without arguments, opens the eleven-section live-mode installer.\n");
}

static int print_status_command(void) {
    if (query_status() != 0) return 1;
    write_cstr("live mode: ");
    write_cstr(status.live_mode ? "yes" : "no");
    write_cstr("\npayload: ");
    write_cstr(status.payload_ready ? "ready" : "missing");
    write_cstr("\nrecommended target: ");
    write_cstr(status.target_name[0] ? status.target_name : "none");
    if (status.target_name[0]) {
        write_cstr(" (id ");
        write_u64(status.target_blkdev_id);
        write_cstr(", ");
        print_disk_size(status.target_size);
        write_cstr(")");
    }
    write_cstr("\nstate: ");
    write_cstr(step_name(status.step));
    write_cstr("\nmessage: ");
    write_cstr(status.message);
    write_cstr("\n");
    return 0;
}

void aos_main(uint64_t argc, char** argv) {
    int install_defaults = 0;
    int dry_run = 0;
    uint32_t target = 0;
    int have_target = 0;
    const char* confirmation = NULL;
    const char* password = NULL;
    const char* username = NULL;

    defaults();
    if (argc == 1) exit_code(interactive());
    for (uint64_t i = 1; i < argc; i++) {
        if (streq(argv[i], "--help")) {
            usage();
            exit_code(0);
        } else if (streq(argv[i], "--status")) {
            exit_code(print_status_command());
        } else if (streq(argv[i], "--dry-run")) {
            dry_run = 1;
        } else if (streq(argv[i], "--install-defaults")) {
            install_defaults = 1;
        } else if (streq(argv[i], "--target") && i + 1 < argc) {
            if (parse_u32(argv[++i], &target) != 0) {
                write_cstr("arootinstall: invalid target id\n");
                exit_code(2);
            }
            have_target = 1;
        } else if (streq(argv[i], "--confirm") && i + 1 < argc) {
            confirmation = argv[++i];
        } else if (streq(argv[i], "--password") && i + 1 < argc) {
            password = argv[++i];
        } else if (streq(argv[i], "--username") && i + 1 < argc) {
            username = argv[++i];
        } else if (streq(argv[i], "--standard")) {
            config.account_admin = 0;
        } else if (streq(argv[i], "--autologin")) {
            config.auto_login = 1;
        } else {
            usage();
            exit_code(2);
        }
    }
    if (query_status() != 0 || !status.live_mode || !status.payload_ready) {
        print_status_command();
        exit_code(1);
    }
    config.target_blkdev_id = have_target ? target : status.target_blkdev_id;
    if (username) copy_cstr(config.username, sizeof(config.username), username);
    if (password) {
        copy_cstr(config.password, sizeof(config.password), password);
        copy_cstr(config.password_confirm, sizeof(config.password_confirm), password);
    }
    if (dry_run) {
        print_summary();
        list_disks();
        write_cstr("Dry run only; no disk changes were made.\n");
        exit_code(0);
    }
    if (!install_defaults || !confirmation || !streq(confirmation, "AOS") ||
        cstrlen(config.password) < 8U) {
        write_cstr("arootinstall: unattended installation requires --install-defaults, --password (8+ characters), and --confirm AOS\n");
        exit_code(2);
    }
    copy_cstr(config.confirmation, sizeof(config.confirmation), confirmation);
    print_summary();
    write_cstr("Explicit AOS confirmation received; starting destructive installation.\n");
    exit_code(run_install() == 0 ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
