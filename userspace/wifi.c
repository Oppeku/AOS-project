/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXIT 60
#define AOS_SYS_PCI_INFO 518
#define AOS_SYS_FIRMWARE_INFO 529
#define AOS_SYS_WIFI_SCAN_INFO 530
#define AOS_SYS_WIFI_STATE_INFO 536
#define AOS_SYS_WIFI_CONTROL 537

#define PCI_CLASS_NETWORK 0x02
#define PCI_SUBCLASS_WIFI 0x80

#define MAC80211_SECURITY_OPEN 0
#define MAC80211_SECURITY_WPA2 1
#define MAC80211_SECURITY_WPA3 2

struct pci_info {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint8_t irq_line;
    uint8_t reserved;
    uint32_t bar[6];
} __attribute__((packed));

struct firmware_info {
    char name[96];
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed));

struct wifi_scan_info {
    char ssid[32];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t security;
    uint8_t reserved[7];
} __attribute__((packed));

struct wifi_state_info {
    uint8_t state;
    uint8_t scan_count;
    uint8_t selected;
    uint8_t security;
    char ssid[32];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t reserved[4];
    uint32_t auth_attempts;
    uint32_t assoc_attempts;
    uint32_t rx_mgmt;
    uint32_t rx_beacon;
    uint32_t rx_probe_resp;
    uint32_t rx_data;
    uint32_t rx_other;
    uint32_t tx_mgmt;
    uint32_t tx_probe_req;
    uint32_t last_tx_len;
    uint32_t driver_tx_calls;
    uint32_t driver_tx_hw;
    uint32_t driver_tx_sim;
    uint32_t driver_tx_errors;
    uint32_t driver_rx_calls;
    uint32_t driver_rx_accepted;
    uint32_t driver_rx_errors;
    uint32_t driver_rx_last_len;
    int8_t driver_rx_last_rssi;
    uint8_t reserved2[3];
} __attribute__((packed));

struct wifi_driver_target {
    uint16_t vendor;
    const char* vendor_name;
    const char* driver;
    const char* firmware;
    const char* stage;
    const char* priority;
    const char* effort;
    const char* blocker;
};

static const struct wifi_driver_target driver_targets[] = {
    {0x8086, "Intel", "aos-iwlwifi", "firmware/iwlwifi-test.fw",
     "upload prepared; hardware upload disabled",
     "first", "weeks for first lab packets",
     "real firmware upload, IRQ, RX/TX rings"},
    {0x10ec, "Realtek", "aos-rtlwifi", "firmware/rtlwifi-test.fw",
     "planned PCI/USB family",
     "second", "weeks per chipset family",
     "chipset split, firmware format, USB support for many adapters"},
    {0x168c, "Qualcomm Atheros", "aos-athwifi", "firmware/athwifi-test.fw",
     "planned PCI family",
     "third", "weeks per chipset family",
     "PCI DMA rings and hardware-specific registers"},
    {0x14c3, "MediaTek", "aos-mtwifi", "firmware/mtwifi-test.fw",
     "planned newer PCI/USB family",
     "third", "weeks per chipset family",
     "modern firmware protocol and USB/PCI variants"},
    {0x14e4, "Broadcom", "aos-b43", "firmware/b43-test.fw",
     "planned legacy/common laptop family",
     "later", "hard; expect longer",
     "closed firmware variants and many incompatible chips"},
    {0x1814, "Ralink", "aos-rt2x00", "firmware/rt2x00-test.fw",
     "planned older USB/PCI family",
     "later", "hard; expect longer",
     "older chip split and USB foundation"},
};

