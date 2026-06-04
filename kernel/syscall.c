/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>
#include <stddef.h>
#include <cpio.h>
#include <aosfs.h>
#include <ext4.h>
#include <fat32.h>
#include <vfs.h>
#include <pmm.h>
#include <vmm.h>
#include <elf64_loader.h>
#include <syscall.h>
#include <process.h>
#include <vga.h>
#include <tty.h>
#include <tmpfs.h>
#include <partition.h>
#include <blkdev.h>
#include <pci.h>
#include <driver.h>
#include <firmware.h>
#include <netdev.h>
#include <timer.h>
#include <rtc.h>
#include <gfx.h>
#include <input.h>
#include <mac80211.h>

extern void outb(uint16_t port, uint8_t val);
extern void serial_print(const char* s);
extern uint64_t p4_table[];
extern void jump_to_user(uint64_t code, uint64_t stack);
extern void switch_to_process(process_t* proc);

static inline void outw_local(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

#define USER_STACK_BASE 0x70000080000ULL
#define USER_STACK_SIZE 65536ULL
#define USER_STACK_PAGES (USER_STACK_SIZE / 4096ULL)
#define USER_MMAP_BASE 0x0000710000000000ULL

#ifndef AOS_LIVE_PERMISSIVE
#define AOS_LIVE_PERMISSIVE 1
#endif

#define MAX_EXEC_ARGS 16
#define MAX_EXEC_ENVP 16
#define MAX_EXEC_STRING 256
#define MAX_FILE_HANDLES 16
#define MAX_PIPE_OBJECTS 8
#define LINUX_AT_FDCWD (-100)
#define LINUX_AT_EMPTY_PATH 0x1000
#define LINUX_AT_REMOVEDIR 0x200
#define LINUX_O_ACCMODE 3
#define LINUX_O_WRONLY 1
#define LINUX_O_RDWR 2
#define LINUX_O_CREAT 64
#define LINUX_O_EXCL 128
#define LINUX_O_TRUNC 512
#define LINUX_O_DIRECTORY 0x10000
#define LINUX_PROT_WRITE 0x2
#define LINUX_MAP_PRIVATE 0x02
#define LINUX_MAP_ANONYMOUS 0x20
#define LINUX_F_OK 0
#define LINUX_X_OK 1
#define LINUX_W_OK 2
#define LINUX_R_OK 4
#define LINUX_DTYPE_REG 8
#define LINUX_DTYPE_DIR 4
#define LINUX_S_IFIFO 0010000U
#define LINUX_S_IFCHR 0020000U
#define LINUX_S_IFREG 0100000U
#define LINUX_S_IFDIR 0040000U
#define LINUX_S_IRUSR 00400U
#define LINUX_S_IWUSR 00200U
#define LINUX_S_IRGRP 00040U
#define LINUX_S_IROTH 00004U
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define IA32_FS_BASE_MSR 0xC0000100
#define LINUX_ARCH_SET_GS 0x1001
#define LINUX_ARCH_SET_FS 0x1002
#define LINUX_ARCH_GET_FS 0x1003
#define LINUX_ARCH_GET_GS 0x1004
#define LINUX_TCGETS 0x5401
#define LINUX_TCSETS 0x5402
#define LINUX_TCSETSW 0x5403
#define LINUX_TCSETSF 0x5404
#define LINUX_TIOCGWINSZ 0x5413
#define LINUX_TIOCSWINSZ 0x5414
#define LINUX_FIONREAD 0x541B
#define LINUX_TIOCGPGRP 0x540F
#define LINUX_TIOCSPGRP 0x5410
#define LINUX_F_DUPFD 0
#define LINUX_F_GETFD 1
#define LINUX_F_SETFD 2
#define LINUX_F_GETFL 3
#define LINUX_F_SETFL 4
#define LINUX_AF_INET 2
#define LINUX_SOCK_STREAM 1
#define LINUX_IPPROTO_TCP 6
#define LINUX_TERMIOS_ICRNL 0000400U
#define LINUX_TERMIOS_OPOST 0000001U
#define LINUX_TERMIOS_CS8 0000060U
#define LINUX_TERMIOS_CREAD 0000200U
#define LINUX_TERMIOS_ISIG 0000001U
#define LINUX_TERMIOS_ICANON 0000002U
#define LINUX_TERMIOS_ECHO 0000010U
#define LINUX_TERMIOS_ECHOE 0000020U
#define LINUX_TERMIOS_ECHOK 0000040U
#define LINUX_TERMIOS_IEXTEN 0100000U
struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct linux_termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_line;
    uint8_t c_cc[19];
};

struct linux_winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

struct linux_iovec {
    uint64_t iov_base;
    uint64_t iov_len;
};

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

enum fd_kind {
    FD_KIND_FREE = 0,
    FD_KIND_STDIN = 1,
    FD_KIND_STDOUT = 2,
    FD_KIND_STDERR = 3,
    FD_KIND_VNODE = 4,
    FD_KIND_PIPE_READER = 5,
    FD_KIND_PIPE_WRITER = 6,
    FD_KIND_TTY = 7,
    FD_KIND_NULL = 8,
    FD_KIND_SOCKET = 9,
};

#define MAX_SOCKET_OBJECTS 8
#define SOCKET_TX_FRAME_SIZE 1518
#define SOCKET_RX_FRAME_SIZE 1518
#define SOCKET_RX_BUFFER_SIZE 2048
#define SOCKET_TCP_PORT_FIRST 40002
#define SOCKET_TCP_PORT_LAST 60000
#define SOCKET_TCP_BASE_SEQ 0xA0502026U
#define SOCKET_DNS_PORT 40001
#define SOCKET_DNS_TXID 0xA057
#define SOCKET_DNS_NAME_MAX 128
#define SOCKET_DNS_CACHE_ENTRIES 8
#define SOCKET_DNS_CACHE_TTL_TICKS 6000ULL
#define SOCKET_ARP_CACHE_ENTRIES 8
#define SOCKET_ARP_CACHE_TTL_TICKS 6000ULL
#define SOCKET_DNS_RECV_POLLS 1000000
#define SOCKET_DNS_RETRIES 3
#define SOCKET_TCP_RETRIES 3
#define SOCKET_TCP_RECV_RETRIES 3

enum socket_state {
    SOCKET_STATE_FREE = 0,
    SOCKET_STATE_CREATED = 1,
    SOCKET_STATE_CONNECTED = 2,
    SOCKET_STATE_CLOSED = 3,
};

struct socket_object {
    uint8_t in_use;
    uint8_t state;
    uint8_t family;
    uint8_t type;
    uint8_t protocol;
    uint8_t dev_index;
    uint16_t local_port;
    uint32_t refcount;
    uint16_t remote_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t remote_ip[4];
    uint8_t next_hop_ip[4];
    uint8_t remote_mac[6];
    uint8_t rx_buffer[SOCKET_RX_BUFFER_SIZE];
    uint32_t rx_len;
    uint32_t rx_off;
};

struct linux_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint8_t sin_addr[4];
    uint8_t sin_zero[8];
} __attribute__((packed));

struct dns_cache_entry {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t expires_at;
    char name[SOCKET_DNS_NAME_MAX];
    uint8_t ipv4[4];
};

struct arp_cache_entry {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t expires_at;
    uint8_t ipv4[4];
    uint8_t mac[6];
    uint8_t reserved2[2];
};

struct file_handle {
    uint8_t in_use;
    uint8_t reserved[7];
    uint32_t refcount;
    uint32_t reserved2;
    uint64_t offset;
    uint64_t open_flags;
    struct vfs_node node;
};

struct pipe_object {
    uint8_t in_use;
    uint8_t reserved[3];
    uint32_t read_refs;
    uint32_t write_refs;
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t size;
    uint8_t buffer[512];
};

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

struct linux_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct linux_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

#define LINUX_POLLIN 0x0001
#define LINUX_POLLOUT 0x0004
#define LINUX_POLLERR 0x0008
#define LINUX_POLLHUP 0x0010
#define LINUX_POLLNVAL 0x0020

static struct file_handle g_file_handles[MAX_FILE_HANDLES];
static struct pipe_object g_pipe_objects[MAX_PIPE_OBJECTS];
static struct socket_object g_socket_objects[MAX_SOCKET_OBJECTS];
static struct dns_cache_entry g_dns_cache[SOCKET_DNS_CACHE_ENTRIES];
static struct arp_cache_entry g_arp_cache[SOCKET_ARP_CACHE_ENTRIES];
static uint16_t g_socket_next_port = SOCKET_TCP_PORT_FIRST;
static void halt_forever(void);
static int64_t socket_send_data(struct socket_object* sock, const uint8_t* data, uint64_t len);
static int64_t socket_recv_data(struct socket_object* sock, uint8_t* dst, uint64_t len);
static int socket_send_fin_close(struct socket_object* sock);

static void write_msr_u64(uint32_t msr, uint64_t value) {
    asm volatile(
        "wrmsr"
        :
        : "c"(msr), "a"((uint32_t)value), "d"((uint32_t)(value >> 32))
        : "memory"
    );
}

void process_load_fs_base(uint64_t fs_base) {
    write_msr_u64(IA32_FS_BASE_MSR, fs_base);
}

static void* local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
    return dst;
}

static void* local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

static uint64_t align_up_page(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static void set_uts_field(char* dst, const char* src) {
    uint64_t i = 0;
    while (src[i] && i < 64) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int64_t copy_user_cstr(const char* user, char* dst, size_t dst_size) {
    if (!user || !dst || dst_size == 0) return -(int64_t)LINUX_EFAULT;

    for (size_t i = 0; i < dst_size; i++) {
        char c = user[i];
        dst[i] = c;
        if (c == '\0') return (int64_t)i;
    }

    dst[dst_size - 1] = '\0';
    return -(int64_t)LINUX_E2BIG;
}

static struct fd_entry* current_fd_table(void) {
    return process_get_fd_table();
}

static void switch_page_table(uint64_t* table) {
    asm volatile("mov %0, %%cr3" : : "r"(table) : "memory");
}

static const char* normalize_path(const char* path) {
    while (*path == '/') {
        path++;
    }
    return path;
}

#if !AOS_LIVE_PERMISSIVE
static int path_starts_with_component(const char* path, const char* prefix) {
    size_t i = 0;

    if (!path || !prefix) return 0;
    while (path[0] == '/') path++;
    while (prefix[0] == '/') prefix++;

    while (prefix[i] != '\0') {
        if (path[i] != prefix[i]) return 0;
        i++;
    }

    return path[i] == '\0' || path[i] == '/';
}
#endif

static int user_can_mutate_path(const char* path) {
#if AOS_LIVE_PERMISSIVE
    (void)path;
    return 1;
#else
    if (process_is_root()) return 1;
    if (!path || *path == '\0') return 0;

    return path_starts_with_component(path, process_get_home()) ||
           path_starts_with_component(path, "tmp") ||
           path_starts_with_component(path, "trash");
#endif
}

static int component_is_dot(const char* s, size_t len) {
    return len == 1 && s[0] == '.';
}

static int component_is_dotdot(const char* s, size_t len) {
    return len == 2 && s[0] == '.' && s[1] == '.';
}

static void pop_path_component(char* out, size_t* len) {
    while (*len > 0 && out[*len - 1] != '/') {
        (*len)--;
    }
    if (*len > 0 && out[*len - 1] == '/') {
        (*len)--;
    }
    out[*len] = '\0';
}

static int append_path_component(char* out, size_t out_size, size_t* len, const char* component, size_t component_len) {
    if (*len != 0) {
        if (*len + 1 >= out_size) return -1;
        out[*len] = '/';
        (*len)++;
    }
    if (*len + component_len >= out_size) return -1;
    for (size_t i = 0; i < component_len; i++) {
        out[*len + i] = component[i];
    }
    *len += component_len;
    out[*len] = '\0';
    return 0;
}

static int feed_path_components(const char* src, char* out, size_t out_size, size_t* len) {
    const char* cursor = src;

    while (cursor && *cursor) {
        size_t component_len = 0;
        while (*cursor == '/') {
            cursor++;
        }
        while (cursor[component_len] && cursor[component_len] != '/') {
            component_len++;
        }
        if (component_len == 0) {
            break;
        }
        if (component_is_dot(cursor, component_len)) {
            /* Stay on the current path. */
        } else if (component_is_dotdot(cursor, component_len)) {
            pop_path_component(out, len);
        } else if (append_path_component(out, out_size, len, cursor, component_len) != 0) {
            return -1;
        }
        cursor += component_len;
    }

    return 0;
}

static int canonicalize_path(const char* base, const char* input, char* out, size_t out_size) {
    size_t len = 0;

    if (!input || !out || out_size == 0) return -1;
    out[0] = '\0';

    if (input[0] != '/' && base && *base) {
        if (feed_path_components(base, out, out_size, &len) != 0) {
            return -1;
        }
    }

    if (feed_path_components(input, out, out_size, &len) != 0) {
        return -1;
    }

    return 0;
}

static int allocate_fd_slot(int start_fd) {
    struct fd_entry* table = current_fd_table();
    if (!table) {
        return -1;
    }
    for (int fd = start_fd; fd < PROCESS_FD_MAX; fd++) {
        if (table[fd].kind == FD_KIND_FREE) {
            return fd;
        }
    }
    return -1;
}

static struct file_handle* get_file_handle_by_index(int32_t handle_index) {
    if (handle_index < 0 || handle_index >= MAX_FILE_HANDLES) {
        return NULL;
    }
    if (!g_file_handles[handle_index].in_use) {
        return NULL;
    }
    return &g_file_handles[handle_index];
}

static struct fd_entry* get_fd_entry(uint64_t fd) {
    struct fd_entry* table = current_fd_table();
    if (!table || fd >= PROCESS_FD_MAX) {
        return NULL;
    }
    if (table[fd].kind == FD_KIND_FREE) {
        return NULL;
    }
    return &table[fd];
}

static struct file_handle* get_vnode_handle(uint64_t fd) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry || entry->kind != FD_KIND_VNODE) {
        return NULL;
    }
    return get_file_handle_by_index(entry->handle_index);
}

static struct pipe_object* get_pipe_object_by_index(int32_t pipe_index) {
    if (pipe_index < 0 || pipe_index >= MAX_PIPE_OBJECTS) {
        return NULL;
    }
    if (!g_pipe_objects[pipe_index].in_use) {
        return NULL;
    }
    return &g_pipe_objects[pipe_index];
}

static struct pipe_object* get_pipe_for_fd(uint64_t fd, uint8_t expected_kind) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry || entry->kind != expected_kind) {
        return NULL;
    }
    return get_pipe_object_by_index(entry->handle_index);
}

static struct socket_object* get_socket_by_index(int32_t socket_index) {
    if (socket_index < 0 || socket_index >= MAX_SOCKET_OBJECTS) {
        return NULL;
    }
    if (!g_socket_objects[socket_index].in_use) {
        return NULL;
    }
    return &g_socket_objects[socket_index];
}

static struct socket_object* get_socket_for_fd(uint64_t fd) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry || entry->kind != FD_KIND_SOCKET) {
        return NULL;
    }
    return get_socket_by_index(entry->handle_index);
}

static void release_file_handle(int32_t handle_index) {
    struct file_handle* handle = get_file_handle_by_index(handle_index);
    if (!handle) {
        return;
    }
    if (handle->refcount > 0) {
        handle->refcount--;
    }
    if (handle->refcount == 0) {
        local_memset(handle, 0, sizeof(*handle));
    }
}

static void retain_fd_entry_refs(struct fd_entry* entry) {
    if (!entry) {
        return;
    }
    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* handle = get_file_handle_by_index(entry->handle_index);
        if (handle) {
            handle->refcount++;
        }
        return;
    }
    if (entry->kind == FD_KIND_PIPE_READER || entry->kind == FD_KIND_PIPE_WRITER) {
        struct pipe_object* pipe = get_pipe_object_by_index(entry->handle_index);
        if (!pipe) {
            return;
        }
        if (entry->kind == FD_KIND_PIPE_READER) {
            pipe->read_refs++;
        } else {
            pipe->write_refs++;
        }
        return;
    }
    if (entry->kind == FD_KIND_SOCKET) {
        struct socket_object* sock = get_socket_by_index(entry->handle_index);
        if (sock) {
            sock->refcount++;
        }
    }
}

