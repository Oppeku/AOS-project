# AOS syscall layout

This is where syscall code lives after the feature split.

## Core

- `kernel/syscall.c`
  - shared FD tables, pipe/socket tables, DNS/ARP cache storage
  - common path helpers
  - common FD install/release helpers
  - exec stack/image loading helpers

- `kernel/syscall/syscall_internal.h`
  - shared constants
  - shared structs used across more than one syscall module
  - exported helper prototypes

- `kernel/syscall/dispatch.c`
  - syscall number routing only

## Linux-compatible syscall modules

- `kernel/syscall/fs.c`
  - `open`, `openat`, `access`, `faccessat`
  - `mkdir`, `mkdirat`, `unlinkat`
  - `read`, `readv`, `lseek`
  - `stat`, `fstat`, `newfstatat`
  - `getdents64`, `close`

- `kernel/syscall/terminal_io.c`
  - `write`, `writev`
  - `ioctl`
  - terminal termios/window-size compatibility

- `kernel/syscall/ipc.c`
  - `pipe`
  - `poll`

- `kernel/syscall/process_lifecycle.c`
  - `wait4`
  - process exit and parent wakeup
  - process FD cleanup

- `kernel/syscall/process_info.c`
  - `getpid`
  - `getppid`

- `kernel/syscall/memory.c`
  - `brk`
  - `mmap`
  - `mprotect`
  - `munmap`
  - `set_tid_address`

- `kernel/syscall/linux_misc.c`
  - `rt_sigaction`
  - `rt_sigprocmask`
  - `dup`, `dup2`, `fcntl`
  - `execve`
  - `getcwd`, `chdir`
  - `arch_prctl`
  - `uname`
  - `clock_gettime`
  - `getrandom`
  - `prlimit64`

## AOS syscall modules

- `kernel/syscall/network.c`
  - Linux socket syscalls: `socket`, `connect`, `sendto`, `recvfrom`
  - AOS net binding: `socket_bind_netdev`
  - kernel DNS lookup
  - TCP/ARP/DNS packet helpers

- `kernel/syscall/aos_services.c`
  - partition manager syscalls
  - block-device info
  - memory/uptime/display/time/user info
  - PCI/driver/netdev/firmware/wifi info
  - raw netdev send/recv
  - shutdown/restart

## Rule

If a helper is only used by one module, keep it in that module.
If a helper touches shared FD/process/socket state or is used by multiple modules, keep it in `syscall.c` and expose it through `syscall_internal.h`.
