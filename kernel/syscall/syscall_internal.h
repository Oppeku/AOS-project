/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef AOS_SYSCALL_INTERNAL_H
#define AOS_SYSCALL_INTERNAL_H

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
extern void keyboard_handler_main(void);

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
#define LINUX_AF_INET6 10
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
#define SOCKET_RX_BUFFER_SIZE 8192
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
#define SOCKET_NDP_CACHE_ENTRIES 8
#define SOCKET_NDP_CACHE_TTL_TICKS 6000ULL
#define SOCKET_DNS_TIMEOUT_SECONDS 5ULL
#define SOCKET_DNS_RETRIES 3
#define SOCKET_TCP_RETRIES 3
#define SOCKET_TCP_RECV_RETRIES 3
#define SOCKET_TCP_INITIAL_RTO_SECONDS 1ULL
#define SOCKET_TCP_MAX_RTO_SHIFT 3
#define SOCKET_TCP_WINDOW_WAIT_SECONDS 4ULL
#define SOCKET_TCP_LOCAL_MSS 1460
#define SOCKET_TCP_DEFAULT_PEER_MSS 1460
#define SOCKET_TCP_INITIAL_CWND_SEGMENTS 1U
#define SOCKET_TCP_INITIAL_SSTHRESH_SEGMENTS 8U
#define SOCKET_TCP_LOCAL_WINDOW_SCALE 2U

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
    uint16_t peer_window;
    uint16_t peer_mss;
    uint32_t peer_window_bytes;
    uint32_t retransmits;
    uint32_t cwnd_bytes;
    uint32_t ssthresh_bytes;
    uint8_t local_window_scale;
    uint8_t peer_window_scale;
    uint16_t reserved_window_scale;
    uint8_t remote_ip[4];
    uint8_t next_hop_ip[4];
    uint8_t remote_ip6[16];
    uint8_t next_hop_ip6[16];
    uint8_t remote_mac[6];
    uint8_t rx_buffer[SOCKET_RX_BUFFER_SIZE];
    uint32_t rx_len;
    uint32_t rx_off;
};

struct dns_cache_entry {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t family;
    uint8_t reserved;
    uint32_t hits;
    uint64_t expires_at;
    char name[SOCKET_DNS_NAME_MAX];
    uint8_t ipv4[4];
    uint8_t ipv6[16];
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

struct ndp_cache_entry {
    uint8_t valid;
    uint8_t dev_index;
    uint8_t reserved[2];
    uint32_t hits;
    uint64_t expires_at;
    uint8_t ipv6[16];
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

#define LINUX_POLLIN 0x0001
#define LINUX_POLLOUT 0x0004
#define LINUX_POLLERR 0x0008
#define LINUX_POLLHUP 0x0010
#define LINUX_POLLNVAL 0x0020

extern struct file_handle g_file_handles[MAX_FILE_HANDLES];
extern struct pipe_object g_pipe_objects[MAX_PIPE_OBJECTS];
extern struct socket_object g_socket_objects[MAX_SOCKET_OBJECTS];
extern struct dns_cache_entry g_dns_cache[SOCKET_DNS_CACHE_ENTRIES];
extern struct arp_cache_entry g_arp_cache[SOCKET_ARP_CACHE_ENTRIES];
extern struct ndp_cache_entry g_ndp_cache[SOCKET_NDP_CACHE_ENTRIES];
extern uint16_t g_socket_next_port;

void halt_forever(void);
void* local_memset(void* dst, int value, size_t n);
void* local_memcpy(void* dst, const void* src, size_t n);
uint64_t align_up_page(uint64_t value);
void set_uts_field(char* dst, const char* src);
int64_t copy_user_cstr(const char* user, char* dst, size_t dst_size);
struct fd_entry* current_fd_table(void);
void switch_page_table(uint64_t* table);
const char* normalize_path(const char* path);
int user_can_mutate_path(const char* path);
int allocate_fd_slot(int start_fd);
struct file_handle* get_file_handle_by_index(int32_t handle_index);
struct fd_entry* get_fd_entry(uint64_t fd);
struct file_handle* get_vnode_handle(uint64_t fd);
struct pipe_object* get_pipe_object_by_index(int32_t pipe_index);
struct pipe_object* get_pipe_for_fd(uint64_t fd, uint8_t expected_kind);
struct socket_object* get_socket_by_index(int32_t socket_index);
struct socket_object* get_socket_for_fd(uint64_t fd);
void release_file_handle(int32_t handle_index);
void retain_fd_entry_refs(struct fd_entry* entry);
void release_pipe_ref(int32_t pipe_index, uint8_t kind);
void release_socket_ref(int32_t socket_index);
void close_socket_ref(int32_t socket_index);
void close_fd_internal(uint64_t fd);
int64_t install_vnode_fd(const struct vfs_node* node, uint64_t open_flags);
int64_t install_device_fd(uint8_t kind, int32_t device_id);
uint16_t allocate_socket_port(void);
int64_t install_socket_fd(int family, int type, int protocol);
int64_t dup_fd_common(uint64_t oldfd, int64_t requested_newfd, int overwrite);
int64_t resolve_path_from_dirfd(int64_t dirfd, const char* path, char* out, size_t out_size);
int64_t open_path_with_flags(const char* path, uint64_t flags);
int64_t exec_initrd_program(const char* normalized, const uint64_t* argv_user, const uint64_t* envp_user);
void process_exit_and_wake_parent(int exit_code);
void syscall_release_fd_table_entries(struct fd_entry* table, size_t count);
void process_load_fs_base(uint64_t fs_base);
int64_t socket_send_data(struct socket_object* sock, const uint8_t* data, uint64_t len);
int64_t socket_recv_data(struct socket_object* sock, uint8_t* dst, uint64_t len);
int socket_send_fin_close(struct socket_object* sock);

#endif
