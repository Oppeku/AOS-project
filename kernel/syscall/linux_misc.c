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

struct linux_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct linux_timezone {
    int32_t tz_minuteswest;
    int32_t tz_dsttime;
};

struct linux_stack {
    void* pointer;
    int32_t flags;
    uint32_t reserved;
    uint64_t size;
};

static int year_is_leap(uint16_t year) {
    return (year % 4U == 0U && year % 100U != 0U) ||
           year % 400U == 0U;
}

static int rtc_to_epoch_seconds(const struct rtc_time* rtc,
                                int64_t* seconds_out) {
    static const uint8_t month_days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    uint64_t days = 0;

    if (!rtc || !seconds_out || rtc->year < 1970 || rtc->month < 1 ||
        rtc->month > 12 || rtc->day < 1 || rtc->hour > 23 ||
        rtc->minute > 59 || rtc->second > 59) {
        return -1;
    }
    for (uint16_t year = 1970; year < rtc->year; year++) {
        days += year_is_leap(year) ? 366U : 365U;
    }
    for (uint8_t month = 1; month < rtc->month; month++) {
        days += month_days[month - 1];
        if (month == 2 && year_is_leap(rtc->year)) days++;
    }
    {
        uint8_t max_day = month_days[rtc->month - 1];
        if (rtc->month == 2 && year_is_leap(rtc->year)) max_day++;
        if (rtc->day > max_day) return -1;
    }
    days += (uint64_t)rtc->day - 1U;
    *seconds_out = (int64_t)(days * 86400ULL +
                             (uint64_t)rtc->hour * 3600ULL +
                             (uint64_t)rtc->minute * 60ULL + rtc->second);
    return 0;
}

static int64_t realtime_seconds(void) {
    struct rtc_time rtc;
    int64_t seconds;

    if (rtc_read_time(&rtc) != 0 || rtc_to_epoch_seconds(&rtc, &seconds) != 0) {
        return -1;
    }
    return seconds;
}

static int cpu_has_rdrand(void) {
    static int cached = -1;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    if (cached >= 0) return cached;
    eax = 1;
    asm volatile("cpuid"
                 : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ebx;
    (void)edx;
    cached = (ecx & (1U << 30)) != 0;
    return cached;
}

static int read_hardware_random(uint64_t* value) {
    uint8_t success;

    if (!value || !cpu_has_rdrand()) return 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        asm volatile("rdrand %0; setc %1"
                     : "=r"(*value), "=qm"(success));
        if (success) return 1;
    }
    return 0;
}

void linux_random_fill(uint8_t* buffer, size_t length) {
    static uint64_t fallback_state = 0xA05C0FFEE1234567ULL;
    uint64_t timestamp;
    uint32_t timestamp_low;
    uint32_t timestamp_high;

    if (!buffer) return;
    asm volatile("rdtsc" : "=a"(timestamp_low), "=d"(timestamp_high));
    timestamp = ((uint64_t)timestamp_high << 32) | timestamp_low;
    fallback_state ^= timestamp ^ timer_get_ticks() ^
                      (uint64_t)(uintptr_t)buffer;
    while (length != 0) {
        uint64_t value;
        size_t chunk = length < sizeof(value) ? length : sizeof(value);

        if (!read_hardware_random(&value)) {
            fallback_state ^= fallback_state << 13;
            fallback_state ^= fallback_state >> 7;
            fallback_state ^= fallback_state << 17;
            value = fallback_state;
        } else {
            fallback_state ^= value;
        }
        local_memcpy(buffer, &value, chunk);
        buffer += chunk;
        length -= chunk;
    }
}

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

