#include "syscall_internal.h"

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct linux_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

int64_t sys_rt_sigaction(struct syscall_regs* regs) {
    (void)regs->rdi;
    (void)regs->rsi;
    void* old_action = (void*)(uintptr_t)regs->rdx;
    (void)regs->r10;

    if (old_action) {
        local_memset(old_action, 0, 32);
    }
    return 0;
}

int64_t sys_rt_sigprocmask(struct syscall_regs* regs) {
    (void)regs->rdi;
    (void)regs->rsi;
    void* old_set = (void*)(uintptr_t)regs->rdx;
    (void)regs->r10;

    if (old_set) {
        local_memset(old_set, 0, 8);
    }
    return 0;
}

int64_t sys_dup(struct syscall_regs* regs) {
    return dup_fd_common(regs->rdi, -1, 0);
}

int64_t sys_dup2(struct syscall_regs* regs) {
    return dup_fd_common(regs->rdi, (int64_t)regs->rsi, 1);
}

int64_t sys_fcntl(struct syscall_regs* regs) {
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

int64_t sys_execve(struct syscall_regs* regs) {
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

int64_t sys_getcwd(struct syscall_regs* regs) {
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

int64_t sys_chdir(struct syscall_regs* regs) {
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

int64_t sys_arch_prctl(struct syscall_regs* regs) {
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

int64_t sys_uname(struct syscall_regs* regs) {
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

int64_t sys_clock_gettime(struct syscall_regs* regs) {
    struct linux_timespec* ts = (struct linux_timespec*)(uintptr_t)regs->rsi;
    uint64_t ticks;
    uint32_t frequency;
    uint64_t rem_ticks;

    (void)regs->rdi;
    if (!ts) return -(int64_t)LINUX_EFAULT;

    frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;
    ticks = timer_get_ticks();
    rem_ticks = ticks % frequency;

    ts->tv_sec = (int64_t)(ticks / frequency);
    ts->tv_nsec = (int64_t)((rem_ticks * 1000000000ULL) / frequency);
    return 0;
}

int64_t sys_nanosleep(struct syscall_regs* regs) {
    const struct linux_timespec* req = (const struct linux_timespec*)(uintptr_t)regs->rdi;
    struct linux_timespec* rem = (struct linux_timespec*)(uintptr_t)regs->rsi;
    uint32_t frequency;
    uint64_t seconds_ticks;
    uint64_t nanos_ticks;
    uint64_t wait_ticks;
    uint64_t start;

    if (!req) return -(int64_t)LINUX_EFAULT;
    if (req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000LL) {
        return -(int64_t)LINUX_EINVAL;
    }

    frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;

    seconds_ticks = (uint64_t)req->tv_sec * (uint64_t)frequency;
    nanos_ticks = ((uint64_t)req->tv_nsec * (uint64_t)frequency + 999999999ULL) / 1000000000ULL;
    wait_ticks = seconds_ticks + nanos_ticks;

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    if (wait_ticks == 0) return 0;

    start = timer_get_ticks();
    while (timer_get_ticks() - start < wait_ticks) {
        asm volatile("hlt");
    }

    return 0;
}

int64_t sys_getrandom(struct syscall_regs* regs) {
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rdi;
    uint64_t len = regs->rsi;
    (void)regs->rdx;
    if (!buf && len != 0) return -(int64_t)LINUX_EFAULT;
    for (uint64_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    }
    return (int64_t)len;
}

int64_t sys_prlimit64(struct syscall_regs* regs) {
    void* old_limit = (void*)(uintptr_t)regs->r10;
    (void)regs->rdi;
    (void)regs->rsi;
    (void)regs->rdx;
    if (old_limit) {
        local_memset(old_limit, 0, 16);
    }
    return 0;
}
