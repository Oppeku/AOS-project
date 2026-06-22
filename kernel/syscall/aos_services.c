#include "syscall_internal.h"

struct aos_partition_user {
    uint64_t size;
    uint64_t offset;
    uint32_t start;
    uint32_t end;
    uint16_t index;
    uint8_t fs_type;
    uint8_t role;
    uint8_t flags;
    uint8_t reserved;
    uint32_t blkdev_id;
    char name[16];
    char fs_name[16];
};

int64_t sys_partition_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_partition_user* out = (struct aos_partition_user*)(uintptr_t)regs->rsi;
    const struct partition* part;
    const char* fs_name;

    if (!out) return -(int64_t)LINUX_EFAULT;

    part = partition_get((size_t)index);
    if (!part) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    out->size = part->size;
    out->offset = part->offset;
    out->start = part->start;
    out->end = part->end;
    out->index = part->index;
    out->fs_type = part->fs_type;
    out->role = part->role;
    out->blkdev_id = part->blkdev_id;
    if (!part->start && !part->end) {
        out->flags = 1;
    }

    for (size_t i = 0; i + 1 < sizeof(out->name) && part->name[i]; i++) {
        out->name[i] = part->name[i];
    }
    fs_name = partition_fs_name(part->fs_type);
    for (size_t i = 0; i + 1 < sizeof(out->fs_name) && fs_name[i]; i++) {
        out->fs_name[i] = fs_name[i];
    }

    return 0;
}

struct aos_blkdev_user {
    uint32_t id;
    uint32_t block_size;
    uint64_t size;
    uint8_t read_only;
    uint8_t has_ops;
    uint8_t reserved[6];
    char name[16];
};

struct aos_mem_info_user {
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

struct aos_uptime_info_user {
    uint64_t ticks;
    uint64_t seconds;
    uint32_t frequency;
    uint32_t reserved;
};

struct aos_display_info_user {
    uint32_t cols;
    uint32_t rows;
    uint32_t detected_cols;
    uint32_t detected_rows;
    uint32_t max_cols;
    uint32_t max_rows;
};

struct aos_gfx_info_user {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t ready;
};

struct aos_input_event_user {
    uint32_t key;
    uint32_t flags;
    uint32_t ascii;
    uint32_t source;
};

struct aos_time_info_user {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t weekday;
    uint8_t reserved;
};

struct aos_user_info_user {
    uint32_t uid;
    uint32_t gid;
    uint32_t euid;
    uint32_t egid;
    char username[32];
    char home[256];
};

struct aos_pci_info_user {
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
    uint8_t reserved[3];
    uint32_t bar[6];
} __attribute__((packed));

struct aos_driver_info_user {
    uint8_t type;
    uint8_t claimed;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t irq_line;
    uint8_t reserved[7];
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar[6];
    char driver[32];
    char status[64];
} __attribute__((packed));

struct aos_netdev_info_user {
    uint8_t type;
    uint8_t link_up;
    uint8_t mac[6];
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t reserved[5];
    char name[16];
    char driver[32];
    char status[64];
    uint8_t ipv4_addr[4];
    uint8_t ipv4_gateway[4];
    uint8_t ipv4_dns[4];
    uint8_t ipv4_prefix;
    uint8_t ipv4_configured;
    uint8_t ipv6_configured;
    uint8_t ipv6_prefix;
    uint8_t reserved2[2];
    uint8_t ipv6_addr[16];
    uint8_t ipv6_gateway[16];
    uint8_t ipv6_dns[16];
    uint8_t reserved3[12];
} __attribute__((packed));

struct aos_netdev_stats_user {
    uint64_t tx_packets;
    uint64_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint64_t tx_errors;
    uint64_t rx_errors;
    uint64_t tx_dropped;
    uint64_t rx_dropped;
    uint64_t tx_arp;
    uint64_t rx_arp;
    uint64_t tx_ipv4;
    uint64_t rx_ipv4;
    uint64_t tx_ipv6;
    uint64_t rx_ipv6;
    uint64_t tx_icmp;
    uint64_t rx_icmp;
    uint64_t tx_udp;
    uint64_t rx_udp;
    uint64_t tx_tcp;
    uint64_t rx_tcp;
} __attribute__((packed));

struct aos_arp_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv4[4];
    uint8_t mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
} __attribute__((packed));

struct aos_ndp_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv6[16];
    uint8_t mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
} __attribute__((packed));

struct aos_dns_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t family;
    uint8_t reserved;
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv4[4];
    uint8_t reserved2[4];
    uint8_t ipv6[16];
    char dev_name[16];
    char name[128];
} __attribute__((packed));

struct aos_socket_info_user {
    uint8_t valid;
    uint8_t index;
    uint8_t state;
    uint8_t family;
    uint8_t type;
    uint8_t protocol;
    uint8_t dev_index;
    uint8_t reserved;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t refcount;
    uint32_t rx_len;
    uint32_t rx_off;
    uint32_t seq;
    uint32_t ack;
    uint16_t peer_window;
    uint16_t peer_mss;
    uint32_t peer_window_bytes;
    uint32_t retransmits;
    uint32_t cwnd_bytes;
    uint32_t ssthresh_bytes;
    uint8_t local_window_scale;
    uint8_t peer_window_scale;
    uint16_t reserved4;
    uint8_t remote_ip[4];
    uint8_t next_hop_ip[4];
    uint8_t remote_ip6[16];
    uint8_t next_hop_ip6[16];
    uint8_t remote_mac[6];
    uint8_t reserved2[2];
    char dev_name[16];
} __attribute__((packed));

