#include "syscall_internal.h"

/* Included by kernel/syscall.c. Feature syscall group. */

static int wait_pid_matches(int64_t requested_pid, uint32_t child_pid) {
    return requested_pid == -1 || requested_pid == (int64_t)child_pid;
}

static int write_process_u32(process_t* process, uint64_t user_address,
                             uint32_t value) {
    if (!process || user_address == 0 ||
        vmm_prepare_user_write(process->p4_table, user_address,
                               sizeof(value)) != 0) {
        return -1;
    }
    *(uint32_t*)(uintptr_t)user_address = value;
    return 0;
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
        } else if (table[i].kind == FD_KIND_EVENTFD) {
            release_eventfd_ref(table[i].handle_index);
        }
        table[i].kind = FD_KIND_FREE;
        table[i].handle_index = -1;
    }
}

int64_t sys_wait4(struct syscall_regs* regs) {
    const uint64_t wait_nohang = 1;
    int64_t requested_pid = (int64_t)regs->rdi;
    int32_t* status_ptr = (int32_t*)(uintptr_t)regs->rsi;
    uint64_t options = regs->rdx;
    process_t* parent = current_process;
    process_t* first_ready = NULL;
    process_t* zombie = NULL;
    int has_matching_child = 0;

    if (!parent) return -(int64_t)LINUX_ECHILD;
    if ((options & ~wait_nohang) != 0) return -(int64_t)LINUX_EINVAL;
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
            if (write_process_u32(parent, (uint64_t)(uintptr_t)status_ptr,
                                  (uint32_t)zombie->exit_status) != 0) {
                return -(int64_t)LINUX_EFAULT;
            }
        }
        local_memset(zombie, 0, sizeof(*zombie));
        zombie->status = PROCESS_STATUS_DEAD;
        return (int64_t)child_pid;
    }

    if (!has_matching_child) {
        return -(int64_t)LINUX_ECHILD;
    }
    if (options & wait_nohang) {
        return 0;
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
    process_load_fs_base(first_ready->fs_base);
    switch_to_process(first_ready);
    return -(int64_t)LINUX_EINTR;
}

static void release_process_memory(process_t* proc) {
    uint64_t* address_space;

    if (!proc || !proc->p4_table) return;
    address_space = proc->p4_table;
    proc->p4_table = NULL;
    process_release_address_space(address_space, proc);
}

static process_t* find_process_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status == PROCESS_STATUS_DEAD) continue;
        if (process_list[i].pid == pid) return &process_list[i];
    }
    return NULL;
}

static process_t* find_parent_for(process_t* child) {
    if (!child) return NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status == PROCESS_STATUS_DEAD) continue;
        if (process_list[i].pid == child->parent_pid) return &process_list[i];
    }
    return NULL;
}

static int wake_parent_if_waiting(process_t* child, int32_t raw_status) {
    process_t* parent = find_parent_for(child);

    if (!parent || parent->status != PROCESS_STATUS_WAITING) return 0;
    if (!wait_pid_matches(parent->wait_target_pid, child->pid)) return 0;

    if (parent->wait_status_ptr) {
        if (vmm_prepare_user_write(
                parent->p4_table,
                (uint64_t)(uintptr_t)parent->wait_status_ptr,
                sizeof(*parent->wait_status_ptr)) != 0) {
            parent->regs.rax = (uint64_t)-(int64_t)LINUX_EFAULT;
            parent->status = PROCESS_STATUS_READY;
            parent->wait_target_pid = -1;
            parent->wait_status_ptr = NULL;
            return 1;
        }
        switch_page_table(parent->p4_table);
        *parent->wait_status_ptr = raw_status;
        if (current_process && current_process->p4_table) {
            switch_page_table(current_process->p4_table);
        }
    }
    parent->regs.rax = child->pid;
    parent->status = PROCESS_STATUS_READY;
    parent->wait_target_pid = -1;
    parent->wait_status_ptr = NULL;
    return 1;
}