static char num_buf[21];

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
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static char lower_char(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

static int strieq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i]) {
        if (lower_char(a[i]) != lower_char(b[i])) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static void write_buf(const char* s, uint64_t n) {
    syscall3(SYS_WRITE, 1, (long)s, (long)n);
}

static void write_cstr(const char* s) {
    write_buf(s, cstrlen(s));
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

static void write_i64(int64_t v) {
    if (v < 0) {
        write_cstr("-");
        write_u64((uint64_t)-v);
    } else {
        write_u64((uint64_t)v);
    }
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

static void write_mac(const uint8_t mac[6]) {
    for (uint64_t i = 0; i < 6; i++) {
        if (i) write_cstr(":");
        write_hex8(mac[i]);
    }
}

static void write_padded(const char* s, uint64_t width) {
    uint64_t n = cstrlen(s);
    write_cstr(s);
    while (n < width) {
        write_cstr(" ");
        n++;
    }
}

static void write_security(uint8_t security) {
    if (security == MAC80211_SECURITY_OPEN) write_cstr("open");
    else if (security == MAC80211_SECURITY_WPA2) write_cstr("wpa2");
    else if (security == MAC80211_SECURITY_WPA3) write_cstr("wpa3");
    else write_cstr("unknown");
}

static void write_state_name(uint8_t state) {
    if (state == 0) write_cstr("down");
    else if (state == 1) write_cstr("scanning");
    else if (state == 2) write_cstr("scanned");
    else if (state == 3) write_cstr("authenticating");
    else if (state == 4) write_cstr("authenticated");
    else if (state == 5) write_cstr("associating");
    else if (state == 6) write_cstr("associated");
    else write_cstr("unknown");
}

static int firmware_exists(const char* name) {
    struct firmware_info info;

    for (uint64_t i = 0; syscall2(AOS_SYS_FIRMWARE_INFO, (long)i, (long)&info) == 0; i++) {
        if (streq(info.name, name)) return 1;
    }
    return 0;
}

static const struct wifi_driver_target* target_for_vendor(uint16_t vendor) {
    for (uint64_t i = 0; i < sizeof(driver_targets) / sizeof(driver_targets[0]); i++) {
        if (driver_targets[i].vendor == vendor) return &driver_targets[i];
    }
    return 0;
}

static int scan_target_index(const char* target) {
    struct wifi_scan_info scan;

    if (target[0] >= '0' && target[0] <= '9' && target[1] == 0) {
        return target[0] - '0';
    }

    for (uint64_t i = 0; syscall2(AOS_SYS_WIFI_SCAN_INFO, (long)i, (long)&scan) == 0; i++) {
        if (strieq(scan.ssid, target)) return (int)i;
    }
    return -1;
}

static void print_state(void) {
    struct wifi_state_info state;

    if (syscall2(AOS_SYS_WIFI_STATE_INFO, 0, (long)&state) < 0) {
        write_cstr("mac80211 state: unavailable\n");
        return;
    }

    write_cstr("mac80211 state:\n");
    write_cstr("  state=");
    write_state_name(state.state);
    write_cstr(" scan-cache=");
    write_u64(state.scan_count);
    write_cstr(" selected=");
    if (state.selected == 0xff) write_cstr("none");
    else write_u64(state.selected);
    write_cstr("\n");

    write_cstr("  network=");
    write_cstr(state.ssid[0] ? state.ssid : "(none)");
    write_cstr(" bssid=");
    write_mac(state.bssid);
    write_cstr(" ch=");
    write_u64(state.channel);
    write_cstr(" rssi=");
    write_i64(state.rssi_dbm);
    write_cstr("dbm security=");
    write_security(state.security);
    write_cstr("\n");

    write_cstr("  rx: mgmt=");
    write_u64(state.rx_mgmt);
    write_cstr(" beacon=");
    write_u64(state.rx_beacon);
    write_cstr(" probe-resp=");
    write_u64(state.rx_probe_resp);
    write_cstr(" data=");
    write_u64(state.rx_data);
    write_cstr(" other=");
    write_u64(state.rx_other);
    write_cstr("\n");

    write_cstr("  tx: mgmt=");
    write_u64(state.tx_mgmt);
    write_cstr(" probe-req=");
    write_u64(state.tx_probe_req);
    write_cstr(" last-len=");
    write_u64(state.last_tx_len);
    write_cstr("\n");

    write_cstr("  driver-tx: calls=");
    write_u64(state.driver_tx_calls);
    write_cstr(" hw=");
    write_u64(state.driver_tx_hw);
    write_cstr(" sim=");
    write_u64(state.driver_tx_sim);
    write_cstr(" err=");
    write_u64(state.driver_tx_errors);
    write_cstr("\n");

    write_cstr("  driver-rx: calls=");
    write_u64(state.driver_rx_calls);
    write_cstr(" accepted=");
    write_u64(state.driver_rx_accepted);
    write_cstr(" err=");
    write_u64(state.driver_rx_errors);
    write_cstr(" last-len=");
    write_u64(state.driver_rx_last_len);
    write_cstr(" last-rssi=");
    write_i64(state.driver_rx_last_rssi);
    write_cstr("dbm\n");

    write_cstr("  auth-attempts=");
    write_u64(state.auth_attempts);
    write_cstr(" assoc-attempts=");
    write_u64(state.assoc_attempts);
    write_cstr("\n");
}

static void wifi_status(void) {
    struct pci_info pci;
    uint64_t found = 0;

    write_cstr("AOS WiFi\n--------\n");
    for (uint64_t i = 0; syscall2(AOS_SYS_PCI_INFO, (long)i, (long)&pci) == 0; i++) {
        const struct wifi_driver_target* target;

        if (pci.class_code != PCI_CLASS_NETWORK || pci.subclass != PCI_SUBCLASS_WIFI) continue;
        found++;
        target = target_for_vendor(pci.vendor_id);

        write_cstr("wifi: PCI 802.11 controller at ");
        write_hex8(pci.bus);
        write_cstr(":");
        write_hex8(pci.slot);
        write_cstr(".");
        write_hex_nibble(pci.function);
        write_cstr(" vendor:device=");
        write_hex16(pci.vendor_id);
        write_cstr(":");
        write_hex16(pci.device_id);
        write_cstr("\n");
        write_cstr("  target-driver=");
        write_cstr(target ? target->driver : "aos-wifi-pci");
        write_cstr("\n");
    }

    if (!found) {
        write_cstr("wifi: no PCI 802.11 controller detected\n");
        write_cstr("wifi: current QEMU run provides e1000 Ethernet only\n");
    }
    print_state();
}

static void wifi_scan(void) {
    struct wifi_scan_info scan;
    uint64_t count = 0;

    write_cstr("AOS WiFi scan cache\n");
    write_cstr("SSID                BSSID              CH  RSSI  SECURITY\n");
    write_cstr("---------------------------------------------------------\n");
    for (uint64_t i = 0; syscall2(AOS_SYS_WIFI_SCAN_INFO, (long)i, (long)&scan) == 0; i++) {
        write_padded(scan.ssid, 20);
        write_mac(scan.bssid);
        write_cstr("  ");
        write_u64(scan.channel);
        write_cstr("   ");
        write_i64(scan.rssi_dbm);
        write_cstr("dbm  ");
        write_security(scan.security);
        write_cstr("\n");
        count++;
    }
    if (!count) write_cstr("wifi scan: no networks cached\n");
}

static void wifi_drivers(void) {
    struct pci_info pci;
    uint64_t detected = 0;

    write_cstr("AOS WiFi driver targets\n");
    write_cstr("-----------------------\n");
    write_cstr("Shared stack: mac80211 scan/auth/assoc + wlan0 netdev bridge\n\n");

    for (uint64_t i = 0; i < sizeof(driver_targets) / sizeof(driver_targets[0]); i++) {
        const struct wifi_driver_target* t = &driver_targets[i];

        write_padded(t->vendor_name, 18);
        write_cstr(" vendor=");
        write_hex16(t->vendor);
        write_cstr(" driver=");
        write_padded(t->driver, 14);
        write_cstr(" firmware=");
        write_cstr(firmware_exists(t->firmware) ? "found" : "missing");
        write_cstr("\n  ");
        write_cstr(t->firmware);
        write_cstr("\n  stage: ");
        write_cstr(t->stage);
        write_cstr("\n  priority: ");
        write_cstr(t->priority);
        write_cstr("  effort: ");
        write_cstr(t->effort);
        write_cstr("\n  blocker: ");
        write_cstr(t->blocker);
        write_cstr("\n");
    }

    write_cstr("\nDetected WiFi hardware:\n");
    for (uint64_t i = 0; syscall2(AOS_SYS_PCI_INFO, (long)i, (long)&pci) == 0; i++) {
        const struct wifi_driver_target* t;
        if (pci.class_code != PCI_CLASS_NETWORK || pci.subclass != PCI_SUBCLASS_WIFI) continue;
        detected++;
        t = target_for_vendor(pci.vendor_id);
        write_cstr("  ");
        write_hex8(pci.bus);
        write_cstr(":");
        write_hex8(pci.slot);
        write_cstr(".");
        write_hex_nibble(pci.function);
        write_cstr(" ");
        write_hex16(pci.vendor_id);
        write_cstr(":");
        write_hex16(pci.device_id);
        write_cstr(" -> ");
        write_cstr(t ? t->driver : "aos-wifi-pci");
        write_cstr("\n");
    }
    if (!detected) write_cstr("  none in this VM; QEMU is using e1000 Ethernet\n");
}

static void wifi_roadmap(void) {
    write_cstr("AOS WiFi roadmap\n");
    write_cstr("----------------\n");
    write_cstr("1. Keep e1000 Ethernet as the stable internet path.\n");
    write_cstr("2. Finish shared mac80211: scan, auth, assoc, data bridge.\n");
    write_cstr("3. Finish WPA2-PSK before encrypted real networks are useful.\n");
    write_cstr("4. Bring up Intel iwlwifi first because it is common and already staged.\n");
    write_cstr("5. Add Realtek, Atheros, MediaTek after the shared stack is solid.\n");
    write_cstr("6. Add USB WiFi only after USB enumeration and transfers are reliable.\n\n");
    write_cstr("Rough time from current AOS state:\n");
    write_cstr("  Intel first working packets: weeks of focused driver work.\n");
    write_cstr("  One more chipset family: weeks each after Intel teaches the pattern.\n");
    write_cstr("  Many WiFi chips like Linux: months, not days.\n\n");
    write_cstr("Current truth: AOS can model WiFi state, but real WiFi hardware TX/RX is not done yet.\n");
}

static void wifi_probe(void) {
    write_cstr("wifi: sending active probe request through mac80211\n");
    if (syscall2(AOS_SYS_WIFI_CONTROL, 5, 0) < 0) {
        write_cstr("wifi probe: failed\n");
        print_state();
        exit_code(1);
    }
    write_cstr("wifi: probe request built, probe response cached\n");
    print_state();
}

static void wifi_rxprobe(void) {
    write_cstr("wifi: injecting probe response through driver RX hook\n");
    if (syscall2(AOS_SYS_WIFI_CONTROL, 6, 0) < 0) {
        write_cstr("wifi rxprobe: failed\n");
        print_state();
        exit_code(1);
    }
    write_cstr("wifi: RX hook accepted probe response\n");
    print_state();
}

static void wifi_connect(const char* target) {
    int index = target ? scan_target_index(target) : 0;

    if (index < 0) {
        write_cstr("wifi connect: network not found in scan cache\n");
        exit_code(1);
    }

    write_cstr("wifi: sending auth and association frames through mac80211\n");
    if (syscall2(AOS_SYS_WIFI_CONTROL, 4, index) < 0) {
        write_cstr("wifi connect: failed\n");
        print_state();
        exit_code(1);
    }
    write_cstr("wifi: auth and association completed\n");
    print_state();
}

static void usage(void) {
    write_cstr("usage: wifi [scan | drivers | roadmap | connect [INDEX|SSID] | probe | rxprobe]\n");
}

void aos_main(uint64_t argc, char** argv) {
    if (argc == 1) {
        wifi_status();
        exit_code(0);
    }
    if (argc == 2 && streq(argv[1], "scan")) {
        wifi_scan();
        exit_code(0);
    }
    if (argc == 2 && streq(argv[1], "drivers")) {
        wifi_drivers();
        exit_code(0);
    }
    if (argc == 2 && (streq(argv[1], "roadmap") || streq(argv[1], "plan"))) {
        wifi_roadmap();
        exit_code(0);
    }
    if ((argc == 2 || argc == 3) && streq(argv[1], "connect")) {
        wifi_connect(argc == 3 ? argv[2] : 0);
        exit_code(0);
    }
    if (argc == 2 && streq(argv[1], "probe")) {
        wifi_probe();
        exit_code(0);
    }
    if (argc == 2 && streq(argv[1], "rxprobe")) {
        wifi_rxprobe();
        exit_code(0);
    }
    if (argc == 2 && (streq(argv[1], "-h") || streq(argv[1], "--help"))) {
        usage();
        exit_code(0);
    }
    usage();
    exit_code(1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