static void release_pipe_ref(int32_t pipe_index, uint8_t kind) {
    struct pipe_object* pipe = get_pipe_object_by_index(pipe_index);
    if (!pipe) {
        return;
    }
    if (kind == FD_KIND_PIPE_READER) {
        if (pipe->read_refs > 0) pipe->read_refs--;
    } else if (kind == FD_KIND_PIPE_WRITER) {
        if (pipe->write_refs > 0) pipe->write_refs--;
    }
    if (pipe->read_refs == 0 && pipe->write_refs == 0) {
        local_memset(pipe, 0, sizeof(*pipe));
    }
}

static void release_socket_ref(int32_t socket_index) {
    struct socket_object* sock = get_socket_by_index(socket_index);
    if (!sock) {
        return;
    }
    if (sock->refcount > 0) {
        sock->refcount--;
    }
    if (sock->refcount == 0) {
        local_memset(sock, 0, sizeof(*sock));
    }
}

static void close_socket_ref(int32_t socket_index) {
    struct socket_object* sock = get_socket_by_index(socket_index);
    if (!sock) {
        return;
    }
    if (sock->refcount <= 1) {
        if (sock->state == SOCKET_STATE_CONNECTED) {
            (void)socket_send_fin_close(sock);
        } else {
            sock->state = SOCKET_STATE_CLOSED;
        }
    }
    release_socket_ref(socket_index);
}

static void close_fd_internal(uint64_t fd) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry) {
        return;
    }
    if (entry->kind == FD_KIND_VNODE) {
        release_file_handle(entry->handle_index);
    } else if (entry->kind == FD_KIND_PIPE_READER || entry->kind == FD_KIND_PIPE_WRITER) {
        release_pipe_ref(entry->handle_index, entry->kind);
    } else if (entry->kind == FD_KIND_SOCKET) {
        close_socket_ref(entry->handle_index);
    }
    entry->kind = FD_KIND_FREE;
    entry->handle_index = -1;
}

static int64_t install_vnode_fd(const struct vfs_node* node, uint64_t open_flags) {
    int handle_index = -1;
    int fd = -1;

    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (!g_file_handles[i].in_use) {
            handle_index = i;
            break;
        }
    }
    if (handle_index < 0) {
        return -(int64_t)LINUX_EMFILE;
    }

    fd = allocate_fd_slot(3);
    if (fd < 0) {
        return -(int64_t)LINUX_EMFILE;
    }

    local_memset(&g_file_handles[handle_index], 0, sizeof(g_file_handles[handle_index]));
    g_file_handles[handle_index].in_use = 1;
    g_file_handles[handle_index].refcount = 1;
    g_file_handles[handle_index].offset = 0;
    g_file_handles[handle_index].open_flags = open_flags;
    local_memcpy(&g_file_handles[handle_index].node, node, sizeof(*node));

    current_fd_table()[fd].kind = FD_KIND_VNODE;
    current_fd_table()[fd].handle_index = handle_index;
    return fd;
}

static int64_t install_device_fd(uint8_t kind, int32_t device_id) {
    int fd = allocate_fd_slot(3);

    if (fd < 0) {
        return -(int64_t)LINUX_EMFILE;
    }

    current_fd_table()[fd].kind = kind;
    current_fd_table()[fd].handle_index = device_id;
    return fd;
}

static uint16_t allocate_socket_port(void) {
    uint16_t port = g_socket_next_port++;

    if (g_socket_next_port < SOCKET_TCP_PORT_FIRST || g_socket_next_port > SOCKET_TCP_PORT_LAST) {
        g_socket_next_port = SOCKET_TCP_PORT_FIRST;
    }
    return port;
}

static int64_t install_socket_fd(int family, int type, int protocol) {
    int socket_index = -1;
    int fd = -1;

    if (family != LINUX_AF_INET || type != LINUX_SOCK_STREAM) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (protocol != 0 && protocol != LINUX_IPPROTO_TCP) {
        return -(int64_t)LINUX_EINVAL;
    }

    for (int i = 0; i < MAX_SOCKET_OBJECTS; i++) {
        if (!g_socket_objects[i].in_use) {
            socket_index = i;
            break;
        }
    }
    if (socket_index < 0) {
        return -(int64_t)LINUX_EMFILE;
    }

    fd = allocate_fd_slot(3);
    if (fd < 0) {
        return -(int64_t)LINUX_EMFILE;
    }

    local_memset(&g_socket_objects[socket_index], 0, sizeof(g_socket_objects[socket_index]));
    g_socket_objects[socket_index].in_use = 1;
    g_socket_objects[socket_index].state = SOCKET_STATE_CREATED;
    g_socket_objects[socket_index].family = (uint8_t)family;
    g_socket_objects[socket_index].type = (uint8_t)type;
    g_socket_objects[socket_index].protocol = LINUX_IPPROTO_TCP;
    g_socket_objects[socket_index].dev_index = 0;
    g_socket_objects[socket_index].refcount = 1;
    g_socket_objects[socket_index].local_port = allocate_socket_port();
    g_socket_objects[socket_index].seq = SOCKET_TCP_BASE_SEQ + (uint32_t)(socket_index * 0x1000U);

    current_fd_table()[fd].kind = FD_KIND_SOCKET;
    current_fd_table()[fd].handle_index = socket_index;
    return fd;
}

static int64_t sys_pipe(struct syscall_regs* regs) {
    int32_t* user_pipefd = (int32_t*)(uintptr_t)regs->rdi;
    int pipe_index = -1;
    int read_fd = -1;
    int write_fd = -1;
    struct fd_entry* table = current_fd_table();

    if (!user_pipefd || !table) return -(int64_t)LINUX_EFAULT;

    for (int i = 0; i < MAX_PIPE_OBJECTS; i++) {
        if (!g_pipe_objects[i].in_use) {
            pipe_index = i;
            break;
        }
    }
    if (pipe_index < 0) return -(int64_t)LINUX_EMFILE;

    read_fd = allocate_fd_slot(3);
    if (read_fd < 0) return -(int64_t)LINUX_EMFILE;
    table[read_fd].kind = FD_KIND_FREE;
    write_fd = allocate_fd_slot(read_fd + 1);
    if (write_fd < 0) return -(int64_t)LINUX_EMFILE;

    local_memset(&g_pipe_objects[pipe_index], 0, sizeof(g_pipe_objects[pipe_index]));
    g_pipe_objects[pipe_index].in_use = 1;
    g_pipe_objects[pipe_index].read_refs = 1;
    g_pipe_objects[pipe_index].write_refs = 1;

    table[read_fd].kind = FD_KIND_PIPE_READER;
    table[read_fd].handle_index = pipe_index;
    table[write_fd].kind = FD_KIND_PIPE_WRITER;
    table[write_fd].handle_index = pipe_index;

    user_pipefd[0] = read_fd;
    user_pipefd[1] = write_fd;
    return 0;
}

static int64_t sys_poll(struct syscall_regs* regs) {
    struct linux_pollfd* fds = (struct linux_pollfd*)(uintptr_t)regs->rdi;
    uint64_t nfds = regs->rsi;
    int64_t ready = 0;

    (void)regs->rdx;

    if (!fds && nfds > 0) return -(int64_t)LINUX_EFAULT;
    if (nfds > 1024) return -(int64_t)LINUX_EINVAL;

    for (uint64_t i = 0; i < nfds; i++) {
        struct linux_pollfd* pfd = &fds[i];
        struct fd_entry* entry = NULL;
        int16_t revents = 0;

        if (pfd->fd < 0) {
            pfd->revents = 0;
            continue;
        }

        entry = get_fd_entry((uint64_t)pfd->fd);
        if (!entry) {
            pfd->revents = LINUX_POLLNVAL;
            ready++;
            continue;
        }

        if (entry->kind == FD_KIND_STDIN || entry->kind == FD_KIND_TTY) {
            if (pfd->events & LINUX_POLLIN) revents |= LINUX_POLLIN;
            if (pfd->events & LINUX_POLLOUT) revents |= LINUX_POLLOUT;
        } else if (entry->kind == FD_KIND_STDOUT || entry->kind == FD_KIND_STDERR || entry->kind == FD_KIND_VNODE || entry->kind == FD_KIND_NULL) {
            if (pfd->events & LINUX_POLLOUT) revents |= LINUX_POLLOUT;
            if (pfd->events & LINUX_POLLIN) revents |= LINUX_POLLIN;
        } else if (entry->kind == FD_KIND_PIPE_READER) {
            struct pipe_object* pipe = get_pipe_object_by_index(entry->handle_index);
            if (!pipe) {
                revents |= LINUX_POLLERR;
            } else {
                if ((pfd->events & LINUX_POLLIN) && pipe->size > 0) revents |= LINUX_POLLIN;
                if (pipe->write_refs == 0) revents |= LINUX_POLLHUP;
            }
        } else if (entry->kind == FD_KIND_PIPE_WRITER) {
            struct pipe_object* pipe = get_pipe_object_by_index(entry->handle_index);
            if (!pipe || pipe->read_refs == 0) {
                revents |= LINUX_POLLERR;
            } else if ((pfd->events & LINUX_POLLOUT) && pipe->size < sizeof(pipe->buffer)) {
                revents |= LINUX_POLLOUT;
            }
        } else if (entry->kind == FD_KIND_SOCKET) {
            struct socket_object* sock = get_socket_by_index(entry->handle_index);
            if (!sock) {
                revents |= LINUX_POLLERR;
            } else {
                if (pfd->events & LINUX_POLLOUT) revents |= LINUX_POLLOUT;
                if ((pfd->events & LINUX_POLLIN) && sock->rx_off < sock->rx_len) {
                    revents |= LINUX_POLLIN;
                }
                if (sock->state == SOCKET_STATE_CLOSED) revents |= LINUX_POLLHUP;
            }
        }

        pfd->revents = revents;
        if (revents) ready++;
    }

    return ready;
}

static int wait_pid_matches(int64_t requested_pid, uint32_t child_pid) {
    return requested_pid == -1 || requested_pid == (int64_t)child_pid;
}

void syscall_release_fd_table_entries(struct fd_entry* table, size_t count) {
    if (!table) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (table[i].kind == FD_KIND_VNODE) {
            release_file_handle(table[i].handle_index);
        } else if (table[i].kind == FD_KIND_PIPE_READER || table[i].kind == FD_KIND_PIPE_WRITER) {
            release_pipe_ref(table[i].handle_index, table[i].kind);
        } else if (table[i].kind == FD_KIND_SOCKET) {
            close_socket_ref(table[i].handle_index);
        }
        table[i].kind = FD_KIND_FREE;
        table[i].handle_index = -1;
    }
}

static int64_t sys_wait4(struct syscall_regs* regs) {
    int64_t requested_pid = (int64_t)regs->rdi;
    int32_t* status_ptr = (int32_t*)(uintptr_t)regs->rsi;
    uint64_t options = regs->rdx;
    process_t* parent = current_process;
    process_t* first_ready = NULL;
    process_t* zombie = NULL;
    int has_matching_child = 0;

    if (!parent) return -(int64_t)LINUX_ECHILD;
    if (options != 0) return -(int64_t)LINUX_EINVAL;
    if (requested_pid == 0 || requested_pid < -1) return -(int64_t)LINUX_EINVAL;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* child = &process_list[i];
        if (child->status == PROCESS_STATUS_DEAD) continue;
        if (child->parent_pid != parent->pid) continue;
        if (!wait_pid_matches(requested_pid, child->pid)) continue;
        has_matching_child = 1;
        if (child->status == PROCESS_STATUS_ZOMBIE) {
            zombie = child;
            break;
        }
        if (!first_ready && child->status == PROCESS_STATUS_READY) {
            first_ready = child;
        }
    }

    if (zombie) {
        uint32_t child_pid = zombie->pid;
        if (status_ptr) {
            *status_ptr = zombie->exit_status;
        }
        local_memset(zombie, 0, sizeof(*zombie));
        zombie->status = PROCESS_STATUS_DEAD;
        return (int64_t)child_pid;
    }

    if (!has_matching_child) {
        return -(int64_t)LINUX_ECHILD;
    }
    if (!first_ready) {
        return -(int64_t)LINUX_ECHILD;
    }

    local_memcpy(&parent->regs, regs, sizeof(*regs));
    parent->status = PROCESS_STATUS_WAITING;
    parent->wait_target_pid = requested_pid;
    parent->wait_status_ptr = status_ptr;

    current_process = first_ready;
    first_ready->status = PROCESS_STATUS_RUNNING;
    switch_to_process(first_ready);
    return -(int64_t)LINUX_EINTR;
}

static void release_process_memory(process_t* proc) {
    if (!proc || !proc->p4_table) return;

    vmm_free_user_space(proc->p4_table);
    if (proc->p4_table != p4_table) {
        pmm_free_block(proc->p4_table);
    }
    proc->p4_table = NULL;
}

static void process_exit_and_wake_parent(int exit_code) {
    process_t* child = current_process;
    process_t* parent = NULL;
    uint32_t child_pid;
    int32_t raw_status;

    if (!child) {
        halt_forever();
    }

    child_pid = child->pid;
    raw_status = (exit_code & 0xFF) << 8;
    syscall_release_fd_table_entries(child->fd_table, PROCESS_FD_MAX);

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status == PROCESS_STATUS_DEAD) continue;
        if (process_list[i].pid == child->parent_pid) {
            parent = &process_list[i];
            break;
        }
    }

    if (parent && parent->status == PROCESS_STATUS_WAITING &&
        wait_pid_matches(parent->wait_target_pid, child_pid)) {
        if (parent->wait_status_ptr) {
            switch_page_table(parent->p4_table);
            *parent->wait_status_ptr = raw_status;
        }
        switch_page_table(parent->p4_table);
        release_process_memory(child);
        parent->regs.rax = child_pid;
        parent->status = PROCESS_STATUS_RUNNING;
        parent->wait_target_pid = -1;
        parent->wait_status_ptr = NULL;
        local_memset(child, 0, sizeof(*child));
        child->status = PROCESS_STATUS_DEAD;
        current_process = parent;
        switch_to_process(parent);
    }

    child->status = PROCESS_STATUS_ZOMBIE;
    child->exit_status = raw_status;

    if (parent && parent->status != PROCESS_STATUS_DEAD && parent->status != PROCESS_STATUS_ZOMBIE) {
        current_process = parent;
        switch_page_table(parent->p4_table);
        release_process_memory(child);
        if (parent->status == PROCESS_STATUS_READY || parent->status == PROCESS_STATUS_WAITING) {
            parent->status = PROCESS_STATUS_RUNNING;
            parent->wait_target_pid = -1;
            parent->wait_status_ptr = NULL;
        }
        switch_to_process(parent);
    }

    halt_forever();
}

void syscall_kill_current_process(int exit_code) {
    if (current_process && current_process->pid != 1) {
        process_exit_and_wake_parent(exit_code);
    }
}

static int64_t dup_fd_common(uint64_t oldfd, int64_t requested_newfd, int overwrite) {
    struct fd_entry* old_entry = get_fd_entry(oldfd);
    int target_fd = -1;

    if (!old_entry) {
        return -(int64_t)LINUX_EBADF;
    }

    if (requested_newfd >= 0) {
        if ((uint64_t)requested_newfd >= PROCESS_FD_MAX) {
            return -(int64_t)LINUX_EBADF;
        }
        if ((uint64_t)requested_newfd == oldfd) {
            return requested_newfd;
        }
        target_fd = (int)requested_newfd;
        if (overwrite) {
            close_fd_internal((uint64_t)target_fd);
        } else if (current_fd_table()[target_fd].kind != FD_KIND_FREE) {
            return -(int64_t)LINUX_EMFILE;
        }
    } else {
        target_fd = allocate_fd_slot(0);
        if (target_fd < 0) {
            return -(int64_t)LINUX_EMFILE;
        }
    }

    current_fd_table()[target_fd] = *old_entry;
    retain_fd_entry_refs(&current_fd_table()[target_fd]);
    if (old_entry->kind == FD_KIND_VNODE) {
        struct file_handle* handle = get_file_handle_by_index(old_entry->handle_index);
        if (!handle) {
            current_fd_table()[target_fd].kind = FD_KIND_FREE;
            current_fd_table()[target_fd].handle_index = -1;
            return -(int64_t)LINUX_EBADF;
        }
    }
    return target_fd;
}

