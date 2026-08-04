/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_BLUETOOTH_INFO 549
#define AOS_SYS_BLUETOOTH_CONTROL 550
#define AOS_SYS_BLUETOOTH_DEVICE_INFO 551
#define AOS_SYS_BLUETOOTH_BOND_INFO 552
#define LINE_MAX 96

#define BLUETOOTH_CONTROLLER_NONE 0
#define BLUETOOTH_CONTROLLER_USB_HCI 1
#define BLUETOOTH_CONTROL_SCAN 1
#define BLUETOOTH_CONTROL_PAIR 2

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

struct bluetooth_device_info {
    uint8_t valid;
    uint8_t address[6];
    uint8_t page_scan_repetition_mode;
    uint8_t has_rssi;
    int8_t rssi;
    uint8_t has_name;
    uint8_t name_status;
    uint8_t connected;
    uint8_t link_type;
    uint8_t encryption_enabled;
    uint8_t pair_status;
    uint8_t bonded;
    uint16_t connection_handle;
    char name[64];
} __attribute__((packed));

struct bluetooth_bond_info {
    uint8_t valid;
    uint8_t address[6];
    uint8_t key_type;
    uint8_t authenticated;
    uint8_t has_name;
    uint8_t reserved[3];
    char name[64];
} __attribute__((packed));

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

static long syscall3(long n, long a, long b, long c);

static long syscall_read(long fd, char* buf, long len) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(SYS_READ), "D"(fd), "S"(buf), "d"(len)
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

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int starts_with_word(const char* line, const char* word) {
    uint64_t i = 0;

    if (!line || !word) return 0;
    while (word[i]) {
        if (line[i] != word[i]) return 0;
        i++;
    }
    return line[i] == ' ' || line[i] == '\t';
}

static int parse_u64(const char* s, uint64_t* out) {
    uint64_t value = 0;
    uint64_t i = 0;
    int seen = 0;

    if (!s || !out) return 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    while (s[i] >= '0' && s[i] <= '9') {
        value = value * 10 + (uint64_t)(s[i] - '0');
        seen = 1;
        i++;
    }
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (!seen || s[i] != '\0') return 0;
    *out = value;
    return 1;
}

static int command_arg_u64(const char* line, const char* command, uint64_t* out) {
    uint64_t len;

    if (!starts_with_word(line, command)) return 0;
    len = cstrlen(command);
    return parse_u64(line + len, out);
}

static const char* basename(const char* path) {
    const char* base = path;
    uint64_t i = 0;

    if (!path) return "";
    while (path[i]) {
        if (path[i] == '/') base = &path[i + 1];
        i++;
    }
    return base;
}

static void write_buf(const char* s, uint64_t n) {
    syscall3(SYS_WRITE, 1, (long)s, (long)n);
}

static void write_cstr(const char* s) {
    write_buf(s, cstrlen(s));
}

static void write_hex_nibble(uint8_t v) {
    static const char hex[] = "0123456789abcdef";
    char c = hex[v & 0xf];
    write_buf(&c, 1);
}

static void write_hex8(uint8_t v) {
    write_hex_nibble((uint8_t)(v >> 4));
    write_hex_nibble(v);
}

static void write_hex16(uint16_t v) {
    write_hex8((uint8_t)(v >> 8));
    write_hex8((uint8_t)v);
}

static void write_u64(uint64_t v) {
    char buf[21];
    uint64_t i = sizeof(buf) - 1;
    buf[i] = '\0';
    if (v == 0) {
        write_cstr("0");
        return;
    }
    while (v && i > 0) {
        buf[--i] = (char)('0' + (v % 10));
        v /= 10;
    }
    write_cstr(&buf[i]);
}

static void write_i64(int64_t v) {
    if (v < 0) {
        write_cstr("-");
        write_u64((uint64_t)-v);
    } else {
        write_u64((uint64_t)v);
    }
}

static void write_controller(uint8_t type) {
    switch (type) {
        case BLUETOOTH_CONTROLLER_NONE:
            write_cstr("none");
            break;
        case BLUETOOTH_CONTROLLER_USB_HCI:
            write_cstr("usb-hci");
            break;
        default:
            write_cstr("unknown");
            break;
    }
}

static void exit_code(int code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

static int read_line(char* out, uint64_t out_size) {
    uint64_t len = 0;

    if (!out || out_size == 0) return -1;
    for (;;) {
        char c = 0;
        long ret = syscall_read(0, &c, 1);
        if (ret <= 0) return -1;

        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            write_cstr("\n");
            out[len] = '\0';
            return (int)len;
        }
        if (c == 8 || c == 127) {
            if (len > 0) {
                len--;
                write_cstr("\b \b");
            }
            continue;
        }
        if (len + 1 < out_size) {
            out[len++] = c;
            write_buf(&c, 1);
        }
    }
}

static void trim_line(char* line) {
    uint64_t start = 0;
    uint64_t end;
    uint64_t out = 0;

    while (line[start] == ' ' || line[start] == '\t') start++;
    end = cstrlen(line + start);
    while (end > 0 && (line[start + end - 1] == ' ' || line[start + end - 1] == '\t')) {
        end--;
    }
    while (out < end) {
        line[out] = line[start + out];
        out++;
    }
    line[out] = '\0';
}