int64_t sys_sigaltstack(struct syscall_regs* regs) {
    const struct linux_stack* new_stack =
        (const struct linux_stack*)(uintptr_t)regs->rdi;
    struct linux_stack* old_stack =
        (struct linux_stack*)(uintptr_t)regs->rsi;
    process_t* process = get_current_process();

    if (!process) return -(int64_t)LINUX_ESRCH;
    if (old_stack) {
        old_stack->pointer = (void*)(uintptr_t)process->signal_stack_pointer;
        old_stack->size = process->signal_stack_size;
        old_stack->flags = process->signal_stack_pointer
                               ? (int32_t)process->signal_stack_flags
                               : LINUX_SS_DISABLE;
        old_stack->reserved = 0;
    }
    if (!new_stack) return 0;
    if (((uint32_t)new_stack->flags &
         ~(LINUX_SS_DISABLE | LINUX_SS_AUTODISARM)) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((uint32_t)new_stack->flags & LINUX_SS_DISABLE) {
        if ((uint32_t)new_stack->flags != LINUX_SS_DISABLE) {
            return -(int64_t)LINUX_EINVAL;
        }
        process->signal_stack_pointer = 0;
        process->signal_stack_size = 0;
        process->signal_stack_flags = 0;
        return 0;
    }
    if (!new_stack->pointer) return -(int64_t)LINUX_EFAULT;
    if (new_stack->size < LINUX_MINSIGSTKSZ) {
        return -(int64_t)LINUX_ENOMEM;
    }
    process->signal_stack_pointer = (uint64_t)(uintptr_t)new_stack->pointer;
    process->signal_stack_size = new_stack->size;
    process->signal_stack_flags =
        (uint32_t)new_stack->flags & LINUX_SS_AUTODISARM;
    return 0;
}

int64_t sys_setsid(struct syscall_regs* regs) {
    process_t* process = get_current_process();
    (void)regs;

    if (!process) return -(int64_t)LINUX_ESRCH;
    if (process->process_group_id == process->pid) {
        return -(int64_t)LINUX_EPERM;
    }
    process->session_id = process->pid;
    process->process_group_id = process->pid;
    return process->pid;
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
            if (entry->kind == FD_KIND_EVENTFD) {
                struct eventfd_object* eventfd =
                    get_eventfd_object_by_index(entry->handle_index);
                if (!eventfd) return -(int64_t)LINUX_EBADF;
                return eventfd->nonblocking ? LINUX_EFD_NONBLOCK : 0;
            }
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
    int64_t clock_id = (int64_t)regs->rdi;
    uint64_t ticks;
    uint32_t frequency;
    uint64_t rem_ticks;

    if (!ts) return -(int64_t)LINUX_EFAULT;

    if (clock_id == 0 || clock_id == 5) {
        int64_t seconds = realtime_seconds();
        if (seconds < 0) return -(int64_t)LINUX_EIO;
        ts->tv_sec = seconds;
        ts->tv_nsec = 0;
        return 0;
    }
    if (clock_id < 1 || clock_id > 7) {
        return -(int64_t)LINUX_EINVAL;
    }

    frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;
    ticks = timer_get_ticks();
    rem_ticks = ticks % frequency;

    ts->tv_sec = (int64_t)(ticks / frequency);
    ts->tv_nsec = (int64_t)((rem_ticks * 1000000000ULL) / frequency);
    return 0;
}

int64_t sys_gettimeofday(struct syscall_regs* regs) {
    struct linux_timeval* tv =
        (struct linux_timeval*)(uintptr_t)regs->rdi;
    struct linux_timezone* tz =
        (struct linux_timezone*)(uintptr_t)regs->rsi;
    int64_t seconds;

    if (tv) {
        seconds = realtime_seconds();
        if (seconds < 0) return -(int64_t)LINUX_EIO;
        tv->tv_sec = seconds;
        tv->tv_usec = 0;
    }
    if (tz) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }
    return 0;
}

int64_t sys_gettid(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    (void)regs;
    return proc ? (int64_t)proc->pid : 0;
}

int64_t sys_futex(struct syscall_regs* regs) {
    volatile uint32_t* uaddr =
        (volatile uint32_t*)(uintptr_t)regs->rdi;
    uint32_t command = (uint32_t)regs->rsi & LINUX_FUTEX_CMD_MASK;
    uint32_t expected = (uint32_t)regs->rdx;
    uint32_t bitset = (uint32_t)regs->r9;

    if (!uaddr) return -(int64_t)LINUX_EFAULT;
    if (((uintptr_t)uaddr & (sizeof(uint32_t) - 1U)) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }

    switch (command) {
        case LINUX_FUTEX_WAIT:
        case LINUX_FUTEX_WAIT_BITSET:
            if (command == LINUX_FUTEX_WAIT_BITSET && bitset == 0) {
                return -(int64_t)LINUX_EINVAL;
            }
            if (*uaddr != expected) return -(int64_t)LINUX_EAGAIN;
            /* Yield once before a permitted spurious wake so a newly cloned
             * thread can run in the shared address space. */
            regs->rax = 0;
            schedule(regs);
            return 0;
        case LINUX_FUTEX_WAKE:
        case LINUX_FUTEX_WAKE_BITSET:
            if (command == LINUX_FUTEX_WAKE_BITSET && bitset == 0) {
                return -(int64_t)LINUX_EINVAL;
            }
            return 0;
        case LINUX_FUTEX_REQUEUE:
            return 0;
        case LINUX_FUTEX_CMP_REQUEUE:
            return *uaddr == bitset ? 0 : -(int64_t)LINUX_EAGAIN;
        default:
            return -(int64_t)LINUX_ENOSYS;
    }
}