struct aos_firmware_info_user {
    char name[96];
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed));

struct aos_wifi_scan_info_user {
    char ssid[32];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi_dbm;
    uint8_t security;
    uint8_t reserved[7];
} __attribute__((packed));

struct aos_wifi_state_info_user {
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

static void copy_cstr_bounded(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int64_t sys_blkdev_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_blkdev_user* out = (struct aos_blkdev_user*)(uintptr_t)regs->rsi;
    const struct blkdev* dev;

    if (!out) return -(int64_t)LINUX_EFAULT;

    dev = blkdev_get_index((size_t)index);
    if (!dev) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    out->id = dev->id;
    out->block_size = dev->block_size;
    out->size = dev->size;
    out->read_only = dev->read_only;
    out->has_ops = dev->has_ops;
    for (size_t i = 0; i + 1 < sizeof(out->name) && dev->name[i]; i++) {
        out->name[i] = dev->name[i];
    }
    return 0;
}

int64_t sys_mem_info(struct syscall_regs* regs) {
    struct aos_mem_info_user* out = (struct aos_mem_info_user*)(uintptr_t)regs->rdi;

    if (!out) return -(int64_t)LINUX_EFAULT;

    out->total = pmm_total_memory();
    out->free = pmm_free_memory();
    out->used = pmm_used_memory();
    return 0;
}

int64_t sys_uptime_info(struct syscall_regs* regs) {
    struct aos_uptime_info_user* out = (struct aos_uptime_info_user*)(uintptr_t)regs->rdi;
    uint32_t frequency = timer_get_frequency();
    uint64_t ticks = timer_get_ticks();

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (frequency == 0) frequency = 100;

    out->ticks = ticks;
    out->seconds = ticks / frequency;
    out->frequency = frequency;
    out->reserved = 0;
    return 0;
}

int64_t sys_display_info(struct syscall_regs* regs) {
    struct aos_display_info_user* out = (struct aos_display_info_user*)(uintptr_t)regs->rdi;
    unsigned int cols = 0;
    unsigned int rows = 0;
    unsigned int detected_cols = 0;
    unsigned int detected_rows = 0;
    unsigned int max_cols = 0;
    unsigned int max_rows = 0;

    if (!out) return -(int64_t)LINUX_EFAULT;

    vga_get_display_mode(&cols, &rows, &detected_cols, &detected_rows, &max_cols, &max_rows);
    out->cols = cols;
    out->rows = rows;
    out->detected_cols = detected_cols;
    out->detected_rows = detected_rows;
    out->max_cols = max_cols;
    out->max_rows = max_rows;
    return 0;
}

int64_t sys_display_set(struct syscall_regs* regs) {
    uint32_t cols = (uint32_t)regs->rdi;
    uint32_t rows = (uint32_t)regs->rsi;

    if (cols == 0 && rows == 0) {
        vga_auto_display_mode();
        return 0;
    }

    if (vga_set_display_mode(cols, rows) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    return 0;
}

int64_t sys_gfx_info(struct syscall_regs* regs) {
    struct aos_gfx_info_user* out = (struct aos_gfx_info_user*)(uintptr_t)regs->rdi;

    if (!out) return -(int64_t)LINUX_EFAULT;

    out->width = gfx_width();
    out->height = gfx_height();
    out->bpp = gfx_bpp();
    out->ready = gfx_is_ready() ? 1U : 0U;
    return 0;
}

int64_t sys_gfx_clear(struct syscall_regs* regs) {
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_clear((uint32_t)regs->rdi);
    return 0;
}

int64_t sys_gfx_pixel(struct syscall_regs* regs) {
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_putpixel((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx);
    return 0;
}

int64_t sys_gfx_rect(struct syscall_regs* regs) {
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_fill_rect((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx,
                  (uint32_t)regs->r10, (uint32_t)regs->r8);
    return 0;
}

int64_t sys_gfx_present(struct syscall_regs* regs) {
    (void)regs;
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_present();
    return 0;
}

int64_t sys_input_poll(struct syscall_regs* regs) {
    struct aos_input_event_user* out = (struct aos_input_event_user*)(uintptr_t)regs->rdi;
    struct aos_input_event event;

    if (!out) return -(int64_t)LINUX_EFAULT;

    keyboard_handler_main();
    if (!input_pop_event(&event)) {
        out->key = 0;
        out->flags = 0;
        out->ascii = 0;
        out->source = 0;
        return 0;
    }

    out->key = event.key;
    out->flags = ((uint32_t)event.pressed) | ((uint32_t)event.modifiers << 8);
    out->ascii = (uint8_t)event.ascii;
    out->source = event.source;
    return 1;
}

int64_t sys_time_info(struct syscall_regs* regs) {
    struct aos_time_info_user* out = (struct aos_time_info_user*)(uintptr_t)regs->rdi;
    struct rtc_time time;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (rtc_read_time(&time) != 0) return -(int64_t)LINUX_EIO;

    out->year = time.year;
    out->month = time.month;
    out->day = time.day;
    out->hour = time.hour;
    out->minute = time.minute;
    out->second = time.second;
    out->weekday = time.weekday;
    out->reserved = 0;
    return 0;
}

int64_t sys_user_info(struct syscall_regs* regs) {
    struct aos_user_info_user* out = (struct aos_user_info_user*)(uintptr_t)regs->rdi;

    if (!out) return -(int64_t)LINUX_EFAULT;

    local_memset(out, 0, sizeof(*out));
    out->uid = process_get_uid();
    out->gid = process_get_gid();
    out->euid = process_get_euid();
    out->egid = process_get_egid();
    copy_cstr_bounded(out->username, sizeof(out->username), process_get_username());
    copy_cstr_bounded(out->home, sizeof(out->home), process_get_home());
    return 0;
}

static int cstr_equals_n(const char* a, const char* b, size_t b_len) {
    size_t i = 0;

    if (!a || !b) return 0;
    while (i < b_len) {
        if (a[i] == '\0' || a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == '\0';
}

static int shadow_password_matches(const char* username, const char* password) {
    struct vfs_node node;
    char shadow[1024];
    uint64_t size;
    uint64_t pos = 0;

    if (!username || !password) {
        return 0;
    }
    if (vfs_lookup("etc/shadow", &node) != 0 || node.type != VFS_NODE_TYPE_REGULAR) {
        return 0;
    }

    size = node.size;
    if (size >= sizeof(shadow)) {
        size = sizeof(shadow) - 1;
    }
    if (vfs_read_node(&node, 0, (uint8_t*)shadow, size) != 0) {
        return 0;
    }
    shadow[size] = '\0';

    while (pos < size) {
        uint64_t line_start = pos;
        uint64_t name_start = pos;
        uint64_t name_len = 0;
        uint64_t pass_start = 0;
        uint64_t pass_len = 0;

        while (pos < size && shadow[pos] != ':' && shadow[pos] != '\n') {
            pos++;
        }
        name_len = pos - name_start;
        if (pos >= size || shadow[pos] != ':') {
            while (pos < size && shadow[pos] != '\n') pos++;
            if (pos < size) pos++;
            continue;
        }
        pos++;
        pass_start = pos;
        while (pos < size && shadow[pos] != ':' && shadow[pos] != '\n') {
            pos++;
        }
        pass_len = pos - pass_start;

        if (cstr_equals_n(username, &shadow[name_start], (size_t)name_len)) {
            if (pass_len == 0) {
                return 1;
            }
            return cstr_equals_n(password, &shadow[pass_start], (size_t)pass_len);
        }

        (void)line_start;
        while (pos < size && shadow[pos] != '\n') pos++;
        if (pos < size) pos++;
    }

    return 0;
}

int64_t sys_sudo_auth(struct syscall_regs* regs) {
    char password[128];
    int64_t rc;

    if (process_is_root()) {
        process_become_root();
        return 0;
    }

    rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, password, sizeof(password));
    if (rc < 0) return rc;

    if (!shadow_password_matches(process_get_username(), password)) {
        return -(int64_t)LINUX_EACCES;
    }

    process_become_root();
    return 0;
}

int64_t sys_pci_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_pci_info_user* out = (struct aos_pci_info_user*)(uintptr_t)regs->rsi;
    const struct pci_device* dev;

    if (!out) return -(int64_t)LINUX_EFAULT;
    dev = pci_get((size_t)index);
    if (!dev) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    out->vendor_id = dev->vendor_id;
    out->device_id = dev->device_id;
    out->bus = dev->bus;
    out->slot = dev->slot;
    out->function = dev->function;
    out->class_code = dev->class_code;
    out->subclass = dev->subclass;
    out->prog_if = dev->prog_if;
    out->revision = dev->revision;
    out->header_type = dev->header_type;
    out->irq_line = dev->irq_line;
    for (size_t i = 0; i < 6; i++) {
        out->bar[i] = dev->bar[i];
    }
    return 0;
}

int64_t sys_driver_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_driver_info_user* out = (struct aos_driver_info_user*)(uintptr_t)regs->rsi;
    const struct driver_device* dev;

    if (!out) return -(int64_t)LINUX_EFAULT;
    dev = driver_get((size_t)index);
    if (!dev) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    out->type = dev->type;
    out->claimed = dev->claimed;
    out->bus = dev->bus;
    out->slot = dev->slot;
    out->function = dev->function;
    out->class_code = dev->class_code;
    out->subclass = dev->subclass;
    out->prog_if = dev->prog_if;
    out->irq_line = dev->irq_line;
    out->vendor_id = dev->vendor_id;
    out->device_id = dev->device_id;
    for (size_t i = 0; i < 6; i++) {
        out->bar[i] = dev->bar[i];
    }
    copy_cstr_bounded(out->driver, sizeof(out->driver), dev->driver);
    copy_cstr_bounded(out->status, sizeof(out->status), dev->status);
    return 0;
}

int64_t sys_netdev_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_netdev_info_user* out = (struct aos_netdev_info_user*)(uintptr_t)regs->rsi;
    const struct netdev* dev;

    if (!out) return -(int64_t)LINUX_EFAULT;
    dev = netdev_get((size_t)index);
    if (!dev) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    out->type = dev->type;
    out->link_up = dev->link_up;
    for (size_t i = 0; i < 6; i++) {
        out->mac[i] = dev->mac[i];
    }
    out->bus = dev->bus;
    out->slot = dev->slot;
    out->function = dev->function;
    for (size_t i = 0; i < 4; i++) {
        out->ipv4_addr[i] = dev->ipv4_addr[i];
        out->ipv4_gateway[i] = dev->ipv4_gateway[i];
        out->ipv4_dns[i] = dev->ipv4_dns[i];
    }
    out->ipv4_prefix = dev->ipv4_prefix;
    out->ipv4_configured = dev->ipv4_configured;
    out->ipv6_configured = dev->ipv6_configured;
    out->ipv6_prefix = dev->ipv6_prefix;
    for (size_t i = 0; i < 16; i++) {
        out->ipv6_addr[i] = dev->ipv6_addr[i];
        out->ipv6_gateway[i] = dev->ipv6_gateway[i];
        out->ipv6_dns[i] = dev->ipv6_dns[i];
    }
    copy_cstr_bounded(out->name, sizeof(out->name), dev->name);
    copy_cstr_bounded(out->driver, sizeof(out->driver), dev->driver);
    copy_cstr_bounded(out->status, sizeof(out->status), dev->status);
    return 0;
}

int64_t sys_netdev_stats(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_netdev_stats_user* out = (struct aos_netdev_stats_user*)(uintptr_t)regs->rsi;
    struct netdev_stats stats;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (netdev_get_stats((size_t)index, &stats) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }

    out->tx_packets = stats.tx_packets;
    out->rx_packets = stats.rx_packets;
    out->tx_bytes = stats.tx_bytes;
    out->rx_bytes = stats.rx_bytes;
    out->tx_errors = stats.tx_errors;
    out->rx_errors = stats.rx_errors;
    out->tx_dropped = stats.tx_dropped;
    out->rx_dropped = stats.rx_dropped;
    out->tx_arp = stats.tx_arp;
    out->rx_arp = stats.rx_arp;
    out->tx_ipv4 = stats.tx_ipv4;
    out->rx_ipv4 = stats.rx_ipv4;
    out->tx_ipv6 = stats.tx_ipv6;
    out->rx_ipv6 = stats.rx_ipv6;
    out->tx_icmp = stats.tx_icmp;
    out->rx_icmp = stats.rx_icmp;
    out->tx_udp = stats.tx_udp;
    out->rx_udp = stats.rx_udp;
    out->tx_tcp = stats.tx_tcp;
    out->rx_tcp = stats.rx_tcp;
    return 0;
}

int64_t sys_arp_cache_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_arp_cache_info_user* out = (struct aos_arp_cache_info_user*)(uintptr_t)regs->rsi;
    struct arp_cache_entry* entry;
    const struct netdev* dev;
    uint64_t now;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (index >= SOCKET_ARP_CACHE_ENTRIES) return -(int64_t)LINUX_ENOENT;

    entry = &g_arp_cache[index];
    now = timer_get_ticks();
    if (!entry->valid || now >= entry->expires_at) {
        entry->valid = 0;
        return -(int64_t)LINUX_ENOENT;
    }

    local_memset(out, 0, sizeof(*out));
    out->valid = 1;
    out->dev_index = entry->dev_index;
    out->hits = entry->hits;
    out->ttl_ticks = entry->expires_at - now;
    for (size_t i = 0; i < 4; i++) {
        out->ipv4[i] = entry->ipv4[i];
    }
    for (size_t i = 0; i < 6; i++) {
        out->mac[i] = entry->mac[i];
    }

    dev = netdev_get(entry->dev_index);
    if (dev) {
        copy_cstr_bounded(out->dev_name, sizeof(out->dev_name), dev->name);
    }
    return 0;
}

int64_t sys_ndp_cache_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_ndp_cache_info_user* out = (struct aos_ndp_cache_info_user*)(uintptr_t)regs->rsi;
    struct ndp_cache_entry* entry;
    const struct netdev* dev;
    uint64_t now;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (index >= SOCKET_NDP_CACHE_ENTRIES) return -(int64_t)LINUX_ENOENT;