void process_exit_and_wake_parent(int exit_code) {
    process_t* child = current_process;
    process_t* parent = NULL;
    uint32_t child_pid;
    int32_t raw_status;
    int wait_status_error = 0;

    if (!child) {
        halt_forever();
    }

    child_pid = child->pid;
    raw_status = (exit_code & 0xFF) << 8;
    process_complete_vfork(child);
    if (child->clear_child_tid) {
        (void)write_process_u32(child, child->clear_child_tid, 0);
    }
    syscall_release_fd_table_entries(child->fd_table, PROCESS_FD_MAX);

    if (child->is_thread) {
        process_t* next = NULL;
        uint32_t thread_group_id = child->thread_group_id;

        release_process_memory(child);
        local_memset(child, 0, sizeof(*child));
        child->status = PROCESS_STATUS_DEAD;
        for (int pass = 0; pass < 2 && !next; pass++) {
            for (int i = 0; i < MAX_PROCESSES; i++) {
                process_t* candidate = &process_list[i];
                if (candidate->status != PROCESS_STATUS_READY) continue;
                if (pass == 0 && candidate->thread_group_id != thread_group_id) {
                    continue;
                }
                next = candidate;
                break;
            }
        }
        if (next) {
            current_process = next;
            next->status = PROCESS_STATUS_RUNNING;
            switch_page_table(next->p4_table);
            process_load_fs_base(next->fs_base);
            switch_to_process(next);
        }
        halt_forever();
    }

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
            if (vmm_prepare_user_write(
                    parent->p4_table,
                    (uint64_t)(uintptr_t)parent->wait_status_ptr,
                    sizeof(*parent->wait_status_ptr)) != 0) {
                wait_status_error = 1;
                parent->wait_status_ptr = NULL;
            }
            switch_page_table(parent->p4_table);
            if (parent->wait_status_ptr) {
                *parent->wait_status_ptr = raw_status;
            }
        }
        switch_page_table(parent->p4_table);
        release_process_memory(child);
        parent->regs.rax = wait_status_error
                               ? (uint64_t)-(int64_t)LINUX_EFAULT
                               : child_pid;
        parent->status = PROCESS_STATUS_RUNNING;
        parent->wait_target_pid = -1;
        parent->wait_status_ptr = NULL;
        local_memset(child, 0, sizeof(*child));
        child->status = PROCESS_STATUS_DEAD;
        current_process = parent;
        process_load_fs_base(parent->fs_base);
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
        process_load_fs_base(parent->fs_base);
        switch_to_process(parent);
    }

    halt_forever();
}

void syscall_kill_current_process(int exit_code) {
    if (current_process && current_process->pid != 1) {
        process_exit_and_wake_parent(exit_code);
    }
}

int64_t sys_process_kill(struct syscall_regs* regs) {
    uint32_t pid = (uint32_t)regs->rdi;
    int32_t exit_code = (int32_t)(regs->rsi & 0xFF);
    int32_t raw_status = (exit_code & 0xFF) << 8;
    process_t* proc = find_process_by_pid(pid);

    if (!proc) return -(int64_t)LINUX_ENOENT;
    if (proc->pid == 1 || proc == current_process) return -(int64_t)LINUX_EPERM;
    if (!process_is_root() && proc->uid != process_get_uid()) {
        return -(int64_t)LINUX_EPERM;
    }
    if (proc->status == PROCESS_STATUS_ZOMBIE) return 0;

    syscall_release_fd_table_entries(proc->fd_table, PROCESS_FD_MAX);
    release_process_memory(proc);
    proc->exit_status = raw_status;
    proc->status = PROCESS_STATUS_ZOMBIE;
    if (wake_parent_if_waiting(proc, raw_status)) {
        local_memset(proc, 0, sizeof(*proc));
        proc->status = PROCESS_STATUS_DEAD;
    }
    return 0;
}

void syscall_terminate_uid_processes(uint32_t uid, uint32_t except_pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = &process_list[i];
        if (proc->status == PROCESS_STATUS_DEAD || proc->pid == except_pid ||
            proc->uid != uid) {
            continue;
        }
        syscall_release_fd_table_entries(proc->fd_table, PROCESS_FD_MAX);
        release_process_memory(proc);
        local_memset(proc, 0, sizeof(*proc));
        proc->status = PROCESS_STATUS_DEAD;
    }
}

int64_t sys_process_pause(struct syscall_regs* regs) {
    uint32_t pid = (uint32_t)regs->rdi;
    process_t* proc = find_process_by_pid(pid);

    if (!proc) return -(int64_t)LINUX_ENOENT;
    if (proc->pid == 1 || proc == current_process) return -(int64_t)LINUX_EPERM;
    if (proc->status == PROCESS_STATUS_READY) {
        proc->status = PROCESS_STATUS_PAUSED;
        return 0;
    }
    if (proc->status == PROCESS_STATUS_PAUSED) return 0;
    return -(int64_t)LINUX_EINVAL;
}

int64_t sys_process_resume(struct syscall_regs* regs) {
    uint32_t pid = (uint32_t)regs->rdi;
    process_t* proc = find_process_by_pid(pid);

    if (!proc) return -(int64_t)LINUX_ENOENT;
    if (proc->status == PROCESS_STATUS_PAUSED) {
        proc->status = PROCESS_STATUS_READY;
        return 0;
    }
    return -(int64_t)LINUX_EINVAL;
}