static process_t* find_linux_process(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status != PROCESS_STATUS_DEAD &&
            process_list[i].pid == pid) {
            return &process_list[i];
        }
    }
    return NULL;
}

#define LINUX_PRIO_PROCESS 0
#define LINUX_PRIO_PGRP 1
#define LINUX_PRIO_USER 2

static int process_matches_priority_selector(const process_t* process,
                                             int which, uint32_t who) {
    const process_t* current = get_current_process();

    if (!process || !current || process->status == PROCESS_STATUS_DEAD ||
        process->status == PROCESS_STATUS_ZOMBIE) {
        return 0;
    }
    switch (which) {
        case LINUX_PRIO_PROCESS:
            return process->pid == (who ? who : current->pid);
        case LINUX_PRIO_PGRP:
            return process->process_group_id ==
                   (who ? who : current->process_group_id);
        case LINUX_PRIO_USER:
            return process->uid == (who ? who : current->uid);
        default:
            return 0;
    }
}

int64_t sys_getpriority(struct syscall_regs* regs) {
    int which = (int)(int32_t)regs->rdi;
    uint32_t who = (uint32_t)regs->rsi;
    int best_nice = 20;
    int matched = 0;

    if (which < LINUX_PRIO_PROCESS || which > LINUX_PRIO_USER) {
        return -(int64_t)LINUX_EINVAL;
    }
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!process_matches_priority_selector(&process_list[i], which, who)) {
            continue;
        }
        matched = 1;
        if ((int)process_list[i].nice_value < best_nice) {
            best_nice = process_list[i].nice_value;
        }
    }
    if (!matched) return -(int64_t)LINUX_ESRCH;

    /* The raw Linux syscall returns this biased value; libc subtracts it. */
    return 20 - best_nice;
}

int64_t sys_setpriority(struct syscall_regs* regs) {
    int which = (int)(int32_t)regs->rdi;
    uint32_t who = (uint32_t)regs->rsi;
    int priority = (int)(int32_t)regs->rdx;
    process_t* current = get_current_process();
    int matched = 0;

    if (!current) return -(int64_t)LINUX_ESRCH;
    if (which < LINUX_PRIO_PROCESS || which > LINUX_PRIO_USER) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (priority < -20) priority = -20;
    if (priority > 19) priority = 19;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* target = &process_list[i];

        if (!process_matches_priority_selector(target, which, who)) continue;
        matched = 1;
        if (!process_is_root() &&
            (target->uid != current->euid || priority < target->nice_value)) {
            return -(int64_t)LINUX_EPERM;
        }
    }
    if (!matched) return -(int64_t)LINUX_ESRCH;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* target = &process_list[i];
        if (process_matches_priority_selector(target, which, who)) {
            target->nice_value = (int8_t)priority;
        }
    }
    return 0;
}

static int signal_is_fatal(int signal) {
    return signal == 4 || signal == 6 || signal == 7 || signal == 8 ||
           signal == 9 || signal == 11 || signal == 15;
}

int64_t sys_tgkill(struct syscall_regs* regs) {
    int64_t tgid = linux_signed_int_arg(regs->rdi);
    int64_t tid = linux_signed_int_arg(regs->rsi);
    int signal = (int)linux_signed_int_arg(regs->rdx);
    process_t* target;

    if (signal < 0 || signal > 64) return -(int64_t)LINUX_EINVAL;
    if (tgid <= 0 || tid <= 0) return -(int64_t)LINUX_ESRCH;
    target = find_linux_process((uint32_t)tid);
    if (!target ||
        (target->thread_group_id ? target->thread_group_id : target->pid) !=
            (uint32_t)tgid) {
        return -(int64_t)LINUX_ESRCH;
    }
    if (!process_is_root() && target->uid != process_get_euid()) {
        return -(int64_t)LINUX_EPERM;
    }
    if (signal == 0) return 0;
    if (target == get_current_process() && target->pid != 1 &&
        signal_is_fatal(signal)) {
        process_exit_and_wake_parent(128 + signal);
    }
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
        asm volatile("sti; hlt; cli" ::: "memory");
    }

    return 0;
}

int64_t sys_getrandom(struct syscall_regs* regs) {
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rdi;
    uint64_t len = regs->rsi;
    (void)regs->rdx;
    if (!buf && len != 0) return -(int64_t)LINUX_EFAULT;
    linux_random_fill(buf, len);
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
