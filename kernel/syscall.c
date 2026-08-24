/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include "syscall/syscall_internal.h"

struct file_handle g_file_handles[MAX_FILE_HANDLES];
struct pipe_object g_pipe_objects[MAX_PIPE_OBJECTS];
struct eventfd_object g_eventfd_objects[MAX_EVENTFD_OBJECTS];
struct socket_object g_socket_objects[MAX_SOCKET_OBJECTS];
struct dns_cache_entry g_dns_cache[SOCKET_DNS_CACHE_ENTRIES];
struct arp_cache_entry g_arp_cache[SOCKET_ARP_CACHE_ENTRIES];
struct ndp_cache_entry g_ndp_cache[SOCKET_NDP_CACHE_ENTRIES];
uint16_t g_socket_next_port = SOCKET_TCP_PORT_FIRST;
void halt_forever(void);

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

void* local_memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    while (n--) {
        *d++ = (uint8_t)value;
    }
    return dst;
}

void* local_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

uint64_t align_up_page(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static int command_has_prefix(const char* command, const char* prefix) {
    if (!command || !prefix) return 0;
    while (*prefix != '\0') {
        if (*command++ != *prefix++) return 0;
    }
    return 1;
}

static void serial_print_i64(int64_t value) {
    char buffer[22];
    size_t cursor = sizeof(buffer);
    uint64_t magnitude;

    buffer[--cursor] = '\0';
    if (value < 0) {
        magnitude = (uint64_t)(-(value + 1)) + 1;
    } else {
        magnitude = (uint64_t)value;
    }
    do {
        buffer[--cursor] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    if (value < 0) buffer[--cursor] = '-';
    serial_print(&buffer[cursor]);
}

int syscall_linux_trace_enabled(void) {
    process_t* proc = get_current_process();
    return proc && command_has_prefix(proc->command, "linux-dynamic-");
}

void syscall_linux_trace(const char* operation, int64_t result) {
    if (!syscall_linux_trace_enabled()) return;
    serial_print("linux-compat: ");
    serial_print(operation);
    serial_print(" -> ");
    serial_print_i64(result);
    serial_print("\n");
}

void syscall_linux_trace_path(const char* operation, const char* path,
                              int64_t result) {
    if (!syscall_linux_trace_enabled()) return;
    serial_print("linux-compat: ");
    serial_print(operation);
    serial_print(" ");
    serial_print(path ? path : "(null)");
    serial_print(" -> ");
    serial_print_i64(result);
    serial_print("\n");
}

void set_uts_field(char* dst, const char* src) {
    uint64_t i = 0;
    while (src[i] && i < 64) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void copy_kernel_cstr_bounded(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int64_t copy_user_cstr(const char* user, char* dst, size_t dst_size) {
    if (!user || !dst || dst_size == 0) return -(int64_t)LINUX_EFAULT;

    for (size_t i = 0; i < dst_size; i++) {
        char c = user[i];
        dst[i] = c;
        if (c == '\0') return (int64_t)i;
    }

    dst[dst_size - 1] = '\0';
    return -(int64_t)LINUX_E2BIG;
}

struct fd_entry* current_fd_table(void) {
    return process_get_fd_table();
}

void switch_page_table(uint64_t* table) {
    asm volatile("mov %0, %%cr3" : : "r"(table) : "memory");
}

const char* normalize_path(const char* path) {
    while (*path == '/') {
        path++;
    }
    return path;
}

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

int user_can_mutate_path(const char* path) {
    if (process_is_root()) return 1;
    if (!path || *path == '\0') return 0;

    return path_starts_with_component(path, process_get_home()) ||
           path_starts_with_component(path, "tmp") ||
           path_starts_with_component(path, "trash");
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

int allocate_fd_slot(int start_fd) {
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

struct file_handle* get_file_handle_by_index(int32_t handle_index) {
    if (handle_index < 0 || handle_index >= MAX_FILE_HANDLES) {
        return NULL;
    }
    if (!g_file_handles[handle_index].in_use) {
        return NULL;
    }
    return &g_file_handles[handle_index];
}

struct fd_entry* get_fd_entry(uint64_t fd) {
    struct fd_entry* table = current_fd_table();
    if (!table || fd >= PROCESS_FD_MAX) {
        return NULL;
    }
    if (table[fd].kind == FD_KIND_FREE) {
        return NULL;
    }
    return &table[fd];
}

struct file_handle* get_vnode_handle(uint64_t fd) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry || entry->kind != FD_KIND_VNODE) {
        return NULL;
    }
    return get_file_handle_by_index(entry->handle_index);
}

struct pipe_object* get_pipe_object_by_index(int32_t pipe_index) {
    if (pipe_index < 0 || pipe_index >= MAX_PIPE_OBJECTS) {
        return NULL;
    }
    if (!g_pipe_objects[pipe_index].in_use) {
        return NULL;
    }
    return &g_pipe_objects[pipe_index];
}

struct pipe_object* get_pipe_for_fd(uint64_t fd, uint8_t expected_kind) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry || entry->kind != expected_kind) {
        return NULL;
    }
    return get_pipe_object_by_index(entry->handle_index);
}

struct eventfd_object* get_eventfd_object_by_index(int32_t eventfd_index) {
    if (eventfd_index < 0 || eventfd_index >= MAX_EVENTFD_OBJECTS) {
        return NULL;
    }
    if (!g_eventfd_objects[eventfd_index].in_use) {
        return NULL;
    }
    return &g_eventfd_objects[eventfd_index];
}

struct socket_object* get_socket_by_index(int32_t socket_index) {
    if (socket_index < 0 || socket_index >= MAX_SOCKET_OBJECTS) {
        return NULL;
    }
    if (!g_socket_objects[socket_index].in_use) {
        return NULL;
    }
    return &g_socket_objects[socket_index];
}

struct socket_object* get_socket_for_fd(uint64_t fd) {
    struct fd_entry* entry = get_fd_entry(fd);
    if (!entry || entry->kind != FD_KIND_SOCKET) {
        return NULL;
    }
    return get_socket_by_index(entry->handle_index);
}

void release_file_handle(int32_t handle_index) {
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

void retain_fd_entry_refs(struct fd_entry* entry) {
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
        return;
    }
    if (entry->kind == FD_KIND_EVENTFD) {
        struct eventfd_object* eventfd =
            get_eventfd_object_by_index(entry->handle_index);
        if (eventfd) eventfd->refcount++;
    }
}

void release_fd_entry_refs(struct fd_entry* entry) {
    if (!entry) return;
    if (entry->kind == FD_KIND_VNODE) {
        release_file_handle(entry->handle_index);
    } else if (entry->kind == FD_KIND_PIPE_READER ||
               entry->kind == FD_KIND_PIPE_WRITER) {
        release_pipe_ref(entry->handle_index, entry->kind);
    } else if (entry->kind == FD_KIND_SOCKET) {
        close_socket_ref(entry->handle_index);
    } else if (entry->kind == FD_KIND_EVENTFD) {
        release_eventfd_ref(entry->handle_index);
    }
    entry->kind = FD_KIND_FREE;
    entry->handle_index = -1;
}

void release_pipe_ref(int32_t pipe_index, uint8_t kind) {
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

void release_eventfd_ref(int32_t eventfd_index) {
    struct eventfd_object* eventfd =
        get_eventfd_object_by_index(eventfd_index);

    if (!eventfd) return;
    if (eventfd->refcount > 0) eventfd->refcount--;
    if (eventfd->refcount == 0) {
        local_memset(eventfd, 0, sizeof(*eventfd));
    }
}

void release_socket_ref(int32_t socket_index) {
    struct socket_object* sock = get_socket_by_index(socket_index);
    struct fd_entry pending[SOCKET_ANCILLARY_FD_MAX];
    uint8_t pending_count = 0;
    if (!sock) {
        return;
    }
    if (sock->refcount > 0) {
        sock->refcount--;
    }
    if (sock->refcount == 0) {
        pending_count = sock->rx_ancillary_fd_count;
        if (pending_count > SOCKET_ANCILLARY_FD_MAX) {
            pending_count = SOCKET_ANCILLARY_FD_MAX;
        }
        for (uint8_t i = 0; i < pending_count; i++) {
            pending[i] = sock->rx_ancillary_fds[i];
        }
        local_memset(sock, 0, sizeof(*sock));
        for (uint8_t i = 0; i < pending_count; i++) {
            release_fd_entry_refs(&pending[i]);
        }
    }
}

void close_socket_ref(int32_t socket_index) {
    struct socket_object* sock = get_socket_by_index(socket_index);
    if (!sock) {
        return;
    }
    if (sock->refcount <= 1) {
        if (sock->family == LINUX_AF_UNIX) {
            struct socket_object* peer = get_socket_by_index(sock->peer_index);
            if (peer) peer->state = SOCKET_STATE_CLOSED;
            sock->state = SOCKET_STATE_CLOSED;
        } else if (sock->state == SOCKET_STATE_CONNECTED) {
            (void)socket_send_fin_close(sock);
        } else {
            sock->state = SOCKET_STATE_CLOSED;
        }
    }
    release_socket_ref(socket_index);
}

void close_fd_internal(uint64_t fd) {
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
    } else if (entry->kind == FD_KIND_EVENTFD) {
        release_eventfd_ref(entry->handle_index);
    }
    entry->kind = FD_KIND_FREE;
    entry->handle_index = -1;
}

int64_t install_vnode_fd(const struct vfs_node* node, uint64_t open_flags) {
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

int64_t install_device_fd(uint8_t kind, int32_t device_id) {
    int fd = allocate_fd_slot(3);

    if (fd < 0) {
        return -(int64_t)LINUX_EMFILE;
    }

    current_fd_table()[fd].kind = kind;
    current_fd_table()[fd].handle_index = device_id;
    return fd;
}

uint16_t allocate_socket_port(void) {
    uint16_t port = g_socket_next_port++;

    if (g_socket_next_port < SOCKET_TCP_PORT_FIRST || g_socket_next_port > SOCKET_TCP_PORT_LAST) {
        g_socket_next_port = SOCKET_TCP_PORT_FIRST;
    }
    return port;
}

int64_t install_socket_fd(int family, int type, int protocol) {
    int socket_index = -1;
    int fd = -1;
    int base_type = type & LINUX_SOCK_TYPE_MASK;

    if ((family != LINUX_AF_UNIX && family != LINUX_AF_INET &&
         family != LINUX_AF_INET6) ||
        (family == LINUX_AF_UNIX
             ? (base_type != LINUX_SOCK_STREAM &&
                base_type != LINUX_SOCK_DGRAM &&
                base_type != LINUX_SOCK_SEQPACKET)
             : base_type != LINUX_SOCK_STREAM) ||
        (type & ~(LINUX_SOCK_TYPE_MASK | LINUX_SOCK_NONBLOCK |
                  LINUX_SOCK_CLOEXEC)) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((family == LINUX_AF_UNIX && protocol != 0) ||
        (family != LINUX_AF_UNIX && protocol != 0 &&
         protocol != LINUX_IPPROTO_TCP)) {
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
    g_socket_objects[socket_index].type = (uint8_t)base_type;
    g_socket_objects[socket_index].protocol =
        family == LINUX_AF_UNIX ? 0 : LINUX_IPPROTO_TCP;
    g_socket_objects[socket_index].dev_index = 0;
    g_socket_objects[socket_index].refcount = 1;
    g_socket_objects[socket_index].peer_index = -1;
    g_socket_objects[socket_index].local_port = allocate_socket_port();
    g_socket_objects[socket_index].seq = SOCKET_TCP_BASE_SEQ + (uint32_t)(socket_index * 0x1000U);
    g_socket_objects[socket_index].peer_mss = SOCKET_TCP_DEFAULT_PEER_MSS;
    g_socket_objects[socket_index].cwnd_bytes = SOCKET_TCP_DEFAULT_PEER_MSS * SOCKET_TCP_INITIAL_CWND_SEGMENTS;
    g_socket_objects[socket_index].ssthresh_bytes = SOCKET_TCP_DEFAULT_PEER_MSS * SOCKET_TCP_INITIAL_SSTHRESH_SEGMENTS;
    g_socket_objects[socket_index].local_window_scale = SOCKET_TCP_LOCAL_WINDOW_SCALE;

    current_fd_table()[fd].kind = FD_KIND_SOCKET;
    current_fd_table()[fd].handle_index = socket_index;
    return fd;
}



int64_t dup_fd_common(uint64_t oldfd, int64_t requested_newfd, int overwrite) {
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

int64_t resolve_path_from_dirfd(int64_t dirfd, const char* path, char* out, size_t out_size) {
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

int64_t open_path_with_flags(const char* path, uint64_t flags,
                             uint16_t mode) {
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
        } else if (tmpfs_create_path_mode(path, mode,
                                          process_get_euid(),
                                          process_get_egid(), &node) == 0) {
            /* tmpfs filled the vnode. */
        } else if (aosfs_create_path_mode(path, mode,
                                          process_get_euid(),
                                          process_get_egid(), &node) != 0) {
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
        if (node.u.first_cluster == VFS_DEV_URANDOM) {
            return install_device_fd(FD_KIND_RANDOM,
                                     (int32_t)node.u.first_cluster);
        }
        if (node.u.first_cluster == VFS_DEV_CONSOLE || node.u.first_cluster == VFS_DEV_TTY0) {
            return install_device_fd(FD_KIND_TTY, (int32_t)node.u.first_cluster);
        }
        return -(int64_t)LINUX_ENOENT;
    }
    if (node.type == VFS_NODE_TYPE_DIRECTORY) {
        return -(int64_t)LINUX_EISDIR;
    }

    if (mutates && !user_can_mutate_path(node.path)) {
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
        if (flags & LINUX_O_TRUNC) {
            if (tmpfs_truncate_path(node.path) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            node.size = 0;
        }
    } else if (node.backend == VFS_BACKEND_AOSFS) {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if (flags & LINUX_O_TRUNC) {
            if (aosfs_truncate_path(node.path) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            node.size = 0;
        }
    } else if (node.backend == VFS_BACKEND_FAT32) {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if (flags & LINUX_O_TRUNC) {
            uint32_t first_cluster = 0;
            uint32_t size = 0;
            if (fat32_truncate_path(node.path, &first_cluster, &size) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            node.u.first_cluster = first_cluster;
            node.size = size;
        }
    } else {
        if ((flags & LINUX_O_EXCL) && (flags & LINUX_O_CREAT)) {
            return -(int64_t)LINUX_EACCES;
        }
        if (flags & LINUX_O_TRUNC) {
            uint32_t size = 0;
            if (ext4_truncate_path(node.path, &size) != 0) {
                return -(int64_t)LINUX_EACCES;
            }
            node.size = size;
        }
    }

    return install_vnode_fd(&node, flags);
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
    uint64_t program_entry,
    uint64_t interpreter_base,
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
        AUXV_AT_PLATFORM = 15,
        AUXV_AT_HWCAP = 16,
        AUXV_AT_CLKTCK = 17,
        AUXV_AT_SECURE = 23,
        AUXV_AT_RANDOM = 25,
        AUXV_AT_HWCAP2 = 26,
        AUXV_AT_EXECFN = 31,
    };
    const size_t aux_pair_count = 19;
    process_t* proc = get_current_process();
    uint64_t sp = USER_STACK_BASE + USER_STACK_SIZE;
    uint64_t arg_ptrs[MAX_EXEC_ARGS];
    uint64_t env_ptrs[MAX_EXEC_ENVP];
    uint64_t random_ptr = 0;
    uint64_t platform_ptr = 0;

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

    {
        int64_t rc = copy_string_to_stack(stack_pages, &sp, "x86_64",
                                          &platform_ptr);
        if (rc < 0) return rc;
    }

    if (sp < USER_STACK_BASE + 16) return -(int64_t)LINUX_E2BIG;
    sp -= 16;
    {
        uint64_t seed = timer_get_ticks() ^
                        ((uint64_t)(proc ? proc->pid : 1U) << 32) ^
                        0xA05C2026D15EA5E5ULL;
        for (size_t i = 0; i < 16; i++) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            uint8_t* dst = stack_va_to_ptr(stack_pages, sp + i);
            if (!dst) return -(int64_t)LINUX_EFAULT;
            *dst = (uint8_t)(seed >> ((i & 7U) * 8U));
        }
    }
    random_ptr = sp;

    sp &= ~0xFULL;
    if (((aux_pair_count * 2U + envc + argc + 3U) & 1U) != 0) {
        int64_t rc = push_u64(stack_pages, &sp, 0);
        if (rc < 0) return rc;
    }

    int64_t rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_NULL, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_EXECFN, arg_ptrs[0]);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_RANDOM, random_ptr);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_SECURE, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_HWCAP2, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_CLKTCK,
                        timer_get_frequency() ? timer_get_frequency() : 100);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_HWCAP, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_PLATFORM, platform_ptr);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_EGID,
                        proc ? proc->egid : 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_GID,
                        proc ? proc->gid : 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_EUID,
                        proc ? proc->euid : 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_UID,
                        proc ? proc->uid : 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_ENTRY, program_entry);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_FLAGS, 0);
    if (rc < 0) return rc;
    rc = push_auxv_pair(stack_pages, &sp, AUXV_AT_BASE, interpreter_base);
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

#define EXEC_USER_ELF_DYN_BASE 0x0000700001000000ULL
#define EXEC_USER_INTERP_DYN_BASE 0x0000701000000000ULL
#define EXEC_FILE_IMAGE_MAX (8U * 1024U * 1024U)

static uint8_t exec_file_image[EXEC_FILE_IMAGE_MAX];
static uint8_t exec_interp_image[EXEC_FILE_IMAGE_MAX];

static int exec_vfs_elf_read(void* context, uint64_t offset,
                             void* buffer, size_t size) {
    const struct vfs_node* node = (const struct vfs_node*)context;
    return vfs_read_node(node, offset, (uint8_t*)buffer,
                         (uint64_t)size) == 0 ? 0 : -1;
}

int64_t exec_initrd_program(
    const char* normalized,
    const uint64_t* argv_user,
    const uint64_t* envp_user
) {
    process_t* proc = get_current_process();
    uint64_t* old_p4 = proc ? proc->p4_table : NULL;
    uint64_t* new_p4 = NULL;
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
    const uint8_t* interp_data = NULL;
    uint32_t elf_size = 0;
    uint32_t interp_size = 0;
    int stream_program = 0;
    int stream_interpreter = 0;
    struct vfs_node program_node;
    struct vfs_node interpreter_node;
    const struct elf64_load_options program_options = {
        .dynamic_base = EXEC_USER_ELF_DYN_BASE,
        .apply_relocations = 1,
    };
    const struct elf64_load_options interpreter_options = {
        .dynamic_base = EXEC_USER_INTERP_DYN_BASE,
        .apply_relocations = 0,
    };
    struct elf64_load_result program_load;
    struct elf64_load_result interpreter_load;
    uint64_t start_entry;
    uint64_t interpreter_base = 0;
    int64_t failure = -(int64_t)LINUX_EINVAL;
    const char* failure_stage = "program ELF";

    if (!proc || !old_p4) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (vfs_lookup(normalized, &program_node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (program_node.type != VFS_NODE_TYPE_REGULAR) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (program_node.backend != VFS_BACKEND_AOSFS &&
        program_node.backend != VFS_BACKEND_INITRD &&
        program_node.backend != VFS_BACKEND_TMPFS &&
        program_node.backend != VFS_BACKEND_FAT32 &&
        program_node.backend != VFS_BACKEND_EXT4) {
        return -(int64_t)LINUX_EACCES;
    }
    elf_size = program_node.size;
    if ((program_node.backend == VFS_BACKEND_AOSFS ||
         program_node.backend == VFS_BACKEND_INITRD) &&
        program_node.u.data) {
        elf_data = program_node.u.data;
    } else {
        if (elf_size > sizeof(exec_file_image)) {
            stream_program = 1;
        } else {
            if (vfs_read_node(&program_node, 0,
                              exec_file_image, elf_size) != 0) {
                return -(int64_t)LINUX_EIO;
            }
            elf_data = exec_file_image;
        }
    }

    if (elf_size < sizeof(struct exec_elf64_ehdr)) {
        return -(int64_t)LINUX_EINVAL;
    }

    new_p4 = vmm_create_user_space(old_p4);
    if (!new_p4) {
        return -(int64_t)LINUX_ENOMEM;
    }

    if ((stream_program
             ? elf64_load_reader_ex(new_p4, elf_size,
                                    exec_vfs_elf_read, &program_node,
                                    &program_options, &program_load)
             : elf64_load_image_ex(new_p4, elf_data, elf_size,
                                   &program_options, &program_load)) != 0) {
        goto exec_failed;
    }
    start_entry = program_load.entry;

    if (program_load.interpreter[0] != '\0') {
        const char* interpreter_path =
            normalize_path(program_load.interpreter);

        failure_stage = "interpreter lookup";
        if (*interpreter_path == '\0' ||
            vfs_lookup(interpreter_path, &interpreter_node) != 0) {
            failure = -(int64_t)LINUX_ENOENT;
            goto exec_failed;
        }
        if (interpreter_node.type != VFS_NODE_TYPE_REGULAR) {
            goto exec_failed;
        }
        interp_size = interpreter_node.size;
        if ((interpreter_node.backend == VFS_BACKEND_AOSFS ||
             interpreter_node.backend == VFS_BACKEND_INITRD) &&
            interpreter_node.u.data) {
            interp_data = interpreter_node.u.data;
        } else {
            failure_stage = "interpreter read";
            if (interp_size > sizeof(exec_interp_image)) {
                stream_interpreter = 1;
            } else {
                if (vfs_read_node(&interpreter_node, 0,
                                  exec_interp_image,
                                  interp_size) != 0) {
                    failure = -(int64_t)LINUX_EIO;
                    goto exec_failed;
                }
                interp_data = exec_interp_image;
            }
        }
        failure_stage = "interpreter ELF";
        if ((stream_interpreter
                 ? elf64_load_reader_ex(new_p4, interp_size,
                                        exec_vfs_elf_read,
                                        &interpreter_node,
                                        &interpreter_options,
                                        &interpreter_load)
                 : elf64_load_image_ex(new_p4, interp_data, interp_size,
                                       &interpreter_options,
                                       &interpreter_load)) != 0 ||
            interpreter_load.interpreter[0] != '\0') {
            goto exec_failed;
        }
        start_entry = interpreter_load.entry;
        interpreter_base = interpreter_load.load_bias;
    }

    uint8_t* stack_pages[USER_STACK_PAGES];
    for (size_t i = 0; i < USER_STACK_PAGES; i++) {
        stack_pages[i] = NULL;
    }
    failure_stage = "stack allocation";
    for (size_t i = 0; i < USER_STACK_PAGES; i++) {
        stack_pages[i] = (uint8_t*)pmm_alloc_block();
        if (!stack_pages[i]) {
            failure = -(int64_t)LINUX_ENOMEM;
            goto exec_failed;
        }
        local_memset(stack_pages[i], 0, 4096);
        vmm_map_page(
            new_p4,
            USER_STACK_BASE + i * 4096ULL,
            (uint64_t)stack_pages[i],
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
        );
    }

    uint64_t new_rsp = 0;
    failure_stage = "initial stack";
    rc = build_exec_stack(
        stack_pages,
        &new_rsp,
        argv_kernel,
        argc,
        envp_kernel,
        envc,
        normalized,
        program_load.entry,
        interpreter_base,
        program_load.program_headers,
        program_load.program_header_size,
        program_load.program_header_count
    );
    if (rc < 0) {
        failure = rc;
        goto exec_failed;
    }

    proc->fs_base = 0;
    proc->brk_base = align_up_page(program_load.image_end);
    proc->brk_current = proc->brk_base;
    proc->brk_mapped_end = proc->brk_base;
    proc->mmap_next = USER_MMAP_BASE;
    proc->clear_child_tid = 0;
    copy_kernel_cstr_bounded(proc->command, sizeof(proc->command), normalized);

    proc->p4_table = new_p4;
    switch_page_table(new_p4);
    process_release_address_space(old_p4, proc);
    process_load_fs_base(0);
    process_complete_vfork(proc);

    serial_print("AOS: execve -> ");
    serial_print(normalized);
    serial_print("\n");

    input_clear_events();
    asm volatile("swapgs");
    jump_to_user(start_entry, new_rsp);
    return 0;

exec_failed:
    serial_print("AOS: execve failed at ");
    serial_print(failure_stage);
    serial_print("\n");
    vmm_destroy_user_space(new_p4);
    return failure;
}
//OREWA OPPEKO THE FREIZA FORCE






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
//Pov me checking my code our "TRYNA FIGURE THIS MESS OUT"