static void print_status(void) {
    struct bluetooth_info info;
    long ret = syscall1(AOS_SYS_BLUETOOTH_INFO, (long)&info);

    if (ret < 0) {
        write_cstr("Bluetooth: kernel service unavailable\n");
        return;
    }

    write_cstr("Bluetooth: ");
    write_cstr(info.status[0] ? info.status : "unknown");
    write_cstr("\n");

    write_cstr("driver: ");
    write_cstr(info.driver[0] ? info.driver : "none");
    write_cstr("\n");

    write_cstr("controller: ");
    write_controller(info.controller_type);
    write_cstr("\n");

    if (info.present) {
        write_cstr("interface: number=0x");
        write_hex8(info.interface_number);
        write_cstr(" class=0x");
        write_hex8(info.interface_class);
        write_cstr(" subclass=0x");
        write_hex8(info.interface_subclass);
        write_cstr(" protocol=0x");
        write_hex8(info.interface_protocol);
        write_cstr(" eps=0x");
        write_hex8(info.endpoints);
        write_cstr("\n");

        write_cstr("event endpoint: addr=0x");
        write_hex8(info.event_endpoint);
        write_cstr(" max_packet=0x");
        write_hex16(info.event_max_packet);
        write_cstr(" interval=0x");
        write_hex8(info.event_interval);
        write_cstr("\n");
    }

    if (info.hci_ready) {
        write_cstr("hci: version=0x");
        write_hex8(info.hci_version);
        write_cstr(" revision=0x");
        write_hex16(info.hci_revision);
        write_cstr(" lmp=0x");
        write_hex8(info.lmp_version);
        write_cstr(" manufacturer=0x");
        write_hex16(info.manufacturer);
        write_cstr(" lmp_subversion=0x");
        write_hex16(info.lmp_subversion);
        write_cstr("\n");
    } else if (info.last_event || info.last_status) {
        write_cstr("hci: last_event=0x");
        write_hex8(info.last_event);
        write_cstr(" status=0x");
        write_hex8(info.last_status);
        write_cstr("\n");
    }

    write_cstr("discovery: ");
    if (info.scanning) {
        write_cstr("scanning");
    } else {
        write_cstr("idle");
    }
    write_cstr(" devices=");
    write_u64(info.discovered_count);
    write_cstr(" last_status=0x");
    write_hex8(info.last_scan_status);
    write_cstr(" pairing=");
    write_cstr(info.pairing ? "running" : "idle");
    write_cstr(" last_pair=0x");
    write_hex8(info.last_pair_status);
    write_cstr("\n");
}

static void print_shell_help(void) {
    write_cstr("Commands: status help scan list paired pair <index> connect <index> exit\n");
    write_cstr("scan runs HCI inquiry; pair/connect stores link keys when the controller provides them.\n");
}

static void write_bt_address(const uint8_t address[6]) {
    for (int i = 5; i >= 0; i--) {
        if (i != 5) write_cstr(":");
        write_hex8(address[i]);
    }
}

static uint64_t list_devices(void) {
    uint64_t shown = 0;

    for (uint64_t i = 0; i < 16; i++) {
        struct bluetooth_device_info dev;
        long ret = syscall2(AOS_SYS_BLUETOOTH_DEVICE_INFO, (long)i, (long)&dev);
        if (ret < 0) {
            break;
        }
        if (!dev.valid) {
            continue;
        }
        write_cstr("device ");
        write_u64(shown);
        write_cstr(": ");
        write_bt_address(dev.address);
        if (dev.has_name && dev.name[0]) {
            write_cstr(" name=\"");
            write_cstr(dev.name);
            write_cstr("\"");
        }
        write_cstr(" page_scan=0x");
        write_hex8(dev.page_scan_repetition_mode);
        if (dev.has_rssi) {
            write_cstr(" rssi=");
            write_i64(dev.rssi);
            write_cstr(" dBm");
        }
        if (dev.connected) {
            write_cstr(" connected handle=0x");
            write_hex16(dev.connection_handle);
            write_cstr(" link=0x");
            write_hex8(dev.link_type);
            write_cstr(" enc=0x");
            write_hex8(dev.encryption_enabled);
        } else if (dev.pair_status) {
            write_cstr(" pair_status=0x");
            write_hex8(dev.pair_status);
        }
        if (dev.bonded) {
            write_cstr(" bonded");
        }
        write_cstr("\n");
        shown++;
    }

    if (shown == 0) {
        write_cstr("No Bluetooth devices discovered.\n");
    }
    return shown;
}