    entry = &g_ndp_cache[index];
    now = timer_get_ticks();
    if (!entry->valid || now >= entry->expires_at) {
        entry->valid = 0;
        return -(int64_t)LINUX_ENOENT;
    }

    local_memset(out, 0, sizeof(*out));
    out->valid = 1;
    out->dev_index = entry->dev_index;
    out->hits = entry->hits;
    out->ttl_ticks = entry->expires_at - now;
    for (size_t i = 0; i < 16; i++) {
        out->ipv6[i] = entry->ipv6[i];
    }
    for (size_t i = 0; i < 6; i++) {
        out->mac[i] = entry->mac[i];
    }

    dev = netdev_get(entry->dev_index);
    if (dev) {
        copy_cstr_bounded(out->dev_name, sizeof(out->dev_name), dev->name);
    }
    return 0;
}

int64_t sys_dns_cache_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_dns_cache_info_user* out = (struct aos_dns_cache_info_user*)(uintptr_t)regs->rsi;
    struct dns_cache_entry* entry;
    const struct netdev* dev;
    uint64_t now;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (index >= SOCKET_DNS_CACHE_ENTRIES) return -(int64_t)LINUX_ENOENT;

    entry = &g_dns_cache[index];
    now = timer_get_ticks();
    if (!entry->valid || now >= entry->expires_at) {
        entry->valid = 0;
        return -(int64_t)LINUX_ENOENT;
    }

    local_memset(out, 0, sizeof(*out));
    out->valid = 1;
    out->dev_index = entry->dev_index;
    out->family = entry->family;
    out->hits = entry->hits;
    out->ttl_ticks = entry->expires_at - now;
    for (size_t i = 0; i < 4; i++) {
        out->ipv4[i] = entry->ipv4[i];
    }
    for (size_t i = 0; i < 16; i++) {
        out->ipv6[i] = entry->ipv6[i];
    }
    copy_cstr_bounded(out->name, sizeof(out->name), entry->name);

    dev = netdev_get(entry->dev_index);
    if (dev) {
        copy_cstr_bounded(out->dev_name, sizeof(out->dev_name), dev->name);
    }
    return 0;
}

