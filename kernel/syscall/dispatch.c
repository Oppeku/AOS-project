/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>
#include <stddef.h>
#include <syscall.h>
#include <process.h>

extern void serial_print(const char* s);
int64_t exec_initrd_program(const char* path, const uint64_t* argv_user, const uint64_t* envp_user);
void process_exit_and_wake_parent(int exit_code);

int64_t sys_access(struct syscall_regs* regs);
int64_t sys_arch_prctl(struct syscall_regs* regs);
int64_t sys_arp_cache_info(struct syscall_regs* regs);
int64_t sys_blkdev_info(struct syscall_regs* regs);
int64_t sys_brk(struct syscall_regs* regs);
int64_t sys_chdir(struct syscall_regs* regs);
int64_t sys_clock_gettime(struct syscall_regs* regs);
int64_t sys_clone(struct syscall_regs* regs);
int64_t sys_close(struct syscall_regs* regs);
int64_t sys_connect(struct syscall_regs* regs);
int64_t sys_display_info(struct syscall_regs* regs);
int64_t sys_display_set(struct syscall_regs* regs);
int64_t sys_dns_cache_info(struct syscall_regs* regs);
int64_t sys_dns_lookup(struct syscall_regs* regs);
int64_t sys_dns_lookup6(struct syscall_regs* regs);
int64_t sys_driver_info(struct syscall_regs* regs);
int64_t sys_dup(struct syscall_regs* regs);
int64_t sys_dup2(struct syscall_regs* regs);
int64_t sys_execve(struct syscall_regs* regs);
int64_t sys_faccessat(struct syscall_regs* regs);
int64_t sys_fcntl(struct syscall_regs* regs);
int64_t sys_firmware_info(struct syscall_regs* regs);
int64_t sys_fork(struct syscall_regs* regs);
int64_t sys_fstat(struct syscall_regs* regs);
int64_t sys_getcwd(struct syscall_regs* regs);
int64_t sys_getdents64(struct syscall_regs* regs);
int64_t sys_getpid(struct syscall_regs* regs);
int64_t sys_getppid(struct syscall_regs* regs);
int64_t sys_getrandom(struct syscall_regs* regs);
int64_t sys_gfx_clear(struct syscall_regs* regs);
int64_t sys_gfx_info(struct syscall_regs* regs);
int64_t sys_gfx_pixel(struct syscall_regs* regs);
int64_t sys_gfx_present(struct syscall_regs* regs);
int64_t sys_gfx_rect(struct syscall_regs* regs);
int64_t sys_input_poll(struct syscall_regs* regs);
int64_t sys_ioctl(struct syscall_regs* regs);
int64_t sys_lseek(struct syscall_regs* regs);
int64_t sys_mem_info(struct syscall_regs* regs);
int64_t sys_mkdir(struct syscall_regs* regs);
int64_t sys_mkdirat(struct syscall_regs* regs);
int64_t sys_mmap(struct syscall_regs* regs);
int64_t sys_mount_info(struct syscall_regs* regs);
int64_t sys_mprotect(struct syscall_regs* regs);
int64_t sys_munmap(struct syscall_regs* regs);
int64_t sys_net_cache_flush(struct syscall_regs* regs);
int64_t sys_netdev_info(struct syscall_regs* regs);
int64_t sys_netdev_ipv4_config(struct syscall_regs* regs);
int64_t sys_netdev_ipv6_config(struct syscall_regs* regs);
int64_t sys_netdev_recv(struct syscall_regs* regs);
int64_t sys_netdev_send(struct syscall_regs* regs);
int64_t sys_netdev_stats(struct syscall_regs* regs);
int64_t sys_ndp_cache_info(struct syscall_regs* regs);
int64_t sys_newfstatat(struct syscall_regs* regs);
int64_t sys_open(struct syscall_regs* regs);
int64_t sys_openat(struct syscall_regs* regs);
int64_t sys_partition_create(struct syscall_regs* regs);
int64_t sys_partition_delete(struct syscall_regs* regs);
int64_t sys_partition_info(struct syscall_regs* regs);
int64_t sys_partition_layout(struct syscall_regs* regs);
int64_t sys_partition_role(struct syscall_regs* regs);
int64_t sys_partition_type(struct syscall_regs* regs);
int64_t sys_partition_write(struct syscall_regs* regs);
int64_t sys_pci_info(struct syscall_regs* regs);
int64_t sys_pipe(struct syscall_regs* regs);
int64_t sys_poll(struct syscall_regs* regs);
int64_t sys_prlimit64(struct syscall_regs* regs);
int64_t sys_read(struct syscall_regs* regs);
int64_t sys_readv(struct syscall_regs* regs);
int64_t sys_recvfrom(struct syscall_regs* regs);
int64_t sys_restart(struct syscall_regs* regs);
int64_t sys_rt_sigaction(struct syscall_regs* regs);
int64_t sys_rt_sigprocmask(struct syscall_regs* regs);
int64_t sys_sendto(struct syscall_regs* regs);
int64_t sys_set_tid_address(struct syscall_regs* regs);
int64_t sys_shutdown(struct syscall_regs* regs);
int64_t sys_socket(struct syscall_regs* regs);
int64_t sys_socket_bind_netdev(struct syscall_regs* regs);
int64_t sys_socket_info(struct syscall_regs* regs);
int64_t sys_stat(struct syscall_regs* regs);
int64_t sys_sudo_auth(struct syscall_regs* regs);
int64_t sys_time_info(struct syscall_regs* regs);
int64_t sys_uname(struct syscall_regs* regs);
int64_t sys_unlinkat(struct syscall_regs* regs);
int64_t sys_uptime_info(struct syscall_regs* regs);
int64_t sys_user_info(struct syscall_regs* regs);
int64_t sys_wait4(struct syscall_regs* regs);
int64_t sys_wifi_control(struct syscall_regs* regs);
int64_t sys_wifi_scan_info(struct syscall_regs* regs);
int64_t sys_wifi_state_info(struct syscall_regs* regs);
int64_t sys_write(struct syscall_regs* regs);
int64_t sys_writev(struct syscall_regs* regs);


void halt_forever(void) {
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
        case AOS_SYS_NDP_CACHE_INFO:
            regs->rax = (uint64_t)sys_ndp_cache_info(regs);
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
        case AOS_SYS_DNS_LOOKUP6:
            regs->rax = (uint64_t)sys_dns_lookup6(regs);
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