static uint64_t list_paired_devices(void) {
    uint64_t shown = 0;

    for (uint64_t i = 0; i < 16; i++) {
        struct bluetooth_bond_info bond;
        long ret = syscall2(AOS_SYS_BLUETOOTH_BOND_INFO, (long)i, (long)&bond);
        if (ret < 0) {
            break;
        }
        if (!bond.valid) {
            continue;
        }

        write_cstr("paired ");
        write_u64(shown);
        write_cstr(": ");
        write_bt_address(bond.address);
        if (bond.has_name && bond.name[0]) {
            write_cstr(" name=\"");
            write_cstr(bond.name);
            write_cstr("\"");
        }
        write_cstr(" key_type=0x");
        write_hex8(bond.key_type);
        write_cstr(" auth=");
        write_cstr(bond.authenticated ? "yes" : "no");
        write_cstr("\n");
        shown++;
    }

    if (shown == 0) {
        write_cstr("No paired Bluetooth devices remembered.\n");
    }
    return shown;
}

static void run_scan(void) {
    long ret;

    write_cstr("Starting Bluetooth inquiry...\n");
    ret = syscall1(AOS_SYS_BLUETOOTH_CONTROL, BLUETOOTH_CONTROL_SCAN);
    if (ret < 0) {
        write_cstr("scan failed: Bluetooth HCI adapter is not ready\n");
        print_status();
        return;
    }
    write_cstr("scan complete\n");
    list_devices();
}

static void run_pair(uint64_t index) {
    long ret;

    write_cstr("Connecting Bluetooth device ");
    write_u64(index);
    write_cstr("...\n");
    ret = syscall2(AOS_SYS_BLUETOOTH_CONTROL, BLUETOOTH_CONTROL_PAIR, (long)index);
    if (ret < 0) {
        write_cstr("pair failed: run scan first and choose a listed device index\n");
        print_status();
        return;
    }
    write_cstr("pair complete\n");
    list_devices();
}

static void bluetooth_shell(void) {
    char line[LINE_MAX];

    write_cstr("Bluetooth shell active\n");
    print_status();
    print_shell_help();

    for (;;) {
        write_cstr("bluetooth# ");
        if (read_line(line, sizeof(line)) < 0) {
            write_cstr("\n");
            return;
        }
        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }
        if (streq(line, "exit") || streq(line, "quit")) {
            return;
        }
        if (streq(line, "help") || streq(line, "?")) {
            print_shell_help();
            continue;
        }
        if (streq(line, "status") || streq(line, "info")) {
            print_status();
            continue;
        }
        if (streq(line, "scan")) {
            run_scan();
            continue;
        }
        if (streq(line, "list")) {
            list_devices();
            continue;
        }
        if (streq(line, "paired") || streq(line, "bonds")) {
            list_paired_devices();
            continue;
        }
        if (streq(line, "pair") || streq(line, "connect")) {
            write_cstr("usage: pair <device-index>\n");
            continue;
        }
        {
            uint64_t index = 0;
            if (command_arg_u64(line, "pair", &index) ||
                command_arg_u64(line, "connect", &index)) {
                run_pair(index);
                continue;
            }
        }
        if (starts_with_word(line, "pair") || starts_with_word(line, "connect")) {
            write_cstr("usage: pair <device-index>\n");
            continue;
        }
        write_cstr("unknown command: ");
        write_cstr(line);
        write_cstr("\n");
    }
}

void aos_main(uint64_t argc, char** argv) {
    const char* cmd = argc > 0 ? basename(argv[0]) : "bluetooth";

    if (streq(cmd, "bluethoot-NOW") || streq(cmd, "bluetooth-NOW") ||
        streq(cmd, "bluethoot-now") || streq(cmd, "bluetooth-now") ||
        (argc > 1 && (streq(argv[1], "shell") || streq(argv[1], "now")))) {
        bluetooth_shell();
        exit_code(0);
    }

    if (argc > 1 && (streq(argv[1], "-h") || streq(argv[1], "--help"))) {
        write_cstr("usage: bluetooth [status|scan|list|paired|pair INDEX|connect INDEX|shell]\n");
        write_cstr("       bt [status|scan|list|paired|pair INDEX|connect INDEX|shell]\n");
        write_cstr("       bluethoot-NOW\n");
        exit_code(0);
    }

    if (argc > 1 && streq(argv[1], "scan")) {
        run_scan();
        exit_code(0);
    }

    if (argc > 1 && streq(argv[1], "list")) {
        list_devices();
        exit_code(0);
    }

    if (argc > 1 && (streq(argv[1], "paired") || streq(argv[1], "bonds"))) {
        list_paired_devices();
        exit_code(0);
    }

    if (argc > 1 && (streq(argv[1], "pair") || streq(argv[1], "connect"))) {
        uint64_t index = 0;
        if (argc < 3 || !parse_u64(argv[2], &index)) {
            write_cstr("usage: bluetooth pair <device-index>\n");
            exit_code(1);
        }
        run_pair(index);
        exit_code(0);
    }

    if (argc > 1 && !streq(argv[1], "status")) {
        write_cstr("bluetooth: unknown command: ");
        write_cstr(argv[1]);
        write_cstr("\n");
        write_cstr("run bluethoot-NOW for the Bluetooth shell\n");
        exit_code(1);
    }

    print_status();

    exit_code(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "and $-16, %rsp\n"
        "call aos_main\n"
        "hlt\n"
    );
}