int64_t sys_net_cache_flush(struct syscall_regs* regs) {
    uint64_t flags = regs->rdi;
    uint64_t dev_filter = regs->rsi;
    uint64_t flushed = 0;

    if ((flags & ~7ULL) != 0 || flags == 0) return -(int64_t)LINUX_EINVAL;
    if (dev_filter > 255U) return -(int64_t)LINUX_EINVAL;

    if (flags & 1U) {
        for (size_t i = 0; i < SOCKET_ARP_CACHE_ENTRIES; i++) {
            if (!g_arp_cache[i].valid) continue;
            if (dev_filter != 255U && g_arp_cache[i].dev_index != dev_filter) continue;
            g_arp_cache[i].valid = 0;
            flushed++;
        }
    }

    if (flags & 2U) {
        for (size_t i = 0; i < SOCKET_DNS_CACHE_ENTRIES; i++) {
            if (!g_dns_cache[i].valid) continue;
            if (dev_filter != 255U && g_dns_cache[i].dev_index != dev_filter) continue;
            g_dns_cache[i].valid = 0;
            flushed++;
        }
    }

    if (flags & 4U) {
        for (size_t i = 0; i < SOCKET_NDP_CACHE_ENTRIES; i++) {
            if (!g_ndp_cache[i].valid) continue;
            if (dev_filter != 255U && g_ndp_cache[i].dev_index != dev_filter) continue;
            g_ndp_cache[i].valid = 0;
            flushed++;
        }
    }

    return (int64_t)flushed;
}

