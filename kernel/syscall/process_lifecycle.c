#include "syscall_internal.h"

/* Included by kernel/syscall.c. Feature syscall group. */

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

int64_t sys_wait4(struct syscall_regs* regs) {
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

void process_exit_and_wake_parent(int exit_code) {
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
