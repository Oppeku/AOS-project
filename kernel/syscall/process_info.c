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