static int64_t resolve_path_from_dirfd(int64_t dirfd, const char* path, char* out, size_t out_size) {
    struct file_handle* dir = NULL;
    const char* base = "";

    if (!path || !out || out_size == 0) return -(int64_t)LINUX_EFAULT;
    if (dirfd == LINUX_AT_FDCWD) {
        base = process_get_cwd();
    } else {
        dir = get_vnode_handle((uint64_t)dirfd);
        if (!dir) return -(int64_t)LINUX_EBADF;
        if (dir->node.type != VFS_NODE_TYPE_DIRECTORY) return -(int64_t)LINUX_ENOTDIR;
        base = dir->node.path;
    }

    if (canonicalize_path(base, path, out, out_size) != 0) {
        return -(int64_t)LINUX_E2BIG;
    }
    return 0;
}

static int64_t open_path_with_flags(const char* path, uint64_t flags) {
    struct vfs_node node;
    int lookup_rc = 0;
    uint64_t access_mode = flags & LINUX_O_ACCMODE;
    int mutates = (access_mode == LINUX_O_WRONLY || access_mode == LINUX_O_RDWR ||
                   (flags & (LINUX_O_CREAT | LINUX_O_TRUNC)) != 0);

    if (!path) return -(int64_t)LINUX_EFAULT;

    lookup_rc = vfs_lookup(path, &node);
    if (lookup_rc != 0) {
        if (mutates && !user_can_mutate_path(path)) {
            return -(int64_t)LINUX_EACCES;
        }
        if ((flags & LINUX_O_CREAT) == 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if ((flags & LINUX_O_DIRECTORY) != 0) {
            return -(int64_t)LINUX_EINVAL;
        }
        if (fat32_is_fat32_path(path)) {
            uint8_t is_dir = 0;
            uint32_t first_cluster = 0;
            uint32_t size = 0;
            if (fat32_create_path(path, &is_dir, &first_cluster, &size) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            if (vfs_lookup(path, &node) != 0) {
                return -(int64_t)LINUX_EIO;
            }
        } else if (ext4_is_ext4_path(path)) {
            uint8_t is_dir = 0;
            uint32_t inode = 0;
            uint32_t size = 0;
            if (ext4_create_path(path, &is_dir, &inode, &size) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            if (vfs_lookup(path, &node) != 0) {
                return -(int64_t)LINUX_EIO;
            }
        } else if (tmpfs_create_path(path, &node) == 0) {
            /* tmpfs filled the vnode. */
        } else if (aosfs_create_path(path, &node) != 0) {
            return -(int64_t)LINUX_EACCES;
        }
        return install_vnode_fd(&node, flags);
    }

    if (flags & LINUX_O_DIRECTORY) {
        if (node.type != VFS_NODE_TYPE_DIRECTORY) return -(int64_t)LINUX_ENOTDIR;
        return install_vnode_fd(&node, flags);
    }
    if (node.type == VFS_NODE_TYPE_CHAR_DEVICE) {
        if (flags & (LINUX_O_CREAT | LINUX_O_EXCL | LINUX_O_TRUNC)) {
            return -(int64_t)LINUX_EACCES;
        }
        if (node.u.first_cluster == VFS_DEV_NULL) {
            return install_device_fd(FD_KIND_NULL, (int32_t)node.u.first_cluster);
        }
        if (node.u.first_cluster == VFS_DEV_CONSOLE || node.u.first_cluster == VFS_DEV_TTY0) {
            return install_device_fd(FD_KIND_TTY, (int32_t)node.u.first_cluster);
        }
        return -(int64_t)LINUX_ENOENT;
    }
    if (node.type == VFS_NODE_TYPE_DIRECTORY) {
        return -(int64_t)LINUX_EISDIR;
    }

    if (mutates && !user_can_mutate_path(path)) {
        return -(int64_t)LINUX_EACCES;
    }

    if (node.backend != VFS_BACKEND_AOSFS && node.backend != VFS_BACKEND_TMPFS && node.backend != VFS_BACKEND_FAT32 && node.backend != VFS_BACKEND_EXT4) {
        if ((flags & LINUX_O_ACCMODE) == LINUX_O_WRONLY || (flags & LINUX_O_ACCMODE) == LINUX_O_RDWR) {
            return -(int64_t)LINUX_EACCES;
        }
        if (flags & (LINUX_O_CREAT | LINUX_O_EXCL | LINUX_O_TRUNC)) {
            return -(int64_t)LINUX_EACCES;
        }
    } else if (node.backend == VFS_BACKEND_TMPFS) {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if ((flags & LINUX_O_TRUNC) && tmpfs_truncate_path(path) == 0) {
            node.size = 0;
        }
    } else if (node.backend == VFS_BACKEND_AOSFS) {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if ((flags & LINUX_O_TRUNC) && aosfs_truncate_path(path) == 0) {
            node.size = 0;
        }
    } else if (node.backend == VFS_BACKEND_FAT32) {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if (flags & LINUX_O_TRUNC) {
            uint32_t first_cluster = 0;
            uint32_t size = 0;
            if (fat32_truncate_path(path, &first_cluster, &size) == 0) {
                node.u.first_cluster = first_cluster;
                node.size = size;
            }
        }
    } else {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if (flags & LINUX_O_TRUNC) {
            uint32_t size = 0;
            if (ext4_truncate_path(path, &size) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            node.size = size;
        }
    }

    return install_vnode_fd(&node, flags);
}

static void fill_linux_stat(struct linux_stat* st, uint64_t inode_seed, uint32_t size, uint32_t mode) {
    local_memset(st, 0, sizeof(*st));
    st->st_dev = 1;
    st->st_ino = inode_seed ? inode_seed : 1;
    st->st_nlink = 1;
    st->st_mode = mode;
    st->st_rdev = 0;
    st->st_size = (int64_t)size;
    st->st_blksize = 512;
    st->st_blocks = (int64_t)((size + 511U) / 512U);
}

static uint8_t* stack_va_to_ptr(uint8_t* stack_pages[USER_STACK_PAGES], uint64_t va) {
    if (va < USER_STACK_BASE || va >= USER_STACK_BASE + USER_STACK_SIZE) return NULL;
    uint64_t stack_offset = va - USER_STACK_BASE;
    uint64_t page_index = stack_offset / 4096ULL;
    uint64_t page_offset = stack_offset & 0xFFFULL;
    if (page_index >= USER_STACK_PAGES || !stack_pages[page_index]) return NULL;
    return stack_pages[page_index] + page_offset;
}

static int64_t push_u64(uint8_t* stack_pages[USER_STACK_PAGES], uint64_t* sp, uint64_t value) {
    if (*sp < USER_STACK_BASE + 8) return -(int64_t)LINUX_E2BIG;
    *sp -= 8;
    uint64_t* p = (uint64_t*)stack_va_to_ptr(stack_pages, *sp);
    if (!p) return -(int64_t)LINUX_EFAULT;
    *p = value;
    return 0;
}

static int64_t push_auxv_pair(uint8_t* stack_pages[USER_STACK_PAGES], uint64_t* sp, uint64_t key, uint64_t value) {
    int64_t rc = push_u64(stack_pages, sp, value);
    if (rc < 0) return rc;
    return push_u64(stack_pages, sp, key);
}

static int64_t copy_string_to_stack(uint8_t* stack_pages[USER_STACK_PAGES], uint64_t* sp, const char* src, uint64_t* out_user_ptr) {
    size_t len = 0;
    while (src[len] != '\0') {
        len++;
        if (len >= MAX_EXEC_STRING) return -(int64_t)LINUX_E2BIG;
    }
    len++;

    if (*sp < USER_STACK_BASE + len) return -(int64_t)LINUX_E2BIG;
    *sp -= len;

    for (size_t i = 0; i < len; i++) {
        uint8_t* dst = stack_va_to_ptr(stack_pages, *sp + i);
        if (!dst) return -(int64_t)LINUX_EFAULT;
        *dst = (uint8_t)src[i];
    }
    *out_user_ptr = *sp;
    return 0;
}

static int64_t snapshot_user_string_array(
    const uint64_t* user_ptrs,
    char storage[][MAX_EXEC_STRING],
    const char** kernel_ptrs,
    size_t max_count,
    size_t* out_count
) {
    size_t count = 0;

    if (!kernel_ptrs || !out_count) {
        return -(int64_t)LINUX_EFAULT;
    }
    *out_count = 0;

    if (!user_ptrs) {
        return 0;
    }

    for (size_t i = 0; i < max_count; i++) {
        uint64_t uptr = user_ptrs[i];
        if (uptr == 0) {
            break;
        }

        int64_t rc = copy_user_cstr((const char*)(uintptr_t)uptr, storage[count], MAX_EXEC_STRING);
        if (rc < 0) {
            return rc;
        }
        kernel_ptrs[count] = storage[count];
        count++;
    }

    *out_count = count;
    return 0;
}

static int64_t build_exec_stack(
    uint8_t* stack_pages[USER_STACK_PAGES],
    uint64_t* out_rsp,
    const char* const* argv_kernel,
    size_t argc,
    const char* const* envp_kernel,
    size_t envc,
    const char* fallback_argv0,
    uint64_t entry,
    uint64_t phdr_va,
    uint64_t phent_size,
    uint64_t phnum
) {
    enum {
        AUXV_AT_NULL = 0,
        AUXV_AT_PHDR = 3,
        AUXV_AT_PHENT = 4,
        AUXV_AT_PHNUM = 5,
        AUXV_AT_PAGESZ = 6,
        AUXV_AT_BASE = 7,
        AUXV_AT_FLAGS = 8,
        AUXV_AT_ENTRY = 9,
        AUXV_AT_UID = 11,
        AUXV_AT_EUID = 12,
        AUXV_AT_GID = 13,
        AUXV_AT_EGID = 14,
        AUXV_AT_SECURE = 23,
        AUXV_AT_RANDOM = 25,
        AUXV_AT_EXECFN = 31,
    };
    uint64_t sp = USER_STACK_BASE + USER_STACK_SIZE;
    uint64_t arg_ptrs[MAX_EXEC_ARGS];
    uint64_t env_ptrs[MAX_EXEC_ENVP];
    uint64_t random_ptr = 0;

    if (argv_kernel) {
        for (size_t i = 0; i < argc; i++) {
            int64_t rc = copy_string_to_stack(stack_pages, &sp, argv_kernel[i], &arg_ptrs[i]);
            if (rc < 0) return rc;
        }
    }

    if (argc == 0) {
        int64_t rc = copy_string_to_stack(stack_pages, &sp, fallback_argv0, &arg_ptrs[0]);
        if (rc < 0) return rc;
        argc = 1;
    }

    if (envp_kernel) {
        for (size_t i = 0; i < envc; i++) {
            int64_t rc = copy_string_to_stack(stack_pages, &sp, envp_kernel[i], &env_ptrs[i]);
            if (rc < 0) return rc;
        }
    }

    if (sp < USER_STACK_BASE + 16) return -(int64_t)LINUX_E2BIG;
    sp -= 16;
    for (size_t i = 0; i < 16; i++) {
        uint8_t* dst = stack_va_to_ptr(stack_pages, sp + i);
        if (!dst) return -(int64_t)LINUX_EFAULT;
        *dst = 0;
    }
    random_ptr = sp;

    sp &= ~0xFULL;

    int64_t rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_NULL, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_EXECFN, arg_ptrs[0]);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_RANDOM, random_ptr);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_SECURE, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_EGID, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_GID, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_EUID, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_UID, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_ENTRY, entry);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_FLAGS, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_BASE, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_PAGESZ, 4096);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_PHNUM, phnum);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_PHENT, phent_size);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_PHDR, phdr_va);
    if (rc < 0) return rc;

    rc = push_u64(stack_pages, &sp, 0);
    if (rc < 0) return rc;
    for (size_t i = envc; i > 0; i--) {
        rc = push_u64(stack_pages, &sp, env_ptrs[i - 1]);
        if (rc < 0) return rc;
    }

    rc = push_u64(stack_pages, &sp, 0);
    if (rc < 0) return rc;
    for (size_t i = argc; i > 0; i--) {
        rc = push_u64(stack_pages, &sp, arg_ptrs[i - 1]);
        if (rc < 0) return rc;
    }

    rc = push_u64(stack_pages, &sp, argc);
    if (rc < 0) return rc;

    *out_rsp = sp;
    return 0;
}

struct exec_elf64_ehdr {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct exec_elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed));

#define EXEC_ELF_TYPE_DYN 3
#define EXEC_ELF_PT_LOAD 1
#define EXEC_USER_ELF_DYN_BASE 0x0000700001000000ULL

static uint64_t exec_align_down_4k(uint64_t value) {
    return value & ~0xFFFULL;
}

static int64_t compute_phdr_user_va(const struct exec_elf64_ehdr* ehdr, const struct exec_elf64_phdr* phdrs, uint64_t load_bias, uint64_t* out) {
    if (!ehdr || !phdrs || !out) {
        return -(int64_t)LINUX_EFAULT;
    }

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const struct exec_elf64_phdr* phdr = &phdrs[i];
        if (phdr->p_type != EXEC_ELF_PT_LOAD || phdr->p_filesz == 0) continue;
        if (ehdr->e_phoff < phdr->p_offset) continue;
        if (ehdr->e_phoff >= phdr->p_offset + phdr->p_filesz) continue;

        *out = load_bias + phdr->p_vaddr + (ehdr->e_phoff - phdr->p_offset);
        return 0;
    }

    return -(int64_t)LINUX_EINVAL;
}

static int64_t exec_initrd_program(
    const char* normalized,
    const uint64_t* argv_user,
    const uint64_t* envp_user
) {
    process_t* proc = get_current_process();
    uint64_t* user_p4 = proc ? proc->p4_table : p4_table;
    char argv_storage[MAX_EXEC_ARGS][MAX_EXEC_STRING];
    char envp_storage[MAX_EXEC_ENVP][MAX_EXEC_STRING];
    const char* argv_kernel[MAX_EXEC_ARGS];
    const char* envp_kernel[MAX_EXEC_ENVP];
    size_t argc = 0;
    size_t envc = 0;

    if (!normalized || *normalized == '\0') {
        return -(int64_t)LINUX_ENOENT;
    }

    int64_t rc = snapshot_user_string_array(argv_user, argv_storage, argv_kernel, MAX_EXEC_ARGS, &argc);
    if (rc < 0) return rc;
    rc = snapshot_user_string_array(envp_user, envp_storage, envp_kernel, MAX_EXEC_ENVP, &envc);
    if (rc < 0) return rc;

    const uint8_t* elf_data = NULL;
    uint32_t elf_size = 0;
    struct vfs_node program_node;
    uint64_t phdr_va = 0;
    uint64_t load_bias = 0;
    if (vfs_lookup(normalized, &program_node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (program_node.type != VFS_NODE_TYPE_REGULAR) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (program_node.backend != VFS_BACKEND_AOSFS && program_node.backend != VFS_BACKEND_INITRD) {
        return -(int64_t)LINUX_EACCES;
    }
    elf_data = program_node.u.data;
    elf_size = program_node.size;

    if (elf_size < sizeof(struct exec_elf64_ehdr)) {
        return -(int64_t)LINUX_EINVAL;
    }

    const struct exec_elf64_ehdr* ehdr = (const struct exec_elf64_ehdr*)elf_data;
    uint64_t image_end = 0;
    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * sizeof(struct exec_elf64_phdr) > elf_size) {
        return -(int64_t)LINUX_EINVAL;
    }
    const struct exec_elf64_phdr* phdrs = (const struct exec_elf64_phdr*)(elf_data + ehdr->e_phoff);
    if (ehdr->e_type == EXEC_ELF_TYPE_DYN) {
        uint64_t min_vaddr = UINT64_MAX;
        int found_load = 0;
        for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
            if (phdrs[i].p_type != EXEC_ELF_PT_LOAD || phdrs[i].p_memsz == 0) continue;
            uint64_t seg_start = exec_align_down_4k(phdrs[i].p_vaddr);
            if (seg_start < min_vaddr) min_vaddr = seg_start;
            found_load = 1;
        }
        if (!found_load || EXEC_USER_ELF_DYN_BASE < min_vaddr) {
            return -(int64_t)LINUX_EINVAL;
        }
        load_bias = EXEC_USER_ELF_DYN_BASE - min_vaddr;
    }
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        uint64_t seg_end = 0;
        if (phdrs[i].p_type != EXEC_ELF_PT_LOAD || phdrs[i].p_memsz == 0) continue;
        seg_end = load_bias + phdrs[i].p_vaddr + phdrs[i].p_memsz;
        if (seg_end > image_end) {
            image_end = seg_end;
        }
    }
    rc = compute_phdr_user_va(ehdr, phdrs, load_bias, &phdr_va);
    if (rc < 0) return rc;

    vmm_free_user_space(user_p4);

    uint64_t entry = 0;
    if (elf64_load_image(user_p4, elf_data, elf_size, &entry) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }

    uint8_t* stack_pages[USER_STACK_PAGES];
    for (size_t i = 0; i < USER_STACK_PAGES; i++) {
        stack_pages[i] = (uint8_t*)pmm_alloc_block();
        if (!stack_pages[i]) return -(int64_t)LINUX_EIO;
        local_memset(stack_pages[i], 0, 4096);
        vmm_map_page(
            user_p4,
            USER_STACK_BASE + i * 4096ULL,
            (uint64_t)stack_pages[i],
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
        );
    }

    uint64_t new_rsp = 0;
    rc = build_exec_stack(
        stack_pages,
        &new_rsp,
        argv_kernel,
        argc,
        envp_kernel,
        envc,
        normalized,
        entry,
        phdr_va,
        ehdr->e_phentsize,
        ehdr->e_phnum
    );
    if (rc < 0) return rc;

    if (proc) {
        proc->fs_base = 0;
        proc->brk_base = align_up_page(image_end);
        proc->brk_current = proc->brk_base;
        proc->brk_mapped_end = proc->brk_base;
        proc->mmap_next = USER_MMAP_BASE;
        proc->clear_child_tid = 0;
    }
    process_load_fs_base(0);

    serial_print("AOS: execve -> ");
    serial_print(normalized);
    serial_print("\n");

    input_clear_events();
    asm volatile("swapgs");
    jump_to_user(entry, new_rsp);
    return 0;
}