int64_t sys_socket_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_socket_info_user* out = (struct aos_socket_info_user*)(uintptr_t)regs->rsi;
    struct socket_object* sock;
    const struct netdev* dev;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (index >= MAX_SOCKET_OBJECTS) return -(int64_t)LINUX_ENOENT;

    sock = &g_socket_objects[index];
    if (!sock->in_use) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    out->valid = 1;
    out->index = (uint8_t)index;
    out->state = sock->state;
    out->family = sock->family;
    out->type = sock->type;
    out->protocol = sock->protocol;
    out->dev_index = sock->dev_index;
    out->local_port = sock->local_port;
    out->remote_port = sock->remote_port;
    out->refcount = sock->refcount;
    out->rx_len = sock->rx_len;
    out->rx_off = sock->rx_off;
    out->seq = sock->seq;
    out->ack = sock->ack;
    out->peer_window = sock->peer_window;
    out->peer_mss = sock->peer_mss;
    out->peer_window_bytes = sock->peer_window_bytes;
    out->retransmits = sock->retransmits;
    out->cwnd_bytes = sock->cwnd_bytes;
    out->ssthresh_bytes = sock->ssthresh_bytes;
    out->local_window_scale = sock->local_window_scale;
    out->peer_window_scale = sock->peer_window_scale;
    for (size_t i = 0; i < 4; i++) {
        out->remote_ip[i] = sock->remote_ip[i];
        out->next_hop_ip[i] = sock->next_hop_ip[i];
    }
    for (size_t i = 0; i < 16; i++) {
        out->remote_ip6[i] = sock->remote_ip6[i];
        out->next_hop_ip6[i] = sock->next_hop_ip6[i];
    }
    for (size_t i = 0; i < 6; i++) {
        out->remote_mac[i] = sock->remote_mac[i];
    }
    dev = netdev_get(sock->dev_index);
    if (dev) {
        copy_cstr_bounded(out->dev_name, sizeof(out->dev_name), dev->name);
    }
    return 0;
}

int64_t sys_netdev_send(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    const uint8_t* frame = (const uint8_t*)(uintptr_t)regs->rsi;
    uint64_t length = regs->rdx;
    uint8_t local_frame[1518];
    int rc;

    if (!frame || length < 14 || length > sizeof(local_frame)) {
        return -(int64_t)LINUX_EINVAL;
    }

    local_memcpy(local_frame, frame, (size_t)length);
    rc = netdev_send((size_t)index, local_frame, (uint16_t)length);
    if (rc < 0) {
        return -(int64_t)LINUX_EIO;
    }
    return rc;
}

