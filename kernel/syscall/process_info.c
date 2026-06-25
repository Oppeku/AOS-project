#include "syscall_internal.h"

/* Included by kernel/syscall.c. Feature syscall group. */

int64_t sys_getpid(struct syscall_regs* regs) {
    (void)regs;
    process_t* proc = get_current_process();
    return proc ? (int64_t)proc->pid : 1;
}

int64_t sys_getppid(struct syscall_regs* regs) {
    (void)regs;
    process_t* proc = get_current_process();
    if (!proc || proc->parent_pid == 0) {
        return 1;
    }
    return (int64_t)proc->parent_pid;
}

struct aos_process_info_user {
    uint8_t valid;
    uint8_t status;
    uint16_t reserved;
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t uid;
    uint32_t euid;
    int32_t exit_status;
    uint64_t brk_current;
    uint64_t mmap_next;
    char username[PROCESS_USERNAME_MAX];
    char command[PROCESS_COMMAND_MAX];
    char cwd[PROCESS_CWD_MAX];
} __attribute__((packed));

static void copy_process_string(char* dst, size_t dst_size, const char* src) {
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

int64_t sys_process_info(struct syscall_regs* regs) {
    uint64_t index = regs->rdi;
    struct aos_process_info_user* out = (struct aos_process_info_user*)(uintptr_t)regs->rsi;
    const process_t* proc;

    if (!out) return -(int64_t)LINUX_EFAULT;
    if (index >= MAX_PROCESSES) return -(int64_t)LINUX_ENOENT;

    local_memset(out, 0, sizeof(*out));
    proc = &process_list[index];
    if (proc->status == PROCESS_STATUS_DEAD) {
        return 0;
    }

    out->valid = 1;
    out->status = (uint8_t)proc->status;
    out->pid = proc->pid;
    out->parent_pid = proc->parent_pid;
    out->uid = proc->uid;
    out->euid = proc->euid;
    out->exit_status = proc->exit_status;
    out->brk_current = proc->brk_current;
    out->mmap_next = proc->mmap_next;
    copy_process_string(out->username, sizeof(out->username), proc->username);
    copy_process_string(out->command, sizeof(out->command), proc->command);
    copy_process_string(out->cwd, sizeof(out->cwd), proc->cwd);
    return 0;
}