static int map_zeroed_user_pages(uint64_t* user_p4, uint64_t start, uint64_t end, uint64_t page_flags) {
    for (uint64_t va = start; va < end; va += 4096ULL) {
        void* phys = pmm_alloc_block();
        if (!phys) {
            return -1;
        }
        local_memset(phys, 0, 4096);
        vmm_map_page(user_p4, va, (uint64_t)phys, page_flags);
    }
    return 0;
}

static int64_t sys_brk(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t requested = regs->rdi;
    uint64_t target = 0;

    if (!proc || !proc->p4_table) {
        return 0;
    }

    if (proc->brk_base == 0) {
        proc->brk_base = align_up_page(0x450000ULL);
        proc->brk_current = proc->brk_base;
        proc->brk_mapped_end = proc->brk_base;
    }

    if (requested == 0) {
        return (int64_t)proc->brk_current;
    }
    if (requested < proc->brk_base) {
        return (int64_t)proc->brk_current;
    }

    target = align_up_page(requested);
    if (target > proc->brk_mapped_end) {
        if (map_zeroed_user_pages(proc->p4_table, proc->brk_mapped_end, target, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
            return (int64_t)proc->brk_current;
        }
        proc->brk_mapped_end = target;
    } else if (target < proc->brk_mapped_end) {
        for (uint64_t va = target; va < proc->brk_mapped_end; va += 4096ULL) {
            uint64_t phys = vmm_unmap_page(proc->p4_table, va);
            if (phys) {
                pmm_free_block((void*)phys);
            }
        }
        proc->brk_mapped_end = target;
    }

    proc->brk_current = requested;
    return (int64_t)proc->brk_current;
}

static int64_t sys_mmap(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t addr = regs->rdi;
    uint64_t len = regs->rsi;
    uint64_t prot = regs->rdx;
    uint64_t flags = regs->r10;
    int64_t fd = (int64_t)regs->r8;
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    uint64_t base = 0;
    uint64_t end = 0;

    (void)regs->r9;

    if (!proc || !proc->p4_table || len == 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((flags & LINUX_MAP_PRIVATE) == 0 || (flags & LINUX_MAP_ANONYMOUS) == 0 || fd != -1) {
        return -(int64_t)LINUX_EINVAL;
    }

    len = align_up_page(len);
    if (prot & LINUX_PROT_WRITE) {
        page_flags |= PAGE_WRITABLE;
    }

    if (addr != 0) {
        base = addr & ~0xFFFULL;
    } else {
        if (proc->mmap_next == 0) {
            proc->mmap_next = USER_MMAP_BASE;
        }
        base = proc->mmap_next;
    }
    end = base + len;

    if (map_zeroed_user_pages(proc->p4_table, base, end, page_flags) != 0) {
        return -(int64_t)LINUX_EIO;
    }

    if (addr == 0) {
        proc->mmap_next = end;
    }
    return (int64_t)base;
}

static int64_t sys_mprotect(struct syscall_regs* regs) {
    (void)regs;
    return 0;
}

static int64_t sys_munmap(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t addr = regs->rdi;
    uint64_t len = regs->rsi;
    uint64_t start = addr & ~0xFFFULL;
    uint64_t end = align_up_page(addr + len);

    if (!proc || !proc->p4_table || len == 0 || end < start) {
        return -(int64_t)LINUX_EINVAL;
    }

    for (uint64_t va = start; va < end; va += 4096ULL) {
        uint64_t phys = vmm_unmap_page(proc->p4_table, va);
        if (phys) {
            pmm_free_block((void*)phys);
        }
    }
    return 0;
}

static int64_t sys_set_tid_address(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    if (proc) {
        proc->clear_child_tid = regs->rdi;
        return (int64_t)proc->pid;
    }
    return 1;
}

static int64_t sys_write(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    struct fd_entry* entry = get_fd_entry(fd);
    const char* buf = (const char*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;

    if (!entry) return -(int64_t)LINUX_EBADF;
    if (!buf) return -(int64_t)LINUX_EFAULT;
    if (entry->kind == FD_KIND_PIPE_WRITER) {
        struct pipe_object* pipe = get_pipe_for_fd(fd, FD_KIND_PIPE_WRITER);
        uint64_t bytes_written = 0;
        if (!pipe) return -(int64_t)LINUX_EBADF;
        while (bytes_written < len && pipe->size < sizeof(pipe->buffer)) {
            pipe->buffer[pipe->write_pos] = (uint8_t)buf[bytes_written++];
            pipe->write_pos = (pipe->write_pos + 1) % sizeof(pipe->buffer);
            pipe->size++;
        }
        return (int64_t)bytes_written;
    }
    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        uint64_t bytes_written = 0;
        uint32_t new_size = 0;

        if (!file) return -(int64_t)LINUX_EBADF;
        if (file->node.type != VFS_NODE_TYPE_REGULAR) return -(int64_t)LINUX_EISDIR;
        if (file->node.backend != VFS_BACKEND_AOSFS && file->node.backend != VFS_BACKEND_TMPFS && file->node.backend != VFS_BACKEND_FAT32 && file->node.backend != VFS_BACKEND_EXT4) return -(int64_t)LINUX_EACCES;
        if (!user_can_mutate_path(file->node.path)) return -(int64_t)LINUX_EACCES;
        if (vfs_write_node(&file->node, file->offset, (const uint8_t*)buf, len, &bytes_written, &new_size) != 0) {
            return -(int64_t)LINUX_EIO;
        }
        file->offset += bytes_written;
        file->node.size = new_size;
        return (int64_t)bytes_written;
    }
    if (entry->kind == FD_KIND_NULL) {
        return (int64_t)len;
    }
    if (entry->kind == FD_KIND_SOCKET) {
        return socket_send_data(get_socket_by_index(entry->handle_index),
                                (const uint8_t*)buf,
                                len);
    }
    if (entry->kind != FD_KIND_STDOUT && entry->kind != FD_KIND_STDERR && entry->kind != FD_KIND_TTY) {
        return -(int64_t)LINUX_EBADF;
    }

    return (int64_t)tty_write(buf, len);
}

static int is_tty_fd_kind(uint8_t kind) {
    return kind == FD_KIND_STDIN || kind == FD_KIND_STDOUT || kind == FD_KIND_STDERR || kind == FD_KIND_TTY;
}

static void fill_default_termios(struct linux_termios* term) {
    local_memset(term, 0, sizeof(*term));
    term->c_iflag = LINUX_TERMIOS_ICRNL;
    term->c_oflag = LINUX_TERMIOS_OPOST;
    term->c_cflag = LINUX_TERMIOS_CREAD | LINUX_TERMIOS_CS8;
    term->c_lflag = LINUX_TERMIOS_ISIG | LINUX_TERMIOS_ICANON | LINUX_TERMIOS_ECHO |
        LINUX_TERMIOS_ECHOE | LINUX_TERMIOS_ECHOK | LINUX_TERMIOS_IEXTEN;
    term->c_cc[0] = 3;     /* VINTR: Ctrl-C */
    term->c_cc[1] = 28;    /* VQUIT: Ctrl-\ */
    term->c_cc[2] = 127;   /* VERASE */
    term->c_cc[3] = 21;    /* VKILL: Ctrl-U */
    term->c_cc[4] = 4;     /* VEOF: Ctrl-D */
    term->c_cc[5] = 0;     /* VTIME */
    term->c_cc[6] = 1;     /* VMIN */
    term->c_cc[8] = 17;    /* VSTART: Ctrl-Q */
    term->c_cc[9] = 19;    /* VSTOP: Ctrl-S */
    term->c_cc[10] = 26;   /* VSUSP: Ctrl-Z */
}

static int64_t sys_ioctl(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    uint64_t request = regs->rsi;
    void* arg = (void*)(uintptr_t)regs->rdx;
    struct fd_entry* entry = get_fd_entry(fd);

    if (!entry) return -(int64_t)LINUX_EBADF;

    switch (request) {
        case LINUX_TCGETS:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            fill_default_termios((struct linux_termios*)arg);
            return 0;
        case LINUX_TCSETS:
        case LINUX_TCSETSW:
        case LINUX_TCSETSF:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            return 0;
        case LINUX_TIOCGWINSZ:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            tty_get_winsize(&((struct linux_winsize*)arg)->ws_row,
                            &((struct linux_winsize*)arg)->ws_col);
            ((struct linux_winsize*)arg)->ws_xpixel = 0;
            ((struct linux_winsize*)arg)->ws_ypixel = 0;
            return 0;
        case LINUX_TIOCSWINSZ:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            return 0;
        case LINUX_FIONREAD:
            if (!arg) return -(int64_t)LINUX_EFAULT;
            *(int*)(uintptr_t)arg = (int)tty_pending();
            return 0;
        case LINUX_TIOCGPGRP:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            *(int*)(uintptr_t)arg = 1;
            return 0;
        case LINUX_TIOCSPGRP:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            return 0;
        default:
            return -(int64_t)LINUX_ENOTTY;
    }
}

static int64_t sys_writev(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    const struct linux_iovec* iov = (const struct linux_iovec*)(uintptr_t)regs->rsi;
    uint64_t iovcnt = regs->rdx;
    int64_t total = 0;

    if (!iov && iovcnt != 0) return -(int64_t)LINUX_EFAULT;
    if (iovcnt > 64) return -(int64_t)LINUX_EINVAL;

    for (uint64_t i = 0; i < iovcnt; i++) {
        struct syscall_regs write_regs = *regs;
        if (!iov[i].iov_base && iov[i].iov_len != 0) return -(int64_t)LINUX_EFAULT;
        write_regs.rdi = fd;
        write_regs.rsi = iov[i].iov_base;
        write_regs.rdx = iov[i].iov_len;
        int64_t rc = sys_write(&write_regs);
        if (rc < 0) {
            return total > 0 ? total : rc;
        }
        total += rc;
        if ((uint64_t)rc != iov[i].iov_len) {
            break;
        }
    }

    return total;
}

static int64_t sys_readv(struct syscall_regs* regs);

extern void keyboard_handler_main();

static int64_t sys_open(struct syscall_regs* regs) {
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    return open_path_with_flags(resolved_path, (uint64_t)regs->rsi);
}

static int64_t sys_access(struct syscall_regs* regs) {
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (regs->rsi & ~(LINUX_R_OK | LINUX_W_OK | LINUX_X_OK)) {
        return -(int64_t)LINUX_EINVAL;
    }
    {
        struct vfs_node node;
        if (vfs_lookup(resolved_path, &node) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if ((regs->rsi & LINUX_W_OK) && node.backend != VFS_BACKEND_AOSFS && node.backend != VFS_BACKEND_TMPFS && node.backend != VFS_BACKEND_FAT32 && node.backend != VFS_BACKEND_EXT4) {
            return -(int64_t)LINUX_EACCES;
        }
    }
    return 0;
}

static int64_t sys_openat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    return open_path_with_flags(resolved_path, regs->rdx);
}

static int64_t sys_mkdirat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node existing;
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    (void)regs->rdx;
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (vfs_lookup(resolved_path, &existing) == 0) {
        return -(int64_t)LINUX_EEXIST;
    }
    if (!user_can_mutate_path(resolved_path)) {
        return -(int64_t)LINUX_EACCES;
    }
    if (aosfs_mkdir_path(resolved_path) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

static int64_t sys_mkdir(struct syscall_regs* regs) {
    struct syscall_regs mkdirat_regs = *regs;
    mkdirat_regs.rdi = (uint64_t)LINUX_AT_FDCWD;
    mkdirat_regs.rsi = regs->rdi;
    mkdirat_regs.rdx = regs->rsi;
    return sys_mkdirat(&mkdirat_regs);
}

static int64_t sys_unlinkat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc;

    if (regs->rdx != 0 && regs->rdx != LINUX_AT_REMOVEDIR) {
        return -(int64_t)LINUX_EINVAL;
    }

    rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (!user_can_mutate_path(resolved_path)) {
        return -(int64_t)LINUX_EACCES;
    }
    if (regs->rdx == LINUX_AT_REMOVEDIR) {
        if (vfs_rmdir_path(resolved_path) != 0) {
            return -(int64_t)LINUX_EACCES;
        }
        return 0;
    }
    if (vfs_unlink_path(resolved_path) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

static int64_t sys_faccessat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (regs->rdx & ~(LINUX_R_OK | LINUX_W_OK | LINUX_X_OK)) {
        return -(int64_t)LINUX_EINVAL;
    }
    {
        struct vfs_node node;
        if (vfs_lookup(resolved_path, &node) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if ((regs->rdx & LINUX_W_OK) && node.backend != VFS_BACKEND_AOSFS && node.backend != VFS_BACKEND_TMPFS && node.backend != VFS_BACKEND_FAT32 && node.backend != VFS_BACKEND_EXT4) {
            return -(int64_t)LINUX_EACCES;
        }
    }
    return 0;
}

static int64_t sys_read(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    struct fd_entry* entry = get_fd_entry(fd);
    char* buf = (char*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;

    if (!buf) return -(int64_t)LINUX_EFAULT;
    if (!entry) return -(int64_t)LINUX_EBADF;

    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        if (!file) return -(int64_t)LINUX_EBADF;
        if (file->node.type != VFS_NODE_TYPE_REGULAR) return -(int64_t)LINUX_EISDIR;

        if (file->offset >= file->node.size) {
            return 0;
        }

        uint64_t available = (uint64_t)file->node.size - file->offset;
        if (len > available) {
            len = available;
        }

        if (vfs_read_node(&file->node, file->offset, (uint8_t*)buf, len) != 0) {
            return -(int64_t)LINUX_EIO;
        }
        file->offset += len;
        return (int64_t)len;
    }
    if (entry->kind == FD_KIND_PIPE_READER) {
        struct pipe_object* pipe = get_pipe_for_fd(fd, FD_KIND_PIPE_READER);
        uint64_t bytes_read = 0;
        if (!pipe) return -(int64_t)LINUX_EBADF;
        while (bytes_read < len && pipe->size > 0) {
            buf[bytes_read++] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % sizeof(pipe->buffer);
            pipe->size--;
        }
        return (int64_t)bytes_read;
    }
    if (entry->kind == FD_KIND_NULL) return 0;
    if (entry->kind == FD_KIND_SOCKET) {
        return socket_recv_data(get_socket_by_index(entry->handle_index),
                                (uint8_t*)buf,
                                len);
    }
    if (entry->kind != FD_KIND_STDIN && entry->kind != FD_KIND_TTY) return -(int64_t)LINUX_EBADF;
    if (len == 0) return 0;

    return (int64_t)tty_read(buf, len);
}

static int64_t sys_readv(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    const struct linux_iovec* iov = (const struct linux_iovec*)(uintptr_t)regs->rsi;
    uint64_t iovcnt = regs->rdx;
    int64_t total = 0;

    if (!iov && iovcnt != 0) return -(int64_t)LINUX_EFAULT;
    if (iovcnt > 64) return -(int64_t)LINUX_EINVAL;

    for (uint64_t i = 0; i < iovcnt; i++) {
        struct syscall_regs read_regs = *regs;
        if (!iov[i].iov_base && iov[i].iov_len != 0) return -(int64_t)LINUX_EFAULT;
        read_regs.rdi = fd;
        read_regs.rsi = iov[i].iov_base;
        read_regs.rdx = iov[i].iov_len;
        int64_t rc = sys_read(&read_regs);
        if (rc < 0) {
            return total > 0 ? total : rc;
        }
        total += rc;
        if ((uint64_t)rc != iov[i].iov_len) {
            break;
        }
    }

    return total;
}

static int64_t sys_lseek(struct syscall_regs* regs) {
    struct file_handle* file = get_vnode_handle(regs->rdi);
    int64_t offset = (int64_t)regs->rsi;
    uint64_t whence = regs->rdx;
    int64_t base = 0;

    if (!file) return -(int64_t)LINUX_EBADF;
    if (file->node.type != VFS_NODE_TYPE_REGULAR) return -(int64_t)LINUX_EINVAL;

    switch (whence) {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = (int64_t)file->offset;
            break;
        case SEEK_END:
            base = (int64_t)file->node.size;
            break;
        default:
            return -(int64_t)LINUX_EINVAL;
    }

    int64_t new_offset = base + offset;
    if (new_offset < 0) {
        return -(int64_t)LINUX_EINVAL;
    }

    file->offset = (uint64_t)new_offset;
    return new_offset;
}

static int64_t sys_newfstatat(struct syscall_regs* regs);

static int64_t sys_fstat(struct syscall_regs* regs) {
    struct fd_entry* entry = get_fd_entry(regs->rdi);
    struct linux_stat* st = (struct linux_stat*)(uintptr_t)regs->rsi;

    if (!entry) return -(int64_t)LINUX_EBADF;
    if (!st) return -(int64_t)LINUX_EFAULT;

    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        if (!file) return -(int64_t)LINUX_EBADF;
        if (file->node.type == VFS_NODE_TYPE_DIRECTORY) {
            uint64_t inode_seed = file->node.inode;
            fill_linux_stat(st, inode_seed, 0, LINUX_S_IFDIR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        } else {
            fill_linux_stat(st, file->node.inode, file->node.size, LINUX_S_IFREG | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        }
        return 0;
    }
    if (entry->kind == FD_KIND_PIPE_READER || entry->kind == FD_KIND_PIPE_WRITER) {
        fill_linux_stat(st, regs->rdi + 1, 0, LINUX_S_IFIFO | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        return 0;
    }

    fill_linux_stat(st, regs->rdi + 1, 0, LINUX_S_IFCHR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
    return 0;
}

static int64_t sys_stat(struct syscall_regs* regs) {
    struct syscall_regs statat_regs = *regs;
    statat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    statat_regs.rsi = regs->rdi;
    statat_regs.rdx = regs->rsi;
    statat_regs.r10 = 0;
    return sys_newfstatat(&statat_regs);
}

static int64_t sys_newfstatat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    const char* path = (const char*)(uintptr_t)regs->rsi;
    struct linux_stat* st = (struct linux_stat*)(uintptr_t)regs->rdx;
    uint64_t flags = regs->r10;
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = 0;

    if (!st) return -(int64_t)LINUX_EFAULT;

    if (path && path[0] == '\0' && (flags & LINUX_AT_EMPTY_PATH)) {
        struct fd_entry* entry = get_fd_entry((uint64_t)dirfd);
        if (!entry) return -(int64_t)LINUX_EBADF;
        if (entry->kind == FD_KIND_VNODE) {
            struct file_handle* file = get_file_handle_by_index(entry->handle_index);
            if (!file) return -(int64_t)LINUX_EBADF;
            if (file->node.type == VFS_NODE_TYPE_DIRECTORY) {
                uint64_t inode_seed = file->node.inode;
                fill_linux_stat(st, inode_seed, 0, LINUX_S_IFDIR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
            } else {
                fill_linux_stat(st, file->node.inode, file->node.size, LINUX_S_IFREG | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
            }
        } else {
            uint32_t mode = (entry->kind == FD_KIND_PIPE_READER || entry->kind == FD_KIND_PIPE_WRITER)
                ? (LINUX_S_IFIFO | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH)
                : (LINUX_S_IFCHR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
            fill_linux_stat(st, (uint64_t)dirfd + 1, 0, mode);
        }
        return 0;
    }

    if (!path) return -(int64_t)LINUX_EFAULT;

    rc = resolve_path_from_dirfd(dirfd, path, resolved_path, sizeof(resolved_path));
    if (rc < 0) {
        return rc;
    }

    {
        struct vfs_node node;
        if (vfs_lookup(resolved_path, &node) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if (node.type == VFS_NODE_TYPE_DIRECTORY) {
            fill_linux_stat(st, node.inode, 0, LINUX_S_IFDIR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        } else if (node.type == VFS_NODE_TYPE_CHAR_DEVICE) {
            fill_linux_stat(st, node.inode, 0, LINUX_S_IFCHR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        } else {
            fill_linux_stat(st, node.inode, node.size, LINUX_S_IFREG | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        }
    }

    return 0;
}

static int64_t sys_getdents64(struct syscall_regs* regs) {
    struct file_handle* file = get_vnode_handle(regs->rdi);
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;
    uint64_t bytes_written = 0;

    if (!file) return -(int64_t)LINUX_EBADF;
    if (!buf) return -(int64_t)LINUX_EFAULT;
    if (file->node.type != VFS_NODE_TYPE_DIRECTORY) return -(int64_t)LINUX_ENOTDIR;

    while (1) {
        char name[64];
        uint32_t entry_size = 0;
        uint8_t d_type = LINUX_DTYPE_REG;
        uint64_t logical_index = file->offset;

        if (vfs_dirent_at(&file->node, logical_index, name, sizeof(name), &entry_size, &d_type) != 0) {
            break;
        }

        uint64_t name_len = 0;
        while (name[name_len] != '\0') {
            name_len++;
        }

        uint16_t reclen = (uint16_t)((offsetof(struct linux_dirent64, d_name) + name_len + 2 + 7) & ~7ULL);
        if (bytes_written + reclen > len) {
            break;
        }

        struct linux_dirent64* ent = (struct linux_dirent64*)(buf + bytes_written);
        ent->d_ino = logical_index + 1;
        ent->d_off = (int64_t)(logical_index + 1);
        ent->d_reclen = reclen;
        ent->d_type = d_type;

        for (uint64_t i = 0; i < name_len; i++) {
            ent->d_name[i] = name[i];
        }
        ent->d_name[name_len] = '\0';

        uint64_t pad_start = offsetof(struct linux_dirent64, d_name) + name_len + 1;
        while (pad_start < reclen) {
            ((uint8_t*)ent)[pad_start++] = 0;
        }

        bytes_written += reclen;
        file->offset = logical_index + 1;
    }

    return (int64_t)bytes_written;
}

static int64_t sys_close(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    if (!get_fd_entry(fd)) return -(int64_t)LINUX_EBADF;
    close_fd_internal(fd);
    return 0;
}

static int64_t sys_getpid(struct syscall_regs* regs) {
    (void)regs;
    process_t* proc = get_current_process();
    return proc ? (int64_t)proc->pid : 1;
}

static int64_t sys_getppid(struct syscall_regs* regs) {
    (void)regs;
    process_t* proc = get_current_process();
    if (!proc || proc->parent_pid == 0) {
        return 1;
    }
    return (int64_t)proc->parent_pid;
}

static uint16_t sock_get_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t sock_get_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void sock_put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void sock_put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t sock_checksum_add(uint32_t sum, const uint8_t* data, uint16_t len) {
    while (len > 1) {
        sum += (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
        data += 2;
        len -= 2;
    }
    if (len) {
        sum += (uint16_t)((uint16_t)data[0] << 8);
    }
    return sum;
}

static uint16_t sock_checksum_finish(uint32_t sum) {
    while (sum >> 16) {
        sum = (sum & 0xffffU) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static int sock_same_bytes(const uint8_t* a, const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static int sock_ipv4_same_subnet(const uint8_t a[4], const uint8_t b[4], uint8_t prefix) {
    uint32_t av;
    uint32_t bv;
    uint32_t mask;

    if (prefix == 0) return 0;
    if (prefix > 32) return 0;
    av = ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | a[3];
    bv = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    mask = prefix == 32 ? 0xffffffffU : (0xffffffffU << (32 - prefix));
    return (av & mask) == (bv & mask);
}

static uint16_t sock_ipv4_checksum(uint8_t* ip) {
    ip[10] = 0;
    ip[11] = 0;
    return sock_checksum_finish(sock_checksum_add(0, ip, 20));
}

static uint16_t sock_tcp_checksum(const struct netdev* dev,
                                  const struct socket_object* sock,
                                  const uint8_t* tcp,
                                  uint16_t tcp_len) {
    uint32_t sum = 0;
    sum = sock_checksum_add(sum, dev->ipv4_addr, 4);
    sum = sock_checksum_add(sum, sock->remote_ip, 4);
    sum += LINUX_IPPROTO_TCP;
    sum += tcp_len;
    sum = sock_checksum_add(sum, tcp, tcp_len);
    return sock_checksum_finish(sum);
}

static int sock_copy_user_name(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0 || !src) return -1;
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    if (i == 0 || src[i] != '\0') return -1;
    return 0;
}

static int sock_name_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int dns_cache_lookup(uint8_t dev_index, const char* name, uint8_t out_ip[4]) {
    uint64_t now = timer_get_ticks();

    for (size_t i = 0; i < SOCKET_DNS_CACHE_ENTRIES; i++) {
        if (!g_dns_cache[i].valid) continue;
        if (g_dns_cache[i].dev_index != dev_index) continue;
        if (g_dns_cache[i].expires_at <= now) {
            g_dns_cache[i].valid = 0;
            continue;
        }
        if (sock_name_equal(g_dns_cache[i].name, name)) {
            local_memcpy(out_ip, g_dns_cache[i].ipv4, 4);
            g_dns_cache[i].hits++;
            return 1;
        }
    }
    return 0;
}

static void dns_cache_store(uint8_t dev_index, const char* name, const uint8_t ip[4]) {
    size_t slot = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    for (size_t i = 0; i < SOCKET_DNS_CACHE_ENTRIES; i++) {
        if (!g_dns_cache[i].valid ||
            (g_dns_cache[i].dev_index == dev_index && sock_name_equal(g_dns_cache[i].name, name))) {
            slot = i;
            goto store;
        }
        if (g_dns_cache[i].expires_at < oldest) {
            oldest = g_dns_cache[i].expires_at;
            slot = i;
        }
    }

store:
    local_memset(&g_dns_cache[slot], 0, sizeof(g_dns_cache[slot]));
    g_dns_cache[slot].valid = 1;
    g_dns_cache[slot].dev_index = dev_index;
    g_dns_cache[slot].expires_at = now + SOCKET_DNS_CACHE_TTL_TICKS;
    sock_copy_user_name(g_dns_cache[slot].name, sizeof(g_dns_cache[slot].name), name);
    local_memcpy(g_dns_cache[slot].ipv4, ip, 4);
}

static int arp_cache_lookup(uint8_t dev_index, const uint8_t ip[4], uint8_t out_mac[6]) {
    uint64_t now = timer_get_ticks();

    if (!ip || !out_mac) return 0;
    for (size_t i = 0; i < SOCKET_ARP_CACHE_ENTRIES; i++) {
        if (!g_arp_cache[i].valid) continue;
        if (g_arp_cache[i].dev_index != dev_index) continue;
        if (g_arp_cache[i].expires_at <= now) {
            g_arp_cache[i].valid = 0;
            continue;
        }
        if (sock_same_bytes(g_arp_cache[i].ipv4, ip, 4)) {
            local_memcpy(out_mac, g_arp_cache[i].mac, 6);
            g_arp_cache[i].hits++;
            return 1;
        }
    }
    return 0;
}

static void arp_cache_store(uint8_t dev_index, const uint8_t ip[4], const uint8_t mac[6]) {
    size_t slot = 0;
    uint64_t oldest = UINT64_MAX;
    uint64_t now = timer_get_ticks();

    if (!ip || !mac) return;
    for (size_t i = 0; i < SOCKET_ARP_CACHE_ENTRIES; i++) {
        if (!g_arp_cache[i].valid ||
            (g_arp_cache[i].dev_index == dev_index && sock_same_bytes(g_arp_cache[i].ipv4, ip, 4))) {
            slot = i;
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
    g_arp_cache[slot].expires_at = now + SOCKET_ARP_CACHE_TTL_TICKS;
    local_memcpy(g_arp_cache[slot].ipv4, ip, 4);
    local_memcpy(g_arp_cache[slot].mac, mac, 6);
}

static uint16_t sock_encode_dns_qname(uint8_t* out, const char* name) {
    uint8_t* start = out;

    while (*name) {
        uint8_t* len = out++;
        uint8_t count = 0;
        while (*name && *name != '.') {
            if (count >= 63) return 0;
            *out++ = (uint8_t)*name++;
            count++;
        }
        if (count == 0) return 0;
        *len = count;
        if (*name == '.') name++;
    }
    *out++ = 0;
    return (uint16_t)(out - start);
}

static uint16_t sock_build_dns_query(const struct netdev* dev,
                                     const uint8_t dns_mac[6],
                                     const char* name,
                                     uint8_t* frame) {
    uint8_t* ip = frame + 14;
    uint8_t* udp = frame + 34;
    uint8_t* dns = frame + 42;
    uint8_t* qname = dns + 12;
    uint16_t qlen;
    uint16_t dns_len;
    uint16_t udp_len;
    uint16_t ip_len;
    uint16_t ip_sum;

    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    local_memcpy(frame, dns_mac, 6);
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;

    ip[0] = 0x45;
    ip[8] = 64;
    ip[9] = 17;
    local_memcpy(ip + 12, dev->ipv4_addr, 4);
    local_memcpy(ip + 16, dev->ipv4_dns, 4);

    sock_put_be16(udp + 0, SOCKET_DNS_PORT);
    sock_put_be16(udp + 2, 53);

    sock_put_be16(dns + 0, SOCKET_DNS_TXID);
    sock_put_be16(dns + 2, 0x0100);
    sock_put_be16(dns + 4, 1);
    qlen = sock_encode_dns_qname(qname, name);
    if (qlen == 0) return 0;
    sock_put_be16(qname + qlen, 1);
    sock_put_be16(qname + qlen + 2, 1);

    dns_len = (uint16_t)(12 + qlen + 4);
    udp_len = (uint16_t)(8 + dns_len);
    ip_len = (uint16_t)(20 + udp_len);
    sock_put_be16(udp + 4, udp_len);
    sock_put_be16(ip + 2, ip_len);
    ip_sum = sock_ipv4_checksum(ip);
    sock_put_be16(ip + 10, ip_sum);
    return (uint16_t)(14 + ip_len);
}

static const uint8_t* sock_skip_dns_name(const uint8_t* p, const uint8_t* end) {
    while (p < end) {
        uint8_t len = *p++;
        if (len == 0) return p;
        if ((len & 0xc0) == 0xc0) {
            if (p >= end) return end;
            return p + 1;
        }
        p += len;
    }
    return end;
}

static void sock_decode_dns_name(const uint8_t* msg,
                                 const uint8_t* p,
                                 const uint8_t* end,
                                 char* out,
                                 size_t out_size) {
    size_t out_i = 0;
    uint32_t jumps = 0;

    if (!out || out_size == 0) return;
    while (p < end && jumps < 24 && out_i + 1 < out_size) {
        uint8_t len = *p++;
        if (len == 0) break;
        if ((len & 0xc0) == 0xc0) {
            uint16_t off;
            if (p >= end) break;
            off = (uint16_t)(((uint16_t)(len & 0x3f) << 8) | *p);
            p = msg + off;
            jumps++;
            continue;
        }
        if (out_i && out_i + 1 < out_size) out[out_i++] = '.';
        while (len-- && p < end && out_i + 1 < out_size) {
            out[out_i++] = (char)*p++;
        }
    }
    out[out_i] = '\0';
}

static int sock_parse_dns_response(const struct netdev* dev,
                                   const uint8_t* frame,
                                   int len,
                                   uint8_t out_ip[4],
                                   char cname[SOCKET_DNS_NAME_MAX]) {
    const uint8_t* msg;
    const uint8_t* p;
    const uint8_t* end = frame + len;
    uint16_t qd;
    uint16_t an;

    if (len < 60) return 0;
    if (frame[12] != 0x08 || frame[13] != 0x00) return 0;
    if (frame[23] != 17) return 0;
    if (!sock_same_bytes(frame + 26, dev->ipv4_dns, 4)) return 0;
    if (!sock_same_bytes(frame + 30, dev->ipv4_addr, 4)) return 0;
    if (sock_get_be16(frame + 34) != 53) return 0;
    if (sock_get_be16(frame + 36) != SOCKET_DNS_PORT) return 0;

    msg = frame + 42;
    if (sock_get_be16(msg) != SOCKET_DNS_TXID) return 0;
    if ((msg[2] & 0x80) == 0) return 0;
    if ((msg[3] & 0x0f) != 0) return -1;

    qd = sock_get_be16(msg + 4);
    an = sock_get_be16(msg + 6);
    p = msg + 12;
    for (uint16_t i = 0; i < qd; i++) {
        p = sock_skip_dns_name(p, end);
        if (p + 4 > end) return 0;
        p += 4;
    }
    for (uint16_t i = 0; i < an; i++) {
        uint16_t type;
        uint16_t klass;
        uint16_t rdlen;
        p = sock_skip_dns_name(p, end);
        if (p + 10 > end) return 0;
        type = sock_get_be16(p);
        klass = sock_get_be16(p + 2);
        rdlen = sock_get_be16(p + 8);
        p += 10;
        if (p + rdlen > end) return 0;
        if (klass == 1 && type == 1 && rdlen == 4) {
            local_memcpy(out_ip, p, 4);
            return 1;
        }
        if (klass == 1 && type == 5 && cname) {
            sock_decode_dns_name(msg, p, end, cname, SOCKET_DNS_NAME_MAX);
        }
        p += rdlen;
    }
    if (cname && cname[0]) return 2;
    return -1;
}

static uint16_t sock_build_arp_request(const struct netdev* dev,
                                       const uint8_t target_ip[4],
                                       uint8_t* frame) {
    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    for (size_t i = 0; i < 6; i++) frame[i] = 0xff;
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x08;
    frame[13] = 0x06;
    sock_put_be16(frame + 14, 1);
    sock_put_be16(frame + 16, 0x0800);
    frame[18] = 6;
    frame[19] = 4;
    sock_put_be16(frame + 20, 1);
    local_memcpy(frame + 22, dev->mac, 6);
    local_memcpy(frame + 28, dev->ipv4_addr, 4);
    local_memcpy(frame + 38, target_ip, 4);
    return 42;
}

static int sock_parse_arp_reply(const uint8_t* frame,
                                int len,
                                const uint8_t target_ip[4],
                                const uint8_t local_ip[4],
                                uint8_t out_mac[6]) {
    if (len < 42) return 0;
    if (frame[12] != 0x08 || frame[13] != 0x06) return 0;
    if (sock_get_be16(frame + 20) != 2) return 0;
    if (!sock_same_bytes(frame + 28, target_ip, 4)) return 0;
    if (!sock_same_bytes(frame + 38, local_ip, 4)) return 0;
    local_memcpy(out_mac, frame + 22, 6);
    return 1;
}

static int sock_arp_lookup(struct socket_object* sock, const struct netdev* dev) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint16_t len;

    if (arp_cache_lookup(sock->dev_index, sock->next_hop_ip, sock->remote_mac)) {
        return 0;
    }

    len = sock_build_arp_request(dev, sock->next_hop_ip, tx);
    if (netdev_send(sock->dev_index, tx, len) < 0) {
        return -1;
    }
    for (int i = 0; i < 400000; i++) {
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (sock_parse_arp_reply(rx, n, sock->next_hop_ip, dev->ipv4_addr, sock->remote_mac)) {
            arp_cache_store(sock->dev_index, sock->next_hop_ip, sock->remote_mac);
            return 0;
        }
    }
    return -1;
}

static uint16_t sock_build_tcp_frame(struct socket_object* sock,
                                     uint8_t flags,
                                     const uint8_t* payload,
                                     uint16_t payload_len,
                                     uint8_t* frame) {
    const struct netdev* dev = netdev_get(sock->dev_index);
    uint8_t* ip = frame + 14;
    uint8_t* tcp = frame + 34;
    uint16_t tcp_len = (uint16_t)(20 + payload_len);
    uint16_t total_len = (uint16_t)(20 + tcp_len);
    uint16_t ip_sum;
    uint16_t tcp_sum;

    local_memset(frame, 0, SOCKET_TX_FRAME_SIZE);
    local_memcpy(frame, sock->remote_mac, 6);
    local_memcpy(frame + 6, dev->mac, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;

    ip[0] = 0x45;
    ip[1] = 0;
    sock_put_be16(ip + 2, total_len);
    sock_put_be16(ip + 4, 0xa055);
    ip[8] = 64;
    ip[9] = LINUX_IPPROTO_TCP;
    local_memcpy(ip + 12, dev->ipv4_addr, 4);
    local_memcpy(ip + 16, sock->remote_ip, 4);
    ip_sum = sock_ipv4_checksum(ip);
    sock_put_be16(ip + 10, ip_sum);

    sock_put_be16(tcp + 0, sock->local_port);
    sock_put_be16(tcp + 2, sock->remote_port);
    sock_put_be32(tcp + 4, sock->seq);
    sock_put_be32(tcp + 8, sock->ack);
    tcp[12] = 0x50;
    tcp[13] = flags;
    sock_put_be16(tcp + 14, 64240);
    if (payload_len && payload) {
        local_memcpy(tcp + 20, payload, payload_len);
    }
    tcp_sum = sock_tcp_checksum(dev, sock, tcp, tcp_len);
    sock_put_be16(tcp + 16, tcp_sum);
    return (uint16_t)(14 + total_len);
}

static int sock_parse_tcp_frame(struct socket_object* sock,
                                const uint8_t* frame,
                                int len,
                                uint8_t* flags,
                                uint32_t* seq,
                                uint32_t* ack,
                                const uint8_t** payload,
                                uint16_t* payload_len) {
    const struct netdev* dev = netdev_get(sock->dev_index);
    uint16_t ip_len;
    uint16_t ip_hlen;
    uint16_t tcp_hlen;

    if (!dev) return 0;
    if (len < 54) return 0;
    if (frame[12] != 0x08 || frame[13] != 0x00) return 0;
    if (frame[23] != LINUX_IPPROTO_TCP) return 0;
    if (!sock_same_bytes(frame + 26, sock->remote_ip, 4)) return 0;
    if (!sock_same_bytes(frame + 30, dev->ipv4_addr, 4)) return 0;
    if (sock_get_be16(frame + 34) != sock->remote_port) return 0;
    if (sock_get_be16(frame + 36) != sock->local_port) return 0;
    ip_hlen = (uint16_t)((frame[14] & 0x0f) * 4);
    if (ip_hlen < 20) return 0;
    tcp_hlen = (uint16_t)((frame[14 + ip_hlen + 12] >> 4) * 4);
    if (tcp_hlen < 20) return 0;
    ip_len = sock_get_be16(frame + 16);
    if (ip_len < ip_hlen + tcp_hlen) return 0;
    *flags = frame[14 + ip_hlen + 13];
    *seq = sock_get_be32(frame + 14 + ip_hlen + 4);
    if (ack) *ack = sock_get_be32(frame + 14 + ip_hlen + 8);
    *payload = frame + 14 + ip_hlen + tcp_hlen;
    *payload_len = (uint16_t)(ip_len - ip_hlen - tcp_hlen);
    return 1;
}

static int sock_wait_tcp_ticks(struct socket_object* sock,
                               uint8_t want_flags,
                               uint64_t timeout_ticks,
                               uint8_t* out_flags,
                               uint32_t* out_seq,
                               uint32_t* out_ack,
                               const uint8_t** out_payload,
                               uint16_t* out_payload_len) {
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start = timer_get_ticks();

    if (timeout_ticks == 0) timeout_ticks = 1;

    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        if ((want_flags == 0x12 && (flags & 0x12) == 0x12) ||
            (want_flags != 0x12 && (payload_len > 0 || (flags & 0x01) || (flags & 0x04)))) {
            if (out_flags) *out_flags = flags;
            if (out_seq) *out_seq = seq;
            if (out_ack) *out_ack = ack;
            if (out_payload) *out_payload = payload;
            if (out_payload_len) *out_payload_len = payload_len;
            return 0;
        }
    }
    return -1;
}

static int sock_wait_tcp(struct socket_object* sock,
                         uint8_t want_flags,
                         uint8_t* out_flags,
                         uint32_t* out_seq,
                         uint32_t* out_ack,
                         const uint8_t** out_payload,
                         uint16_t* out_payload_len) {
    uint32_t frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;
    return sock_wait_tcp_ticks(sock,
                               want_flags,
                               (uint64_t)frequency * 8ULL,
                               out_flags,
                               out_seq,
                               out_ack,
                               out_payload,
                               out_payload_len);
}

static int socket_send_ack(struct socket_object* sock) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint16_t len = sock_build_tcp_frame(sock, 0x10, NULL, 0, tx);
    return netdev_send(sock->dev_index, tx, len);
}

static void socket_store_rx_payload(struct socket_object* sock,
                                    const uint8_t* payload,
                                    uint16_t payload_len,
                                    uint32_t seq) {
    uint16_t store = payload_len;

    if (!sock || !payload || payload_len == 0) {
        return;
    }
    if (store > SOCKET_RX_BUFFER_SIZE) store = SOCKET_RX_BUFFER_SIZE;
    local_memcpy(sock->rx_buffer, payload, store);
    sock->rx_len = store;
    sock->rx_off = 0;
    sock->ack = seq + payload_len;
    socket_send_ack(sock);
}

static int sock_wait_ack(struct socket_object* sock, uint32_t expected_ack, uint64_t timeout_ticks) {
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start = timer_get_ticks();

    if (timeout_ticks == 0) timeout_ticks = 1;

    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) return -1;
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        if (flags & 0x04) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
        if (payload_len > 0) {
            socket_store_rx_payload(sock, payload, payload_len, seq);
        }
        if ((int32_t)(ack - expected_ack) >= 0) {
            return 0;
        }
        if (flags & 0x01) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
    }
    return -1;
}

static int socket_send_fin_close(struct socket_object* sock) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    static uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint64_t start;
    uint64_t timeout_ticks;
    uint32_t frequency = timer_get_frequency();
    uint32_t fin_seq;

    if (!sock || sock->state != SOCKET_STATE_CONNECTED) {
        return -1;
    }

    fin_seq = sock->seq;
    {
        uint16_t len = sock_build_tcp_frame(sock, 0x11, NULL, 0, tx);
        if (netdev_send(sock->dev_index, tx, len) < 0) {
            return -1;
        }
    }
    sock->seq++;

    if (frequency == 0) frequency = 100;
    timeout_ticks = (uint64_t)frequency * 2ULL;
    start = timer_get_ticks();
    while (timer_get_ticks() - start < timeout_ticks) {
        uint8_t flags = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        const uint8_t* payload = NULL;
        uint16_t payload_len = 0;
        int n = netdev_recv(sock->dev_index, rx, sizeof(rx));
        if (n < 0) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
        if (!sock_parse_tcp_frame(sock, rx, n, &flags, &seq, &ack, &payload, &payload_len)) {
            continue;
        }
        if (payload_len > 0) {
            sock->ack = seq + payload_len;
            socket_send_ack(sock);
            continue;
        }
        if ((flags & 0x01) != 0) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            return 0;
        }
        if ((flags & 0x10) != 0 && ack == fin_seq + 1) {
            sock->state = SOCKET_STATE_CLOSED;
            return 0;
        }
        if ((flags & 0x04) != 0) {
            sock->state = SOCKET_STATE_CLOSED;
            return -1;
        }
    }

    sock->state = SOCKET_STATE_CLOSED;
    return -1;
}

static int64_t sys_socket(struct syscall_regs* regs) {
    return install_socket_fd((int)regs->rdi, (int)regs->rsi, (int)regs->rdx);
}

static int64_t sys_socket_bind_netdev(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    uint64_t index = regs->rsi;
    const struct netdev* dev;

    if (!sock) return -(int64_t)LINUX_EBADF;
    if (index > 255U) return -(int64_t)LINUX_EINVAL;
    dev = netdev_get((size_t)index);
    if (!dev) return -(int64_t)LINUX_ENODEV;
    if (sock->state != SOCKET_STATE_CREATED) return -(int64_t)LINUX_EINVAL;
    sock->dev_index = (uint8_t)index;
    return 0;
}

static int64_t sys_connect(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    const struct linux_sockaddr_in* addr = (const struct linux_sockaddr_in*)(uintptr_t)regs->rsi;
    uint64_t addr_len = regs->rdx;
    const struct netdev* dev;
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t flags = 0;
    uint32_t reply_seq = 0;
    uint32_t reply_ack = 0;
    const uint8_t* payload = NULL;
    uint16_t payload_len = 0;

    if (!sock) return -(int64_t)LINUX_EBADF;
    if (!addr || addr_len < sizeof(*addr)) return -(int64_t)LINUX_EFAULT;
    if (addr->sin_family != LINUX_AF_INET) return -(int64_t)LINUX_EINVAL;
    dev = netdev_get(sock->dev_index);
    if (!dev || !dev->link_up || !dev->ipv4_configured) return -(int64_t)LINUX_ENODEV;

    local_memcpy(sock->remote_ip, addr->sin_addr, 4);
    sock->remote_port = sock_get_be16((const uint8_t*)&addr->sin_port);
    if (sock_ipv4_same_subnet(sock->remote_ip, dev->ipv4_addr, dev->ipv4_prefix)) {
        local_memcpy(sock->next_hop_ip, sock->remote_ip, 4);
    } else {
        local_memcpy(sock->next_hop_ip, dev->ipv4_gateway, 4);
    }
    if (sock_arp_lookup(sock, dev) != 0) {
        serial_print("socket: ARP failed\n");
        return -(int64_t)LINUX_EIO;
    }

    sock->ack = 0;
    for (int attempt = 0; attempt < SOCKET_TCP_RETRIES; attempt++) {
        uint16_t len = sock_build_tcp_frame(sock, 0x02, NULL, 0, tx);
        if (netdev_send(sock->dev_index, tx, len) < 0) {
            serial_print("socket: SYN send failed\n");
            return -(int64_t)LINUX_EIO;
        }
        if (sock_wait_tcp_ticks(sock,
                                0x12,
                                (uint64_t)(timer_get_frequency() ? timer_get_frequency() : 100) * 2ULL,
                                &flags,
                                &reply_seq,
                                &reply_ack,
                                &payload,
                                &payload_len) == 0) {
            break;
        }
        flags = 0;
    }
    if ((flags & 0x12) != 0x12) {
        serial_print("socket: SYN-ACK wait failed\n");
        return -(int64_t)LINUX_EIO;
    }
    (void)payload;
    (void)payload_len;
    if ((flags & 0x12) != 0x12 || reply_ack != sock->seq + 1) {
        serial_print("socket: unexpected TCP flags\n");
        return -(int64_t)LINUX_EIO;
    }
    sock->seq++;
    sock->ack = reply_seq + 1;
    socket_send_ack(sock);
    sock->state = SOCKET_STATE_CONNECTED;
    return 0;
}

static int64_t socket_send_data(struct socket_object* sock, const uint8_t* data, uint64_t len) {
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint64_t sent = 0;

    if (!sock || sock->state != SOCKET_STATE_CONNECTED) return -(int64_t)LINUX_EBADF;
    if (!data && len > 0) return -(int64_t)LINUX_EFAULT;
    while (sent < len) {
        uint16_t chunk = (uint16_t)(len - sent);
        uint32_t seq_before;
        uint32_t expected_ack;
        int acked = 0;
        if (chunk > 1024) chunk = 1024;
        seq_before = sock->seq;
        expected_ack = seq_before + chunk;
        for (int attempt = 0; attempt < SOCKET_TCP_RETRIES; attempt++) {
            uint16_t frame_len = sock_build_tcp_frame(sock, 0x18, data + sent, chunk, tx);
            if (netdev_send(sock->dev_index, tx, frame_len) < 0) {
                return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
            }
            if (sock_wait_ack(sock,
                              expected_ack,
                              (uint64_t)(timer_get_frequency() ? timer_get_frequency() : 100) * 2ULL) == 0) {
                acked = 1;
                break;
            }
            if (sock->state == SOCKET_STATE_CLOSED) {
                return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
            }
        }
        if (!acked) {
            sock->seq = seq_before;
            return sent ? (int64_t)sent : -(int64_t)LINUX_EIO;
        }
        sock->seq = expected_ack;
        sent += chunk;
    }
    return (int64_t)sent;
}

static int64_t socket_recv_data(struct socket_object* sock, uint8_t* dst, uint64_t len) {
    const uint8_t* payload = NULL;
    uint16_t payload_len = 0;
    uint8_t flags = 0;
    uint32_t seq = 0;
    uint32_t ack = 0;
    uint64_t copied = 0;
    int recv_retries = 0;

    if (!sock) return -(int64_t)LINUX_EBADF;
    if (sock->state == SOCKET_STATE_CLOSED && sock->rx_off >= sock->rx_len) return 0;
    if (sock->state != SOCKET_STATE_CONNECTED && sock->state != SOCKET_STATE_CLOSED) return -(int64_t)LINUX_EBADF;
    if (!dst && len > 0) return -(int64_t)LINUX_EFAULT;
    if (len == 0) return 0;

    while (copied < len) {
        if (sock->rx_off < sock->rx_len) {
            uint64_t avail = sock->rx_len - sock->rx_off;
            if (avail > len - copied) avail = len - copied;
            local_memcpy(dst + copied, sock->rx_buffer + sock->rx_off, avail);
            sock->rx_off += (uint32_t)avail;
            copied += avail;
            if (copied > 0) break;
            continue;
        }

        sock->rx_off = 0;
        sock->rx_len = 0;
        if (sock_wait_tcp(sock, 0x10, &flags, &seq, &ack, &payload, &payload_len) != 0) {
            if (sock->state == SOCKET_STATE_CONNECTED && recv_retries < SOCKET_TCP_RECV_RETRIES) {
                recv_retries++;
                (void)socket_send_ack(sock);
                continue;
            }
            break;
        }
        recv_retries = 0;
        (void)ack;
        if (flags & 0x04) {
            sock->state = SOCKET_STATE_CLOSED;
            break;
        }
        if (payload_len > 0 && (flags & 0x01)) {
            uint16_t store = payload_len;
            if (store > SOCKET_RX_BUFFER_SIZE) store = SOCKET_RX_BUFFER_SIZE;
            local_memcpy(sock->rx_buffer, payload, store);
            sock->rx_len = store;
            sock->rx_off = 0;
            sock->ack = seq + payload_len + 1U;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            continue;
        }
        if (payload_len > 0) {
            socket_store_rx_payload(sock, payload, payload_len, seq);
            continue;
        }
        if (flags & 0x01) {
            sock->ack = seq + 1;
            socket_send_ack(sock);
            sock->state = SOCKET_STATE_CLOSED;
            break;
        }
    }
    if (copied == 0 && sock->state == SOCKET_STATE_CONNECTED) {
        return -(int64_t)LINUX_EIO;
    }
    return (int64_t)copied;
}

static int64_t sys_sendto(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    const uint8_t* buf = (const uint8_t*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;
    (void)regs->r10;
    (void)regs->r8;
    (void)regs->r9;
    return socket_send_data(sock, buf, len);
}

static int64_t sys_recvfrom(struct syscall_regs* regs) {
    struct socket_object* sock = get_socket_for_fd(regs->rdi);
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;
    (void)regs->r10;
    (void)regs->r8;
    (void)regs->r9;
    return socket_recv_data(sock, buf, len);
}

static int64_t sys_dns_lookup(struct syscall_regs* regs) {
    const char* user_name = (const char*)(uintptr_t)regs->rdi;
    uint8_t* out_ip = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t requested_dev = regs->rdx;
    const struct netdev* dev;
    struct socket_object dns_sock;
    uint8_t tx[SOCKET_TX_FRAME_SIZE];
    uint8_t rx[SOCKET_RX_FRAME_SIZE];
    uint8_t answer[4];
    char name[SOCKET_DNS_NAME_MAX];
    char current[SOCKET_DNS_NAME_MAX];
    char cname[SOCKET_DNS_NAME_MAX];

    if (!user_name || !out_ip) return -(int64_t)LINUX_EFAULT;
    if (requested_dev > 255U) return -(int64_t)LINUX_EINVAL;
    dev = netdev_get((size_t)requested_dev);
    if (!dev || !dev->link_up || !dev->ipv4_configured) return -(int64_t)LINUX_ENODEV;
    if (dev->ipv4_dns[0] == 0 && dev->ipv4_dns[1] == 0 &&
        dev->ipv4_dns[2] == 0 && dev->ipv4_dns[3] == 0) {
        return -(int64_t)LINUX_ENODEV;
    }
    if (sock_copy_user_name(name, sizeof(name), user_name) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (dns_cache_lookup((uint8_t)requested_dev, name, answer)) {
        local_memcpy(out_ip, answer, 4);
        return 0;
    }
    local_memcpy(current, name, sizeof(current));

    local_memset(&dns_sock, 0, sizeof(dns_sock));
    dns_sock.dev_index = (uint8_t)requested_dev;
    if (sock_ipv4_same_subnet(dev->ipv4_dns, dev->ipv4_addr, dev->ipv4_prefix)) {
        local_memcpy(dns_sock.next_hop_ip, dev->ipv4_dns, 4);
    } else {
        local_memcpy(dns_sock.next_hop_ip, dev->ipv4_gateway, 4);
    }
    if (sock_arp_lookup(&dns_sock, dev) != 0) {
        return -(int64_t)LINUX_EIO;
    }

    for (int depth = 0; depth < 3; depth++) {
        uint16_t query_len;
        int followed_cname = 0;

        cname[0] = '\0';
        query_len = sock_build_dns_query(dev, dns_sock.remote_mac, current, tx);
        if (query_len == 0) return -(int64_t)LINUX_EINVAL;

        for (int attempt = 0; attempt < SOCKET_DNS_RETRIES; attempt++) {
            if (netdev_send((size_t)requested_dev, tx, query_len) < 0) {
                return -(int64_t)LINUX_EIO;
            }

            for (int poll = 0; poll < SOCKET_DNS_RECV_POLLS; poll++) {
                int parsed;
                int n = netdev_recv((size_t)requested_dev, rx, sizeof(rx));
                if (n < 0) return -(int64_t)LINUX_EIO;
                parsed = sock_parse_dns_response(dev, rx, n, answer, cname);
                if (parsed == 1) {
                    dns_cache_store((uint8_t)requested_dev, name, answer);
                    local_memcpy(out_ip, answer, 4);
                    return 0;
                }
                if (parsed == 2) {
                    local_memcpy(current, cname, sizeof(current));
                    followed_cname = 1;
                    break;
                }
                if (parsed < 0) {
                    return -(int64_t)LINUX_ENOENT;
                }
            }
            if (followed_cname) break;
        }
        if (!followed_cname) break;
    }

    return -(int64_t)LINUX_ENOENT;
}

static int64_t sys_rt_sigaction(struct syscall_regs* regs) {
    (void)regs->rdi;
    (void)regs->rsi;
    void* old_action = (void*)(uintptr_t)regs->rdx;
    (void)regs->r10;

    if (old_action) {
        local_memset(old_action, 0, 32);
    }
    return 0;
}

static int64_t sys_rt_sigprocmask(struct syscall_regs* regs) {
    (void)regs->rdi;
    (void)regs->rsi;
    void* old_set = (void*)(uintptr_t)regs->rdx;
    (void)regs->r10;

    if (old_set) {
        local_memset(old_set, 0, 8);
    }
    return 0;
}

static int64_t sys_dup(struct syscall_regs* regs) {
    return dup_fd_common(regs->rdi, -1, 0);
}

static int64_t sys_dup2(struct syscall_regs* regs) {
    return dup_fd_common(regs->rdi, (int64_t)regs->rsi, 1);
}

static int64_t sys_fcntl(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    uint64_t cmd = regs->rsi;
    struct fd_entry* entry = get_fd_entry(fd);

    if (!entry) return -(int64_t)LINUX_EBADF;

    switch (cmd) {
        case LINUX_F_DUPFD:
            return dup_fd_common(fd, -1, 0);
        case LINUX_F_GETFD:
            return 0;
        case LINUX_F_SETFD:
            return 0;
        case LINUX_F_GETFL:
            return 0;
        case LINUX_F_SETFL:
            return 0;
        default:
            return -(int64_t)LINUX_EINVAL;
    }
}

static int64_t sys_execve(struct syscall_regs* regs) {
    const char* path_user = (const char*)(uintptr_t)regs->rdi;
    const uint64_t* argv_user = (const uint64_t*)(uintptr_t)regs->rsi;
    const uint64_t* envp_user = (const uint64_t*)(uintptr_t)regs->rdx;

    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr(path_user, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    const char* normalized = normalize_path(resolved_path);
    if (*normalized == '\0') return -(int64_t)LINUX_ENOENT;
    rc = exec_initrd_program(normalized, argv_user, envp_user);
    return rc;
}

static int64_t sys_getcwd(struct syscall_regs* regs) {
    char* buf = (char*)(uintptr_t)regs->rdi;
    uint64_t size = regs->rsi;
    const char* cwd = process_get_cwd();
    uint64_t cwd_len = 0;

    if (!buf || size == 0) return -(int64_t)LINUX_EFAULT;

    while (cwd[cwd_len] != '\0') {
        cwd_len++;
    }

    if (cwd_len == 0) {
        if (size < 2) return -(int64_t)LINUX_ERANGE;
        buf[0] = '/';
        buf[1] = '\0';
        return 2;
    }

    if (size < cwd_len + 2) return -(int64_t)LINUX_ERANGE;
    buf[0] = '/';
    for (uint64_t i = 0; i < cwd_len; i++) {
        buf[i + 1] = cwd[i];
    }
    buf[cwd_len + 1] = '\0';
    return (int64_t)(cwd_len + 2);
}

static int64_t sys_chdir(struct syscall_regs* regs) {
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node node;
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;
    if (vfs_lookup(resolved_path, &node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (node.type != VFS_NODE_TYPE_DIRECTORY) {
        return -(int64_t)LINUX_ENOTDIR;
    }

    process_set_cwd(resolved_path);
    return 0;
}

static int64_t sys_arch_prctl(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t code = regs->rdi;
    uint64_t addr = regs->rsi;

    if (!proc) {
        return -(int64_t)LINUX_EIO;
    }

    switch (code) {
        case LINUX_ARCH_SET_FS:
            proc->fs_base = addr;
            process_load_fs_base(addr);
            return 0;
        case LINUX_ARCH_GET_FS:
            if (addr == 0) return -(int64_t)LINUX_EFAULT;
            *(uint64_t*)(uintptr_t)addr = proc->fs_base;
            return 0;
        case LINUX_ARCH_SET_GS:
        case LINUX_ARCH_GET_GS:
            return -(int64_t)LINUX_EINVAL;
        default:
            return -(int64_t)LINUX_EINVAL;
    }
}

static int64_t sys_uname(struct syscall_regs* regs) {
    struct linux_utsname* uts = (struct linux_utsname*)(uintptr_t)regs->rdi;
    if (!uts) return -(int64_t)LINUX_EFAULT;

    for (size_t i = 0; i < sizeof(struct linux_utsname); i++) {
        ((char*)uts)[i] = 0;
    }

    set_uts_field(uts->sysname, "AOS");
    set_uts_field(uts->nodename, "oppeko-aos");
    set_uts_field(uts->release, "0.1");
    set_uts_field(uts->version, "x86_64");
    set_uts_field(uts->machine, "x86_64");
    set_uts_field(uts->domainname, "localdomain");
    return 0;
}

static int64_t sys_clock_gettime(struct syscall_regs* regs) {
    struct linux_timespec* ts = (struct linux_timespec*)(uintptr_t)regs->rsi;
    (void)regs->rdi;
    if (!ts) return -(int64_t)LINUX_EFAULT;
    ts->tv_sec = 0;
    ts->tv_nsec = 0;
    return 0;
}

static int64_t sys_getrandom(struct syscall_regs* regs) {
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rdi;
    uint64_t len = regs->rsi;
    (void)regs->rdx;
    if (!buf && len != 0) return -(int64_t)LINUX_EFAULT;
    for (uint64_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    }
    return (int64_t)len;
}

static int64_t sys_prlimit64(struct syscall_regs* regs) {
    void* old_limit = (void*)(uintptr_t)regs->r10;
    (void)regs->rdi;
    (void)regs->rsi;
    (void)regs->rdx;
    if (old_limit) {
        local_memset(old_limit, 0, 16);
    }
    return 0;
}

static int64_t sys_partition_info(struct syscall_regs* regs) {
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

struct aos_dns_cache_info_user {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t ttl_ticks;
    uint8_t ipv4[4];
    uint8_t reserved2[4];
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
    uint8_t remote_ip[4];
    uint8_t next_hop_ip[4];
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

static int64_t sys_blkdev_info(struct syscall_regs* regs) {
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

static int64_t sys_mem_info(struct syscall_regs* regs) {
    struct aos_mem_info_user* out = (struct aos_mem_info_user*)(uintptr_t)regs->rdi;

    if (!out) return -(int64_t)LINUX_EFAULT;

    out->total = pmm_total_memory();
    out->free = pmm_free_memory();
    out->used = pmm_used_memory();
    return 0;
}

static int64_t sys_uptime_info(struct syscall_regs* regs) {
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

static int64_t sys_display_info(struct syscall_regs* regs) {
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

static int64_t sys_display_set(struct syscall_regs* regs) {
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

static int64_t sys_gfx_info(struct syscall_regs* regs) {
    struct aos_gfx_info_user* out = (struct aos_gfx_info_user*)(uintptr_t)regs->rdi;

    if (!out) return -(int64_t)LINUX_EFAULT;

    out->width = gfx_width();
    out->height = gfx_height();
    out->bpp = gfx_bpp();
    out->ready = gfx_is_ready() ? 1U : 0U;
    return 0;
}

static int64_t sys_gfx_clear(struct syscall_regs* regs) {
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_clear((uint32_t)regs->rdi);
    return 0;
}

static int64_t sys_gfx_pixel(struct syscall_regs* regs) {
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_putpixel((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx);
    return 0;
}

static int64_t sys_gfx_rect(struct syscall_regs* regs) {
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_fill_rect((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx,
                  (uint32_t)regs->r10, (uint32_t)regs->r8);
    return 0;
}

static int64_t sys_gfx_present(struct syscall_regs* regs) {
    (void)regs;
    if (!gfx_is_ready()) return -(int64_t)LINUX_ENODEV;
    gfx_present();
    return 0;
}

static int64_t sys_input_poll(struct syscall_regs* regs) {
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

static int64_t sys_time_info(struct syscall_regs* regs) {
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

static int64_t sys_user_info(struct syscall_regs* regs) {
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

static int64_t sys_sudo_auth(struct syscall_regs* regs) {
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

static int64_t sys_pci_info(struct syscall_regs* regs) {
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

static int64_t sys_driver_info(struct syscall_regs* regs) {
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

static int64_t sys_netdev_info(struct syscall_regs* regs) {
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

static int64_t sys_netdev_stats(struct syscall_regs* regs) {
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
    return 0;
}

static int64_t sys_arp_cache_info(struct syscall_regs* regs) {
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

static int64_t sys_dns_cache_info(struct syscall_regs* regs) {
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
    out->hits = entry->hits;
    out->ttl_ticks = entry->expires_at - now;
    for (size_t i = 0; i < 4; i++) {
        out->ipv4[i] = entry->ipv4[i];
    }
    copy_cstr_bounded(out->name, sizeof(out->name), entry->name);

    dev = netdev_get(entry->dev_index);
    if (dev) {
        copy_cstr_bounded(out->dev_name, sizeof(out->dev_name), dev->name);
    }
    return 0;
}

static int64_t sys_net_cache_flush(struct syscall_regs* regs) {
    uint64_t flags = regs->rdi;
    uint64_t dev_filter = regs->rsi;
    uint64_t flushed = 0;

    if ((flags & ~3ULL) != 0 || flags == 0) return -(int64_t)LINUX_EINVAL;
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

    return (int64_t)flushed;
}

static int64_t sys_socket_info(struct syscall_regs* regs) {
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
    for (size_t i = 0; i < 4; i++) {
        out->remote_ip[i] = sock->remote_ip[i];
        out->next_hop_ip[i] = sock->next_hop_ip[i];
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

static int64_t sys_netdev_send(struct syscall_regs* regs) {
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

static int64_t sys_netdev_recv(struct syscall_regs* regs) {
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
        local_memcpy(frame, local_frame, (size_t)rc);
    }
    return rc;
}

static int64_t sys_netdev_ipv6_config(struct syscall_regs* regs) {
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

static int64_t sys_netdev_ipv4_config(struct syscall_regs* regs) {
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

static int64_t sys_firmware_info(struct syscall_regs* regs) {
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

static int64_t sys_wifi_scan_info(struct syscall_regs* regs) {
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

static int64_t sys_wifi_state_info(struct syscall_regs* regs) {
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

static int64_t sys_wifi_control(struct syscall_regs* regs) {
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

static int64_t sys_shutdown(struct syscall_regs* regs) {
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

static int64_t sys_restart(struct syscall_regs* regs) {
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

static int64_t sys_partition_create(struct syscall_regs* regs) {
    uint8_t fs_type = (uint8_t)regs->rdi;
    uint64_t size = regs->rsi;
    uint8_t role = (uint8_t)regs->rdx;
    int rc = partition_create_planned(fs_type, size, role);
    if (rc < 0) return -(int64_t)LINUX_EINVAL;
    return rc;
}

static int64_t sys_partition_delete(struct syscall_regs* regs) {
    if (partition_delete_planned((size_t)regs->rdi) != 0) return -(int64_t)LINUX_EINVAL;
    return 0;
}

static int64_t sys_partition_type(struct syscall_regs* regs) {
    if (partition_cycle_planned_type((size_t)regs->rdi) != 0) return -(int64_t)LINUX_EINVAL;
    return 0;
}

static int64_t sys_partition_write(struct syscall_regs* regs) {
    uint32_t blkdev_id = (uint32_t)regs->rdi;
    if (partition_write_table(blkdev_id) != 0) return -(int64_t)LINUX_EIO;
    return 0;
}

static int64_t sys_mount_info(struct syscall_regs* regs) {
    struct vfs_mount_info* out = (struct vfs_mount_info*)(uintptr_t)regs->rsi;
    if (!out) return -(int64_t)LINUX_EFAULT;
    if (vfs_mount_info_at((size_t)regs->rdi, out) != 0) return -(int64_t)LINUX_ENOENT;
    return 0;
}

static int64_t sys_partition_role(struct syscall_regs* regs) {
    if (partition_cycle_planned_role((size_t)regs->rdi) != 0) return -(int64_t)LINUX_EINVAL;
    return 0;
}

static int64_t sys_partition_layout(struct syscall_regs* regs) {
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

static void halt_forever(void) {
    for (;;) {
        asm volatile("hlt");
    }
}

static void serial_print_u64(uint64_t value) {
    char buf[21];
    size_t i = sizeof(buf) - 1;
    buf[i] = '\0';
    if (value == 0) {
        serial_print("0");
        return;
    }
    while (value > 0 && i > 0) {
        i--;
        buf[i] = (char)('0' + (value % 10));
        value /= 10;
    }
    serial_print(&buf[i]);
}

void syscall_handler(struct syscall_regs* regs) {
    switch (regs->rax) {
        case LINUX_SYS_READ:
            regs->rax = (uint64_t)sys_read(regs);
            return;
        case LINUX_SYS_WRITE:
            regs->rax = (uint64_t)sys_write(regs);
            return;
        case LINUX_SYS_POLL:
            regs->rax = (uint64_t)sys_poll(regs);
            return;
        case LINUX_SYS_IOCTL:
            regs->rax = (uint64_t)sys_ioctl(regs);
            return;
        case LINUX_SYS_READV:
            regs->rax = (uint64_t)sys_readv(regs);
            return;
        case LINUX_SYS_WRITEV:
            regs->rax = (uint64_t)sys_writev(regs);
            return;
        case LINUX_SYS_OPEN:
            regs->rax = (uint64_t)sys_open(regs);
            return;
        case LINUX_SYS_ACCESS:
            regs->rax = (uint64_t)sys_access(regs);
            return;
        case LINUX_SYS_PIPE:
            regs->rax = (uint64_t)sys_pipe(regs);
            return;
        case LINUX_SYS_DUP:
            regs->rax = (uint64_t)sys_dup(regs);
            return;
        case LINUX_SYS_DUP2:
            regs->rax = (uint64_t)sys_dup2(regs);
            return;
        case LINUX_SYS_SOCKET:
            regs->rax = (uint64_t)sys_socket(regs);
            return;
        case LINUX_SYS_CONNECT:
            regs->rax = (uint64_t)sys_connect(regs);
            return;
        case LINUX_SYS_SENDTO:
            regs->rax = (uint64_t)sys_sendto(regs);
            return;
        case LINUX_SYS_RECVFROM:
            regs->rax = (uint64_t)sys_recvfrom(regs);
            return;
        case LINUX_SYS_WAIT4:
            regs->rax = (uint64_t)sys_wait4(regs);
            return;
        case LINUX_SYS_OPENAT:
            regs->rax = (uint64_t)sys_openat(regs);
            return;
        case LINUX_SYS_MKDIR:
            regs->rax = (uint64_t)sys_mkdir(regs);
            return;
        case LINUX_SYS_MKDIRAT:
            regs->rax = (uint64_t)sys_mkdirat(regs);
            return;
        case LINUX_SYS_UNLINKAT:
            regs->rax = (uint64_t)sys_unlinkat(regs);
            return;
        case LINUX_SYS_CLOSE:
            regs->rax = (uint64_t)sys_close(regs);
            return;
        case LINUX_SYS_STAT:
            regs->rax = (uint64_t)sys_stat(regs);
            return;
        case LINUX_SYS_FSTAT:
            regs->rax = (uint64_t)sys_fstat(regs);
            return;
        case LINUX_SYS_MMAP:
            regs->rax = (uint64_t)sys_mmap(regs);
            return;
        case LINUX_SYS_MPROTECT:
            regs->rax = (uint64_t)sys_mprotect(regs);
            return;
        case LINUX_SYS_MUNMAP:
            regs->rax = (uint64_t)sys_munmap(regs);
            return;
        case LINUX_SYS_BRK:
            regs->rax = (uint64_t)sys_brk(regs);
            return;
        case LINUX_SYS_RT_SIGACTION:
            regs->rax = (uint64_t)sys_rt_sigaction(regs);
            return;
        case LINUX_SYS_RT_SIGPROCMASK:
            regs->rax = (uint64_t)sys_rt_sigprocmask(regs);
            return;
        case LINUX_SYS_LSEEK:
            regs->rax = (uint64_t)sys_lseek(regs);
            return;
        case LINUX_SYS_NEWFSTATAT:
            regs->rax = (uint64_t)sys_newfstatat(regs);
            return;
        case LINUX_SYS_GETDENTS64:
            regs->rax = (uint64_t)sys_getdents64(regs);
            return;
        case LINUX_SYS_SET_TID_ADDRESS:
            regs->rax = (uint64_t)sys_set_tid_address(regs);
            return;
        case LINUX_SYS_FADVISE64:
            regs->rax = 0;
            return;
        case LINUX_SYS_CLOCK_GETTIME:
            regs->rax = (uint64_t)sys_clock_gettime(regs);
            return;
        case LINUX_SYS_READLINKAT:
            regs->rax = (uint64_t)(-(int64_t)LINUX_ENOENT);
            return;
        case LINUX_SYS_SET_ROBUST_LIST:
        case LINUX_SYS_RSEQ:
            regs->rax = 0;
            return;
        case LINUX_SYS_PRLIMIT64:
            regs->rax = (uint64_t)sys_prlimit64(regs);
            return;
        case LINUX_SYS_GETRANDOM:
            regs->rax = (uint64_t)sys_getrandom(regs);
            return;
        case LINUX_SYS_STATX:
            regs->rax = (uint64_t)(-(int64_t)LINUX_ENOSYS);
            return;
        case AOS_SYS_PARTITION_INFO:
            regs->rax = (uint64_t)sys_partition_info(regs);
            return;
        case AOS_SYS_PARTITION_CREATE:
            regs->rax = (uint64_t)sys_partition_create(regs);
            return;
        case AOS_SYS_PARTITION_DELETE:
            regs->rax = (uint64_t)sys_partition_delete(regs);
            return;
        case AOS_SYS_PARTITION_TYPE:
            regs->rax = (uint64_t)sys_partition_type(regs);
            return;
        case AOS_SYS_BLKDEV_INFO:
            regs->rax = (uint64_t)sys_blkdev_info(regs);
            return;
        case AOS_SYS_PARTITION_WRITE:
            regs->rax = (uint64_t)sys_partition_write(regs);
            return;
        case AOS_SYS_MOUNT_INFO:
            regs->rax = (uint64_t)sys_mount_info(regs);
            return;
        case AOS_SYS_PARTITION_ROLE:
            regs->rax = (uint64_t)sys_partition_role(regs);
            return;
        case AOS_SYS_PARTITION_LAYOUT:
            regs->rax = (uint64_t)sys_partition_layout(regs);
            return;
        case AOS_SYS_MEM_INFO:
            regs->rax = (uint64_t)sys_mem_info(regs);
            return;
        case AOS_SYS_UPTIME_INFO:
            regs->rax = (uint64_t)sys_uptime_info(regs);
            return;
        case AOS_SYS_DISPLAY_INFO:
            regs->rax = (uint64_t)sys_display_info(regs);
            return;
        case AOS_SYS_DISPLAY_SET:
            regs->rax = (uint64_t)sys_display_set(regs);
            return;
        case AOS_SYS_SHUTDOWN:
            regs->rax = (uint64_t)sys_shutdown(regs);
            return;
        case AOS_SYS_RESTART:
            regs->rax = (uint64_t)sys_restart(regs);
            return;
        case AOS_SYS_TIME_INFO:
            regs->rax = (uint64_t)sys_time_info(regs);
            return;
        case AOS_SYS_USER_INFO:
            regs->rax = (uint64_t)sys_user_info(regs);
            return;
        case AOS_SYS_SUDO_AUTH:
            regs->rax = (uint64_t)sys_sudo_auth(regs);
            return;
        case AOS_SYS_PCI_INFO:
            regs->rax = (uint64_t)sys_pci_info(regs);
            return;
        case AOS_SYS_DRIVER_INFO:
            regs->rax = (uint64_t)sys_driver_info(regs);
            return;
        case AOS_SYS_GFX_INFO:
            regs->rax = (uint64_t)sys_gfx_info(regs);
            return;
        case AOS_SYS_GFX_CLEAR:
            regs->rax = (uint64_t)sys_gfx_clear(regs);
            return;
        case AOS_SYS_GFX_PIXEL:
            regs->rax = (uint64_t)sys_gfx_pixel(regs);
            return;
        case AOS_SYS_GFX_RECT:
            regs->rax = (uint64_t)sys_gfx_rect(regs);
            return;
        case AOS_SYS_GFX_PRESENT:
            regs->rax = (uint64_t)sys_gfx_present(regs);
            return;
        case AOS_SYS_INPUT_POLL:
            regs->rax = (uint64_t)sys_input_poll(regs);
            return;
        case AOS_SYS_NETDEV_INFO:
            regs->rax = (uint64_t)sys_netdev_info(regs);
            return;
        case AOS_SYS_NETDEV_STATS:
            regs->rax = (uint64_t)sys_netdev_stats(regs);
            return;
        case AOS_SYS_ARP_CACHE_INFO:
            regs->rax = (uint64_t)sys_arp_cache_info(regs);
            return;
        case AOS_SYS_DNS_CACHE_INFO:
            regs->rax = (uint64_t)sys_dns_cache_info(regs);
            return;
        case AOS_SYS_NET_CACHE_FLUSH:
            regs->rax = (uint64_t)sys_net_cache_flush(regs);
            return;
        case AOS_SYS_SOCKET_INFO:
            regs->rax = (uint64_t)sys_socket_info(regs);
            return;
        case AOS_SYS_NETDEV_SEND:
            regs->rax = (uint64_t)sys_netdev_send(regs);
            return;
        case AOS_SYS_NETDEV_RECV:
            regs->rax = (uint64_t)sys_netdev_recv(regs);
            return;
        case AOS_SYS_NETDEV_IPV6_CONFIG:
            regs->rax = (uint64_t)sys_netdev_ipv6_config(regs);
            return;
        case AOS_SYS_NETDEV_IPV4_CONFIG:
            regs->rax = (uint64_t)sys_netdev_ipv4_config(regs);
            return;
        case AOS_SYS_FIRMWARE_INFO:
            regs->rax = (uint64_t)sys_firmware_info(regs);
            return;
        case AOS_SYS_WIFI_SCAN_INFO:
            regs->rax = (uint64_t)sys_wifi_scan_info(regs);
            return;
        case AOS_SYS_WIFI_STATE_INFO:
            regs->rax = (uint64_t)sys_wifi_state_info(regs);
            return;
        case AOS_SYS_WIFI_CONTROL:
            regs->rax = (uint64_t)sys_wifi_control(regs);
            return;
        case AOS_SYS_SOCKET_BIND_NETDEV:
            regs->rax = (uint64_t)sys_socket_bind_netdev(regs);
            return;
        case AOS_SYS_DNS_LOOKUP:
            regs->rax = (uint64_t)sys_dns_lookup(regs);
            return;
        case LINUX_SYS_FACCESSAT:
            regs->rax = (uint64_t)sys_faccessat(regs);
            return;
        case LINUX_SYS_GETPID:
            regs->rax = (uint64_t)sys_getpid(regs);
            return;
        case LINUX_SYS_GETPPID:
            regs->rax = (uint64_t)sys_getppid(regs);
            return;
        case LINUX_SYS_GETUID:
            regs->rax = process_get_uid();
            return;
        case LINUX_SYS_GETGID:
            regs->rax = process_get_gid();
            return;
        case LINUX_SYS_GETEUID:
            regs->rax = process_get_euid();
            return;
        case LINUX_SYS_GETEGID:
            regs->rax = process_get_egid();
            return;
        case LINUX_SYS_SETUID:
        case LINUX_SYS_SETGID:
            regs->rax = (uint64_t)(-(int64_t)LINUX_EPERM);
            return;
        case LINUX_SYS_PRCTL:
            regs->rax = 0;
            return;
        case LINUX_SYS_UNAME:
            regs->rax = (uint64_t)sys_uname(regs);
            return;
        case LINUX_SYS_FCNTL:
            regs->rax = (uint64_t)sys_fcntl(regs);
            return;
        case LINUX_SYS_GETCWD:
            regs->rax = (uint64_t)sys_getcwd(regs);
            return;
        case LINUX_SYS_CHDIR:
            regs->rax = (uint64_t)sys_chdir(regs);
            return;
        case LINUX_SYS_ARCH_PRCTL:
            regs->rax = (uint64_t)sys_arch_prctl(regs);
            return;
        case LINUX_SYS_EXECVE:
            regs->rax = (uint64_t)sys_execve(regs);
            return;
        case LINUX_SYS_FORK:
            regs->rax = (uint64_t)sys_fork(regs);
            return;
        case LINUX_SYS_CLONE:
            regs->rax = (uint64_t)sys_clone(regs);
            return;
        case LINUX_SYS_EXIT:
        case LINUX_SYS_EXIT_GROUP:
            if (current_process && current_process->pid != 1) {
                process_exit_and_wake_parent((int)regs->rdi);
            }
            serial_print("AOS: userspace exited\n");
            regs->rax = (uint64_t)exec_initrd_program("shell.elf", NULL, NULL);
            if ((int64_t)regs->rax < 0) {
                serial_print("AOS PANIC: failed to relaunch shell\n");
                halt_forever();
            }
            return;
        default:
            serial_print("AOS: unimplemented syscall ");
            serial_print_u64(regs->rax);
            serial_print("\n");
            regs->rax = (uint64_t)(-(int64_t)LINUX_ENOSYS);
            return;
    }
}

void syscall_init_process_fd_table(struct fd_entry* table, size_t count) {
    if (!table || count < 3) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        table[i].kind = FD_KIND_FREE;
        table[i].handle_index = -1;
    }
    table[0].kind = FD_KIND_STDIN;
    table[1].kind = FD_KIND_STDOUT;
    table[2].kind = FD_KIND_STDERR;
}

void syscall_retain_fd_table_entries(struct fd_entry* table, size_t count) {
    if (!table) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (table[i].kind != FD_KIND_FREE) {
            retain_fd_entry_refs(&table[i]);
        }
    }
}