static int bytes_equal(const uint8_t* a, const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static void arp_cache_store(uint8_t dev_index, const uint8_t ipv4[4], const uint8_t mac[6]) {
    size_t slot = 0;
    uint32_t hits = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    if (!ipv4 || !mac) return;

    for (size_t i = 0; i < SOCKET_ARP_CACHE_ENTRIES; i++) {
        if (!g_arp_cache[i].valid) {
            slot = i;
            goto store;
        }
        if (g_arp_cache[i].dev_index == dev_index && bytes_equal(g_arp_cache[i].ipv4, ipv4, 4)) {
            slot = i;
            hits = g_arp_cache[i].hits + 1;
            goto store;
        }
        if (g_arp_cache[i].expires_at < oldest) {
            oldest = g_arp_cache[i].expires_at;
            slot = i;
        }
    }

store:
    local_memset(&g_arp_cache[slot], 0, sizeof(g_arp_cache[slot]));
    g_arp_cache[slot].valid = 1;
    g_arp_cache[slot].dev_index = dev_index;
    g_arp_cache[slot].hits = hits;
    g_arp_cache[slot].expires_at = now + SOCKET_ARP_CACHE_TTL_TICKS;
    local_memcpy(g_arp_cache[slot].ipv4, ipv4, 4);
    local_memcpy(g_arp_cache[slot].mac, mac, 6);
}

static void arp_cache_learn_from_frame(uint8_t dev_index, const uint8_t* frame, uint16_t length) {
    const uint8_t* arp;
    uint16_t op;

    if (!frame || length < 42) return;
    if (frame[12] != 0x08 || frame[13] != 0x06) return;

    arp = frame + 14;
    if (arp[0] != 0x00 || arp[1] != 0x01) return;
    if (arp[2] != 0x08 || arp[3] != 0x00) return;
    if (arp[4] != 0x06 || arp[5] != 0x04) return;

    op = ((uint16_t)arp[6] << 8) | arp[7];
    if (op != 1 && op != 2) return;

    arp_cache_store(dev_index, arp + 14, arp + 8);
}

static void ndp_cache_store(uint8_t dev_index, const uint8_t ipv6[16], const uint8_t mac[6]) {
    size_t slot = 0;
    uint32_t hits = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    if (!ipv6 || !mac) return;

    for (size_t i = 0; i < SOCKET_NDP_CACHE_ENTRIES; i++) {
        if (!g_ndp_cache[i].valid) {
            slot = i;
            goto store;
        }
        if (g_ndp_cache[i].dev_index == dev_index && bytes_equal(g_ndp_cache[i].ipv6, ipv6, 16)) {
            slot = i;
            hits = g_ndp_cache[i].hits + 1;
            goto store;
        }
        if (g_ndp_cache[i].expires_at < oldest) {
            oldest = g_ndp_cache[i].expires_at;
            slot = i;
        }
    }

store:
    local_memset(&g_ndp_cache[slot], 0, sizeof(g_ndp_cache[slot]));
    g_ndp_cache[slot].valid = 1;
    g_ndp_cache[slot].dev_index = dev_index;
    g_ndp_cache[slot].hits = hits;
    g_ndp_cache[slot].expires_at = now + SOCKET_NDP_CACHE_TTL_TICKS;
    local_memcpy(g_ndp_cache[slot].ipv6, ipv6, 16);
    local_memcpy(g_ndp_cache[slot].mac, mac, 6);
}

static void ndp_cache_learn_from_frame(uint8_t dev_index, const uint8_t* frame, uint16_t length) {
    const uint8_t* ip;
    const uint8_t* icmp;
    uint8_t mac[6];

    if (!frame || length < 86) return;
    if (frame[12] != 0x86 || frame[13] != 0xdd) return;

    ip = frame + 14;
    icmp = frame + 54;
    if ((ip[0] >> 4) != 6) return;
    if (ip[6] != 58) return;
    if (icmp[0] != 136) return;

    local_memcpy(mac, frame + 6, sizeof(mac));
    if (icmp[24] == 2 && icmp[25] == 1) {
        local_memcpy(mac, icmp + 26, sizeof(mac));
    }
    ndp_cache_store(dev_index, icmp + 8, mac);
}

int64_t sys_netdev_recv(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    uint8_t* frame = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t max_length = regs->rdx;
    uint8_t local_frame[1518];
    int rc;

    if (!frame || max_length < 14) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (max_length > sizeof(local_frame)) {
        max_length = sizeof(local_frame);
    }

    rc = netdev_recv((size_t)index, local_frame, (uint16_t)max_length);
    if (rc < 0) {
        return -(int64_t)LINUX_EIO;
    }
    if (rc > 0) {
        arp_cache_learn_from_frame((uint8_t)index, local_frame, (uint16_t)rc);
        ndp_cache_learn_from_frame((uint8_t)index, local_frame, (uint16_t)rc);
        local_memcpy(frame, local_frame, (size_t)rc);
    }
    return rc;
}

int64_t sys_netdev_ipv6_config(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    const uint8_t* addr = (const uint8_t*)(uintptr_t)regs->rsi;
    uint64_t prefix = regs->rdx;
    const uint8_t* gateway = (const uint8_t*)(uintptr_t)regs->r10;
    const uint8_t* dns = (const uint8_t*)(uintptr_t)regs->r8;
    uint8_t local_addr[16];
    uint8_t local_gateway[16];
    uint8_t local_dns[16];

    if (!addr || prefix > 128) {
        return -(int64_t)LINUX_EINVAL;
    }

    local_memcpy(local_addr, addr, sizeof(local_addr));
    if (gateway) {
        local_memcpy(local_gateway, gateway, sizeof(local_gateway));
    } else {
        local_memset(local_gateway, 0, sizeof(local_gateway));
    }
    if (dns) {
        local_memcpy(local_dns, dns, sizeof(local_dns));
    } else {
        local_memset(local_dns, 0, sizeof(local_dns));
    }

    if (netdev_configure_ipv6((size_t)index,
                              local_addr,
                              (uint8_t)prefix,
                              local_gateway,
                              local_dns) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    return 0;
}

int64_t sys_netdev_ipv4_config(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    const uint8_t* addr = (const uint8_t*)(uintptr_t)regs->rsi;
    uint64_t prefix = regs->rdx;
    const uint8_t* gateway = (const uint8_t*)(uintptr_t)regs->r10;
    const uint8_t* dns = (const uint8_t*)(uintptr_t)regs->r8;
    uint8_t local_addr[4];
    uint8_t local_gateway[4];
    uint8_t local_dns[4];

    if (!addr || prefix > 32) {
        return -(int64_t)LINUX_EINVAL;
    }

    local_memcpy(local_addr, addr, sizeof(local_addr));
    if (gateway) {
        local_memcpy(local_gateway, gateway, sizeof(local_gateway));
    } else {
        local_memset(local_gateway, 0, sizeof(local_gateway));
    }
    if (dns) {
        local_memcpy(local_dns, dns, sizeof(local_dns));
    } else {
        local_memset(local_dns, 0, sizeof(local_dns));
    }

    if (netdev_configure_ipv4((size_t)index,
                              local_addr,
                              (uint8_t)prefix,
                              local_gateway,
                              local_dns) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    return 0;
}

int64_t sys_firmware_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_firmware_info_user* out = (struct aos_firmware_info_user*)(uintptr_t)regs->rsi;
    const struct firmware_blob* blob;

    if (!out) return -(int64_t)LINUX_EFAULT;
    blob = firmware_get((size_t)index);
    if (!blob) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    copy_cstr_bounded(out->name, sizeof(out->name), blob->name);
    out->size = blob->size;
    return 0;
}

int64_t sys_wifi_scan_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_wifi_scan_info_user* out = (struct aos_wifi_scan_info_user*)(uintptr_t)regs->rsi;
    const struct mac80211_scan_result* result;

    if (!out) return -(int64_t)LINUX_EFAULT;
    result = mac80211_scan_get((uint32_t)index);
    if (!result) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    copy_cstr_bounded(out->ssid, sizeof(out->ssid), result->ssid);
    for (size_t i = 0; i < 6; i++) {
        out->bssid[i] = result->bssid[i];
    }
    out->channel = result->channel;
    out->rssi_dbm = result->rssi_dbm;
    out->security = result->security;
    return 0;
}

int64_t sys_wifi_state_info(struct syscall_regs* regs) {
    struct aos_wifi_state_info_user* out = (struct aos_wifi_state_info_user*)(uintptr_t)regs->rsi;
    const struct mac80211_state* state;

    if (!out) return -(int64_t)LINUX_EFAULT;
    state = mac80211_get_state();

    local_memset(out, 0, sizeof(*out));
    out->state = state->state;
    out->scan_count = state->scan_count;
    out->selected = state->selected;
    out->security = state->security;
    copy_cstr_bounded(out->ssid, sizeof(out->ssid), state->ssid);
    for (size_t i = 0; i < 6; i++) {
        out->bssid[i] = state->bssid[i];
    }
    out->channel = state->channel;
    out->rssi_dbm = state->rssi_dbm;
    out->auth_attempts = state->auth_attempts;
    out->assoc_attempts = state->assoc_attempts;
    out->rx_mgmt = state->rx_mgmt;
    out->rx_beacon = state->rx_beacon;
    out->rx_probe_resp = state->rx_probe_resp;
    out->rx_data = state->rx_data;
    out->rx_other = state->rx_other;
    out->tx_mgmt = state->tx_mgmt;
    out->tx_probe_req = state->tx_probe_req;
    out->last_tx_len = state->last_tx_len;
    out->driver_tx_calls = state->driver_tx_calls;
    out->driver_tx_hw = state->driver_tx_hw;
    out->driver_tx_sim = state->driver_tx_sim;
    out->driver_tx_errors = state->driver_tx_errors;
    out->driver_rx_calls = state->driver_rx_calls;
    out->driver_rx_accepted = state->driver_rx_accepted;
    out->driver_rx_errors = state->driver_rx_errors;
    out->driver_rx_last_len = state->driver_rx_last_len;
    out->driver_rx_last_rssi = state->driver_rx_last_rssi;
    return 0;
}

int64_t sys_wifi_control(struct syscall_regs* regs) {
    uint64_t action = regs->rdi;
    uint64_t index = regs->rsi;
    int rc = -1;

    switch (action) {
        case 1:
            rc = mac80211_select_network((uint32_t)index);
            break;
        case 2:
            rc = mac80211_authenticate_selected();
            break;
        case 3:
            rc = mac80211_associate_selected();
            break;
        case 4:
            rc = mac80211_select_network((uint32_t)index);
            if (rc == 0) rc = mac80211_authenticate_selected();
            if (rc == 0) rc = mac80211_associate_selected();
            break;
        case 5:
            rc = mac80211_active_probe();
            break;
        case 6:
            rc = mac80211_test_rx_probe_response();
            break;
        default:
            return -(int64_t)LINUX_EINVAL;
    }

    return rc == 0 ? 0 : -(int64_t)LINUX_EINVAL;
}

int64_t sys_shutdown(struct syscall_regs* regs) {
    (void)regs;

    serial_print("AOS: shutdown requested\n");

    /* QEMU/modern ACPI PM1a control block. */
    outw_local(0x604, 0x2000);
    io_wait();

    /* Bochs/QEMU legacy poweroff ports. */
    outw_local(0xB004, 0x2000);
    io_wait();
    outw_local(0x4004, 0x3400);
    io_wait();

    return -(int64_t)LINUX_EIO;
}

int64_t sys_restart(struct syscall_regs* regs) {
    (void)regs;

    serial_print("AOS: restart requested\n");

    /* PCI reset control, then classic keyboard controller reset. */
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    io_wait();
    outb(0x64, 0xFE);
    io_wait();

    return -(int64_t)LINUX_EIO;
}

int64_t sys_partition_create(struct syscall_regs* regs) {
    uint8_t fs_type = (uint8_t)regs->rdi;
    uint64_t size = regs->rsi;
    uint8_t role = (uint8_t)regs->rdx;
    int rc = partition_create_planned(fs_type, size, role);
    if (rc < 0) return -(int64_t)LINUX_EINVAL;
    return rc;
}

int64_t sys_partition_delete(struct syscall_regs* regs) {
    if (partition_delete_planned((size_t)regs->rdi) != 0) return -(int64_t)LINUX_EINVAL;
    return 0;
}

int64_t sys_partition_type(struct syscall_regs* regs) {
    if (partition_cycle_planned_type((size_t)regs->rdi) != 0) return -(int64_t)LINUX_EINVAL;
    return 0;
}

int64_t sys_partition_write(struct syscall_regs* regs) {
    uint32_t blkdev_id = (uint32_t)regs->rdi;
    if (partition_write_table(blkdev_id) != 0) return -(int64_t)LINUX_EIO;
    return 0;
}

int64_t sys_mount_info(struct syscall_regs* regs) {
    struct vfs_mount_info* out = (struct vfs_mount_info*)(uintptr_t)regs->rsi;
    if (!out) return -(int64_t)LINUX_EFAULT;
    if (vfs_mount_info_at((size_t)regs->rdi, out) != 0) return -(int64_t)LINUX_ENOENT;
    return 0;
}

int64_t sys_partition_role(struct syscall_regs* regs) {
    if (partition_cycle_planned_role((size_t)regs->rdi) != 0) return -(int64_t)LINUX_EINVAL;
    return 0;
}

int64_t sys_partition_layout(struct syscall_regs* regs) {
    uint32_t blkdev_id = (uint32_t)regs->rdi;
    const struct partition* part;

    if (partition_create_default_layout(blkdev_id) != 0) return -(int64_t)LINUX_EIO;
    if (partition_write_table(blkdev_id) != 0) return -(int64_t)LINUX_EIO;

    part = partition_find_by_role(PARTITION_ROLE_ROOT);
    if (part && part->fs_type == PARTITION_FS_AOSFS) {
        (void)aosfs_mount_role(PARTITION_ROLE_ROOT, part->blkdev_id, part->offset);
    }
    part = partition_find_by_role(PARTITION_ROLE_MAIN);
    if (part && part->fs_type == PARTITION_FS_AOSFS &&
        aosfs_mount_role(PARTITION_ROLE_MAIN, part->blkdev_id, part->offset) == 0) {
        (void)vfs_mount("main", VFS_BACKEND_AOSFS, "@main");
    }
    part = partition_find_by_role(PARTITION_ROLE_ETC);
    if (part && part->fs_type == PARTITION_FS_AOSFS &&
        aosfs_mount_role(PARTITION_ROLE_ETC, part->blkdev_id, part->offset) == 0) {
        (void)vfs_mount("etc", VFS_BACKEND_AOSFS, "@etc");
    }
    part = partition_find_by_role(PARTITION_ROLE_COMMANDS);
    if (part && part->fs_type == PARTITION_FS_AOSFS) {
        (void)aosfs_mount_role(PARTITION_ROLE_COMMANDS, part->blkdev_id, part->offset);
    }

    return 0;
}
