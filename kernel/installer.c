/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <aos_install.h>
#include <aosfs.h>
#include <blkdev.h>
#include <credentials.h>
#include <installer.h>
#include <partition.h>
#include <vfs.h>

#define INSTALL_COPY_CHUNK (256U * 1024U)
#define INSTALL_KERNEL_PHASE_END (4U * 1024U * 1024U)
#define INSTALL_MBR_PARTITION_OFFSET 446U
#define INSTALL_ROOT_MBR_TYPE 0x7FU

extern void serial_print(const char* s);

static const uint8_t* g_boot_payload;
static uint32_t g_boot_payload_size;
static const uint8_t* g_bios_payload;
static uint32_t g_bios_payload_size;
static struct aos_install_config g_config;
static struct aos_install_status g_status;
static uint32_t g_copy_offset;

static void mem_zero(void* dst, size_t n) {
    uint8_t* out = (uint8_t*)dst;
    while (n--) *out++ = 0;
}

static void mem_copy(void* dst, const void* src, size_t n) {
    uint8_t* out = (uint8_t*)dst;
    const uint8_t* in = (const uint8_t*)src;
    while (n--) *out++ = *in++;
}

static size_t str_len(const char* s) {
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static int str_equal(const char* a, const char* b) {
    size_t i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

static void copy_text(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    while (src[i] && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void append_text(char* dst, size_t dst_size, const char* src) {
    size_t at = str_len(dst);
    size_t i = 0;
    if (!src || at >= dst_size) return;
    while (src[i] && at + 1 < dst_size) dst[at++] = src[i++];
    dst[at] = 0;
}

static void append_u32(char* dst, size_t dst_size, uint32_t value) {
    char digits[11];
    size_t at = sizeof(digits);
    digits[--at] = 0;
    if (value == 0) {
        append_text(dst, dst_size, "0");
        return;
    }
    while (value && at > 0) {
        digits[--at] = (char)('0' + value % 10U);
        value /= 10U;
    }
    append_text(dst, dst_size, &digits[at]);
}

static void put_le32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static int installed_marker_present(void) {
    struct vfs_node node;
    return aosfs_lookup_path("etc/aos-install.conf", &node) == 0;
}

static const struct blkdev* recommended_disk(void) {
    for (size_t i = 0; i < BLKDEV_MAX; i++) {
        const struct blkdev* dev = blkdev_get_index(i);
        if (!dev) break;
        if (dev->has_ops && !dev->read_only) return dev;
    }
    return NULL;
}

static int payload_ready(void) {
    return g_boot_payload && g_boot_payload_size == AOS_INSTALL_BOOT_SIZE &&
           g_bios_payload && g_bios_payload_size >= 1024U &&
           g_bios_payload_size <= AOS_INSTALL_BOOT_OFFSET;
}

static void set_message(const char* message) {
    copy_text(g_status.message, sizeof(g_status.message), message);
}

static int fail_install(uint32_t error, const char* message) {
    g_status.running = 0;
    g_status.step = AOS_INSTALL_STEP_ERROR;
    g_status.error = error;
    set_message(message);
    serial_print("Arootinstall: failed: ");
    serial_print(message);
    serial_print("\n");
    return -1;
}

static int valid_username(const char* value) {
    size_t n = str_len(value);
    if (n == 0 || n >= sizeof(g_config.username)) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
    }
    return 1;
}

static int valid_hostname(const char* value) {
    size_t n = str_len(value);
    if (n == 0 || n >= sizeof(g_config.hostname) || value[0] == '-' || value[n - 1] == '-') return 0;
    for (size_t i = 0; i < n; i++) {
        char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return 0;
    }
    return 1;
}

static void terminate_config_strings(void) {
    g_config.username[sizeof(g_config.username) - 1] = 0;
    g_config.password[sizeof(g_config.password) - 1] = 0;
    g_config.password_confirm[sizeof(g_config.password_confirm) - 1] = 0;
    g_config.hostname[sizeof(g_config.hostname) - 1] = 0;
    g_config.language[sizeof(g_config.language) - 1] = 0;
    g_config.region[sizeof(g_config.region) - 1] = 0;
    g_config.timezone[sizeof(g_config.timezone) - 1] = 0;
    g_config.keyboard[sizeof(g_config.keyboard) - 1] = 0;
    g_config.wifi_ssid[sizeof(g_config.wifi_ssid) - 1] = 0;
    g_config.wifi_password[sizeof(g_config.wifi_password) - 1] = 0;
    g_config.wifi_ip[sizeof(g_config.wifi_ip) - 1] = 0;
    g_config.wifi_gateway[sizeof(g_config.wifi_gateway) - 1] = 0;
    g_config.wifi_dns[sizeof(g_config.wifi_dns) - 1] = 0;
    g_config.confirmation[sizeof(g_config.confirmation) - 1] = 0;
}

static int write_file(const char* path, const char* data) {
    struct vfs_node node;
    uint64_t written = 0;
    uint32_t new_size = 0;
    size_t len = str_len(data);

    if (aosfs_lookup_path(path, &node) == 0) {
        if (node.type != VFS_NODE_TYPE_REGULAR || aosfs_truncate_path(path) != 0) return -1;
    } else if (aosfs_create_path(path, &node) != 0) {
        return -1;
    }
    if (len == 0) return 0;
    return aosfs_write_path(path, 0, (const uint8_t*)data, len, &written, &new_size) == 0 && written == len ? 0 : -1;
}

static int ensure_dir(const char* path) {
    struct vfs_node node;
    if (aosfs_lookup_path(path, &node) == 0) return node.type == VFS_NODE_TYPE_DIRECTORY ? 0 : -1;
    return aosfs_mkdir_path(path);
}

static int write_system_configuration(void) {
    char config[4096];
    char path[128];
    char passwd[512];
    char shadow[256];
    char group[256];
    char sudoers[256];
    char wifi[768];
    char encoded_password[AOS_PASSWORD_ENCODED_MAX];
    uint8_t password_salt[AOS_PASSWORD_SALT_SIZE];
    const char* home_children[] = {
        "Desktop", "Documents", "Downloads", "Pictures", "Music",
        "Projects", "Starred", "Trash"
    };

    config[0] = 0;
    append_text(config, sizeof(config), "version=1\ninstalled=yes\nusername=");
    append_text(config, sizeof(config), g_config.username);
    append_text(config, sizeof(config), "\nhostname=");
    append_text(config, sizeof(config), g_config.hostname);
    append_text(config, sizeof(config), "\nlanguage=");
    append_text(config, sizeof(config), g_config.language);
    append_text(config, sizeof(config), "\nregion=");
    append_text(config, sizeof(config), g_config.region);
    append_text(config, sizeof(config), "\ntimezone=");
    append_text(config, sizeof(config), g_config.timezone);
    append_text(config, sizeof(config), "\nkeyboard=");
    append_text(config, sizeof(config), g_config.keyboard);
    append_text(config, sizeof(config), "\naccount=");
    append_text(config, sizeof(config), g_config.account_admin ? "administrator" : "standard");
    append_text(config, sizeof(config), "\nautologin=");
    append_text(config, sizeof(config), g_config.auto_login ? "yes" : "no");
    append_text(config, sizeof(config), "\napp_profile=");
    append_u32(config, sizeof(config), g_config.app_profile);
    append_text(config, sizeof(config), "\napp_mask=");
    append_u32(config, sizeof(config), g_config.app_mask);
    append_text(config, sizeof(config), "\ndriver_mode=");
    append_u32(config, sizeof(config), g_config.driver_mode);
    append_text(config, sizeof(config), "\ndriver_scan_mask=");
    append_u32(config, sizeof(config), g_config.driver_scan_mask);
    append_text(config, sizeof(config), "\nbootloader=grub-bios-uefi\n");

    if (write_file("etc/aos-system.conf", config) != 0 ||
        write_file("etc/hostname", g_config.hostname) != 0 ||
        write_file("etc/locale", g_config.language) != 0 ||
        write_file("etc/timezone", g_config.timezone) != 0 ||
        write_file("etc/keyboard", g_config.keyboard) != 0) return -1;

    passwd[0] = 0;
    append_text(passwd, sizeof(passwd), "root:x:0:0:AOS Root:/root:/commands/vash\n");
    append_text(passwd, sizeof(passwd), g_config.username);
    append_text(passwd, sizeof(passwd), ":x:1000:1000:AOS User:/main:/commands/vash\n");

    credentials_generate_salt(password_salt, g_config.username,
                              str_len(g_config.username));
    if (credentials_format_password_hash(
            g_config.password, password_salt, AOS_PASSWORD_ITERATIONS,
            encoded_password, sizeof(encoded_password)) != 0) {
        credentials_wipe(password_salt, sizeof(password_salt));
        return -1;
    }

    shadow[0] = 0;
    append_text(shadow, sizeof(shadow), "root::0:0:99999:7:::\n");
    append_text(shadow, sizeof(shadow), g_config.username);
    append_text(shadow, sizeof(shadow), ":");
    append_text(shadow, sizeof(shadow), encoded_password);
    append_text(shadow, sizeof(shadow), ":0:0:99999:7:::\n");
    credentials_wipe(password_salt, sizeof(password_salt));
    credentials_wipe(encoded_password, sizeof(encoded_password));

    group[0] = 0;
    append_text(group, sizeof(group), "root:x:0:root\nusers:x:1000:");
    append_text(group, sizeof(group), g_config.username);
    append_text(group, sizeof(group), "\n");
    if (g_config.account_admin) {
        append_text(group, sizeof(group), "wheel:x:10:");
        append_text(group, sizeof(group), g_config.username);
        append_text(group, sizeof(group), "\n");
    }

    sudoers[0] = 0;
    append_text(sudoers, sizeof(sudoers), "root ALL=(ALL) NOPASSWD: ALL\n");
    if (g_config.account_admin) {
        append_text(sudoers, sizeof(sudoers), g_config.username);
        append_text(sudoers, sizeof(sudoers), " ALL=(ALL) ALL\n");
    }
    if (write_file("etc/passwd", passwd) != 0 || write_file("etc/shadow", shadow) != 0 ||
        write_file("etc/group", group) != 0 || write_file("etc/sudoers", sudoers) != 0) return -1;

    wifi[0] = 0;
    append_text(wifi, sizeof(wifi), "enabled=");
    append_text(wifi, sizeof(wifi), g_config.wifi_enabled ? "yes" : "no");
    append_text(wifi, sizeof(wifi), "\nssid=");
    append_text(wifi, sizeof(wifi), g_config.wifi_ssid);
    append_text(wifi, sizeof(wifi), "\npassword=");
    append_text(wifi, sizeof(wifi), g_config.wifi_password);
    append_text(wifi, sizeof(wifi), "\nmode=");
    append_text(wifi, sizeof(wifi), g_config.wifi_static ? "static" : "dhcp");
    append_text(wifi, sizeof(wifi), "\nip=");
    append_text(wifi, sizeof(wifi), g_config.wifi_ip);
    append_text(wifi, sizeof(wifi), "\ngateway=");
    append_text(wifi, sizeof(wifi), g_config.wifi_gateway);
    append_text(wifi, sizeof(wifi), "\ndns=");
    append_text(wifi, sizeof(wifi), g_config.wifi_dns);
    append_text(wifi, sizeof(wifi), "\n");
    if (write_file("etc/wifi.conf", wifi) != 0) return -1;

    path[0] = 0;
    append_text(path, sizeof(path), "home/");
    append_text(path, sizeof(path), g_config.username);
    if (ensure_dir(path) != 0) return -1;
    for (size_t i = 0; i < sizeof(home_children) / sizeof(home_children[0]); i++) {
        char child[128];
        child[0] = 0;
        append_text(child, sizeof(child), path);
        append_text(child, sizeof(child), "/");
        append_text(child, sizeof(child), home_children[i]);
        if (ensure_dir(child) != 0) return -1;
    }
    return 0;
}

static int write_install_log(void) {
    char log[2048];
    log[0] = 0;
    append_text(log, sizeof(log), "AOS installation completed\nTarget: ");
    append_text(log, sizeof(log), g_status.target_name);
    append_text(log, sizeof(log), "\nUser: ");
    append_text(log, sizeof(log), g_config.username);
    append_text(log, sizeof(log), "\nHostname: ");
    append_text(log, sizeof(log), g_config.hostname);
    append_text(log, sizeof(log), "\nLanguage: ");
    append_text(log, sizeof(log), g_config.language);
    append_text(log, sizeof(log), "\nRegion: ");
    append_text(log, sizeof(log), g_config.region);
    append_text(log, sizeof(log), "\nTimezone: ");
    append_text(log, sizeof(log), g_config.timezone);
    append_text(log, sizeof(log), "\nKeyboard: ");
    append_text(log, sizeof(log), g_config.keyboard);
    append_text(log, sizeof(log), "\nBoot: GRUB BIOS + UEFI\n");
    if (write_file("logs/install.log", log) != 0) return -1;
    return write_file("etc/aos-install.conf", "version=1\ninstalled=yes\nstate=complete\n");
}

static int write_partition_mbr(const struct blkdev* dev) {
    uint8_t mbr[512];
    uint8_t* boot_entry = mbr + INSTALL_MBR_PARTITION_OFFSET;
    uint8_t* root_entry = boot_entry + 16;
    uint64_t sectors = dev->size / 512ULL;
    uint64_t root_start = AOS_INSTALL_ROOT_OFFSET / 512ULL;
    uint64_t root_sectors = sectors > root_start ? sectors - root_start : 0;

    if (g_bios_payload_size < sizeof(mbr) || root_sectors == 0) return -1;
    mem_copy(mbr, g_bios_payload, sizeof(mbr));
    mem_zero(boot_entry, 64);

    boot_entry[0] = 0x80;
    boot_entry[1] = 0xFE;
    boot_entry[2] = 0xFF;
    boot_entry[3] = 0xFF;
    boot_entry[4] = 0xEF;
    boot_entry[5] = 0xFE;
    boot_entry[6] = 0xFF;
    boot_entry[7] = 0xFF;
    put_le32(boot_entry + 8, (uint32_t)(AOS_INSTALL_BOOT_OFFSET / 512ULL));
    put_le32(boot_entry + 12, (uint32_t)(AOS_INSTALL_BOOT_SIZE / 512ULL));

    root_entry[0] = 0;
    root_entry[1] = 0xFE;
    root_entry[2] = 0xFF;
    root_entry[3] = 0xFF;
    root_entry[4] = INSTALL_ROOT_MBR_TYPE;
    root_entry[5] = 0xFE;
    root_entry[6] = 0xFF;
    root_entry[7] = 0xFF;
    put_le32(root_entry + 8, (uint32_t)root_start);
    put_le32(root_entry + 12, root_sectors > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (uint32_t)root_sectors);
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    return blkdev_write(dev->id, 0, mbr, sizeof(mbr));
}

static int copy_boot_chunk(const struct blkdev* dev, uint32_t limit) {
    uint32_t remaining;
    uint32_t count;
    if (g_copy_offset >= limit) return 1;
    remaining = limit - g_copy_offset;
    count = remaining > INSTALL_COPY_CHUNK ? INSTALL_COPY_CHUNK : remaining;
    if (blkdev_write(dev->id, AOS_INSTALL_BOOT_OFFSET + g_copy_offset,
                     g_boot_payload + g_copy_offset, count) != 0) return -1;
    g_copy_offset += count;
    g_status.bytes_done = g_copy_offset;
    return g_copy_offset >= limit ? 1 : 0;
}

static int verify_install(const struct blkdev* dev) {
    uint8_t check[512];
    struct vfs_node marker;
    if (blkdev_read(dev->id, 0, check, sizeof(check)) != 0 ||
        check[510] != 0x55 || check[511] != 0xAA || check[450] != 0xEF ||
        check[466] != INSTALL_ROOT_MBR_TYPE) return -1;
    if (blkdev_read(dev->id, AOS_INSTALL_BOOT_OFFSET, check, sizeof(check)) != 0 ||
        check[510] != 0x55 || check[511] != 0xAA) return -1;
    mem_zero(check, sizeof(check));
    if (blkdev_read(dev->id, AOS_INSTALL_ROOT_OFFSET, check, sizeof(check)) != 0 ||
        check[0] != 'A' || check[1] != 'O' || check[2] != 'S' ||
        check[3] != 'F' || check[4] != 'S' || check[5] != '1') return -1;
    return aosfs_lookup_path("etc/aos-install.conf", &marker) == 0 ? 0 : -1;
}

void installer_init(void) {
    mem_zero(&g_config, sizeof(g_config));
    mem_zero(&g_status, sizeof(g_status));
    g_status.version = AOS_INSTALL_ABI_VERSION;
    g_status.step = AOS_INSTALL_STEP_IDLE;
    set_message("Ready to inspect live installation media");
    g_boot_payload = NULL;
    g_boot_payload_size = 0;
    g_bios_payload = NULL;
    g_bios_payload_size = 0;
    g_copy_offset = 0;
}

int installer_set_payload(uint32_t kind, uint32_t start, uint32_t end) {
    if (start >= end) return -1;
    if (kind == INSTALLER_PAYLOAD_BOOT) {
        g_boot_payload = (const uint8_t*)(uintptr_t)start;
        g_boot_payload_size = end - start;
        return 0;
    }
    if (kind == INSTALLER_PAYLOAD_BIOS) {
        g_bios_payload = (const uint8_t*)(uintptr_t)start;
        g_bios_payload_size = end - start;
        return 0;
    }
    return -1;
}

int installer_get_status(struct aos_install_status* out) {
    const struct blkdev* dev;
    if (!out) return -1;
    g_status.version = AOS_INSTALL_ABI_VERSION;
    g_status.payload_ready = payload_ready() ? 1U : 0U;
    if (!g_status.running && g_status.step != AOS_INSTALL_STEP_COMPLETE &&
        g_status.step != AOS_INSTALL_STEP_ERROR) {
        g_status.live_mode = installed_marker_present() ? 0U : 1U;
    }
    if (!g_status.running && g_status.target_size == 0) {
        dev = recommended_disk();
        if (dev) {
            g_status.target_blkdev_id = dev->id;
            g_status.target_size = dev->size;
            copy_text(g_status.target_name, sizeof(g_status.target_name), dev->name);
        }
    }
    mem_copy(out, &g_status, sizeof(*out));
    return 0;
}

int installer_begin(const struct aos_install_config* config) {
    const struct blkdev* dev;
    if (!config) return fail_install(AOS_INSTALL_ERROR_BAD_CONFIG, "Installer configuration is missing");
    if (g_status.running) return -1;
    if (installed_marker_present()) return fail_install(AOS_INSTALL_ERROR_NOT_LIVE, "Install AOS is available from live mode only");
    if (!payload_ready()) return fail_install(AOS_INSTALL_ERROR_PAYLOAD_MISSING, "Live installation payload is incomplete");

    mem_copy(&g_config, config, sizeof(g_config));
    terminate_config_strings();
    if (g_config.version != AOS_INSTALL_ABI_VERSION || !valid_username(g_config.username) ||
        !valid_hostname(g_config.hostname) || !str_equal(g_config.password, g_config.password_confirm) ||
        str_len(g_config.password) < 8U ||
        str_len(g_config.language) == 0 || str_len(g_config.region) == 0 ||
        str_len(g_config.timezone) == 0 || str_len(g_config.keyboard) == 0) {
        return fail_install(AOS_INSTALL_ERROR_BAD_CONFIG, "Check the account, hostname, locale, and password fields");
    }
    if (!str_equal(g_config.confirmation, "AOS")) {
        return fail_install(AOS_INSTALL_ERROR_BAD_CONFIRMATION, "Type AOS exactly to confirm disk erasure");
    }
    if (g_config.partition_mode != AOS_INSTALL_PARTITION_AUTO) {
        return fail_install(AOS_INSTALL_ERROR_MANUAL_LAYOUT, "Manual layouts must provide a FAT boot partition and AOSFS root");
    }
    dev = blkdev_get(g_config.target_blkdev_id);
    if (!dev || !dev->has_ops) return fail_install(AOS_INSTALL_ERROR_NO_DISK, "The selected hardware disk is unavailable");
    if (dev->read_only) return fail_install(AOS_INSTALL_ERROR_READ_ONLY, "The selected disk is read-only");
    if (dev->size < AOS_INSTALL_MIN_DISK_SIZE) return fail_install(AOS_INSTALL_ERROR_DISK_TOO_SMALL, "The selected disk must be at least 32 MiB");

    mem_zero(&g_status, sizeof(g_status));
    g_status.version = AOS_INSTALL_ABI_VERSION;
    g_status.live_mode = 1;
    g_status.payload_ready = 1;
    g_status.running = 1;
    g_status.step = AOS_INSTALL_STEP_PREPARING;
    g_status.progress_percent = 2;
    g_status.target_blkdev_id = dev->id;
    g_status.target_size = dev->size;
    g_status.bytes_total = AOS_INSTALL_BOOT_SIZE + g_bios_payload_size;
    copy_text(g_status.target_name, sizeof(g_status.target_name), dev->name);
    set_message("Preparing the live installer");
    g_copy_offset = 0;
    serial_print("Arootinstall: installation transaction started\n");
    return 0;
}

int installer_advance(void) {
    const struct blkdev* dev = blkdev_get(g_status.target_blkdev_id);
    int rc;
    if (!g_status.running) return g_status.step == AOS_INSTALL_STEP_COMPLETE ? 1 : -1;
    if (!dev) return fail_install(AOS_INSTALL_ERROR_NO_DISK, "The target disk disappeared");

    switch (g_status.step) {
        case AOS_INSTALL_STEP_PREPARING:
            if (g_bios_payload[510] != 0x55 || g_bios_payload[511] != 0xAA ||
                g_boot_payload[510] != 0x55 || g_boot_payload[511] != 0xAA) {
                return fail_install(AOS_INSTALL_ERROR_PAYLOAD_MISSING, "The GRUB or FAT payload is invalid");
            }
            g_status.step = AOS_INSTALL_STEP_CHECKING_DISK;
            g_status.progress_percent = 5;
            set_message("Checking the selected target disk");
            return 0;
        case AOS_INSTALL_STEP_CHECKING_DISK: {
            uint8_t sector[512];
            if (blkdev_read(dev->id, 0, sector, sizeof(sector)) != 0) {
                return fail_install(AOS_INSTALL_ERROR_IO, "Could not read the selected target disk");
            }
            g_status.step = AOS_INSTALL_STEP_PARTITIONING;
            g_status.progress_percent = 8;
            set_message("Creating BIOS, EFI, and AOSFS partitions");
            return 0;
        }
        case AOS_INSTALL_STEP_PARTITIONING:
            if (write_partition_mbr(dev) != 0 ||
                partition_create_installed_layout(dev->id, AOS_INSTALL_BOOT_OFFSET,
                                                  AOS_INSTALL_BOOT_SIZE,
                                                  AOS_INSTALL_ROOT_OFFSET) != 0) {
                return fail_install(AOS_INSTALL_ERROR_IO, "Could not create the disk layout");
            }
            g_status.step = AOS_INSTALL_STEP_FORMATTING;
            g_status.progress_percent = 12;
            set_message("Formatting the AOSFS system partition");
            return 0;
        case AOS_INSTALL_STEP_FORMATTING:
            if (aosfs_format_role(PARTITION_ROLE_ROOT, dev->id, AOS_INSTALL_ROOT_OFFSET) != 0) {
                return fail_install(AOS_INSTALL_ERROR_FILESYSTEM, "Could not format the AOSFS system partition");
            }
            g_status.step = AOS_INSTALL_STEP_COPYING_KERNEL;
            g_status.progress_percent = 18;
            set_message("Copying the EFI loader and kernel");
            return 0;
        case AOS_INSTALL_STEP_COPYING_KERNEL:
            rc = copy_boot_chunk(dev, INSTALL_KERNEL_PHASE_END);
            if (rc < 0) return fail_install(AOS_INSTALL_ERROR_IO, "Could not copy the EFI loader and kernel");
            g_status.progress_percent = 18U + (g_copy_offset * 17U / INSTALL_KERNEL_PHASE_END);
            if (rc > 0) {
                g_status.step = AOS_INSTALL_STEP_COPYING_INITRD;
                g_status.progress_percent = 35;
                set_message("Copying the initrd and bundled userspace");
            }
            return 0;
        case AOS_INSTALL_STEP_COPYING_INITRD:
            rc = copy_boot_chunk(dev, (uint32_t)AOS_INSTALL_BOOT_SIZE);
            if (rc < 0) return fail_install(AOS_INSTALL_ERROR_IO, "Could not copy the initrd and userspace");
            g_status.progress_percent = 35U + ((g_copy_offset - INSTALL_KERNEL_PHASE_END) * 35U /
                                                ((uint32_t)AOS_INSTALL_BOOT_SIZE - INSTALL_KERNEL_PHASE_END));
            if (rc > 0) {
                g_status.step = AOS_INSTALL_STEP_COPYING_USERSPACE;
                g_status.progress_percent = 70;
                set_message("Registering the selected application profile");
            }
            return 0;
        case AOS_INSTALL_STEP_COPYING_USERSPACE:
            g_status.step = AOS_INSTALL_STEP_WRITING_CONFIG;
            g_status.progress_percent = 74;
            set_message("Writing account and system configuration");
            return 0;
        case AOS_INSTALL_STEP_WRITING_CONFIG:
            if (write_system_configuration() != 0) {
                return fail_install(AOS_INSTALL_ERROR_FILESYSTEM, "Could not write the installed system configuration");
            }
            g_status.step = AOS_INSTALL_STEP_INSTALLING_BOOTLOADER;
            g_status.progress_percent = 82;
            set_message("Installing GRUB for legacy BIOS and UEFI");
            return 0;
        case AOS_INSTALL_STEP_INSTALLING_BOOTLOADER:
            if (blkdev_write(dev->id, 512, g_bios_payload + 512,
                             g_bios_payload_size - 512U) != 0) {
                return fail_install(AOS_INSTALL_ERROR_IO, "Could not install the GRUB BIOS core image");
            }
            g_status.bytes_done = AOS_INSTALL_BOOT_SIZE + g_bios_payload_size;
            g_status.step = AOS_INSTALL_STEP_CREATING_LOG;
            g_status.progress_percent = 91;
            set_message("Creating the installation log");
            return 0;
        case AOS_INSTALL_STEP_CREATING_LOG:
            if (write_install_log() != 0) {
                return fail_install(AOS_INSTALL_ERROR_FILESYSTEM, "Could not create the installation log");
            }
            g_status.step = AOS_INSTALL_STEP_FINISHING;
            g_status.progress_percent = 96;
            set_message("Verifying the installed system");
            return 0;
        case AOS_INSTALL_STEP_FINISHING:
            if (verify_install(dev) != 0) {
                return fail_install(AOS_INSTALL_ERROR_VERIFY, "Installed system verification failed");
            }
            mem_zero(g_config.password, sizeof(g_config.password));
            mem_zero(g_config.password_confirm, sizeof(g_config.password_confirm));
            mem_zero(g_config.wifi_password, sizeof(g_config.wifi_password));
            mem_zero(g_config.confirmation, sizeof(g_config.confirmation));
            g_status.running = 0;
            g_status.live_mode = 0;
            g_status.step = AOS_INSTALL_STEP_COMPLETE;
            g_status.progress_percent = 100;
            g_status.error = AOS_INSTALL_ERROR_NONE;
            set_message("AOS is installed. Remove live media before rebooting");
            serial_print("Arootinstall: installation complete\n");
            return 1;
        default:
            return fail_install(AOS_INSTALL_ERROR_IO, "Installer entered an invalid state");
    }
}

int installer_cancel(void) {
    if (!g_status.running) return 0;
    if (g_status.step >= AOS_INSTALL_STEP_PARTITIONING) {
        return -1;
    }
    mem_zero(&g_config, sizeof(g_config));
    g_status.running = 0;
    g_status.step = AOS_INSTALL_STEP_IDLE;
    g_status.progress_percent = 0;
    g_status.error = AOS_INSTALL_ERROR_CANCELLED;
    set_message("Installation cancelled before the disk was changed");
    return 0;
}
