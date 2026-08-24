#include <process.h>
#include <pmm.h>
#include <vmm.h>
#include <timer.h>
#include <thermal.h>
#include <xhci.h>
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);
extern uint64_t p4_table[];
extern void syscall_retain_fd_table_entries(struct fd_entry* table, size_t count);
extern void syscall_release_fd_table_entries(struct fd_entry* table, size_t count);
extern void syscall_init_process_fd_table(struct fd_entry* table, size_t count);
extern void process_load_fs_base(uint64_t fs_base);
extern void switch_page_table(uint64_t* table);

process_t process_list[MAX_PROCESSES];
process_t* current_process = NULL;
static uint32_t next_pid = 1;
static uint8_t g_thermal_policy_active;

#define LINUX_CLONE_VM 0x00000100ULL
#define LINUX_CLONE_VFORK 0x00004000ULL
#define LINUX_CLONE_THREAD 0x00010000ULL
#define LINUX_CLONE_SETTLS 0x00080000ULL
#define LINUX_CLONE_PARENT_SETTID 0x00100000ULL
#define LINUX_CLONE_CHILD_CLEARTID 0x00200000ULL
#define LINUX_CLONE_CHILD_SETTID 0x01000000ULL

static void local_strcpy_bounded(char* dst, size_t dst_size, const char* src) {
    size_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1 < dst_size) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void* local_memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = dest;
    const uint8_t* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static void* local_memset(void* dest, int val, size_t n) {
    uint8_t* d = dest;
    while (n--) *d++ = (uint8_t)val;
    return dest;
}

static void serial_print_hex64(uint64_t value) {
    char text[19];

    text[0] = '0';
    text[1] = 'x';
    for (int i = 0; i < 16; i++) {
        uint8_t digit = (uint8_t)((value >> (60 - i * 4)) & 0xF);
        text[2 + i] = (char)(digit < 10 ? '0' + digit
                                       : 'A' + digit - 10);
    }
    text[18] = '\0';
    serial_print(text);
}

static int local_streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static const char* command_basename(const char* command) {
    const char* base = command ? command : "";

    for (const char* p = base; *p; p++) {
        if (*p == '/') base = p + 1;
    }
    return base;
}

static int process_is_thermal_essential(const process_t* proc) {
    const char* command;

    if (!proc || proc->pid == 1) return 1;
    command = command_basename(proc->command);
    return local_streq(command, "desktop.elf") ||
           local_streq(command, "desktop") ||
           local_streq(command, "dextop") ||
           local_streq(command, "dm") ||
           local_streq(command, "mui") ||
           local_streq(command, "mui.elf") ||
           local_streq(command, "MUI") ||
           local_streq(command, "shutdown") ||
           local_streq(command, "shutdown.elf") ||
           local_streq(command, "restart") ||
           local_streq(command, "restart.elf") ||
           local_streq(command, "reboot");
}

static process_t* process_select_next_ready(process_t* current) {
    int start_idx = 0;
    process_t* throttled_fallback = NULL;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (&process_list[i] == current) {
            start_idx = i + 1;
            break;
        }
    }

    for (int i = 0; i < MAX_PROCESSES; i++) {
        int idx = (start_idx + i) % MAX_PROCESSES;
        process_t* candidate = &process_list[idx];

        if (candidate->status != PROCESS_STATUS_READY) continue;
        if (candidate->thermal_throttled) {
            if (!throttled_fallback) throttled_fallback = candidate;
            candidate->thermal_skip_phase =
                (uint8_t)((candidate->thermal_skip_phase + 1U) & 3U);
            if (candidate->thermal_skip_phase != 0) continue;
        }
        return candidate;
    }
    return throttled_fallback;
}

static void process_save_interrupt_context(
    process_t* proc, const struct process_interrupt_frame* frame) {
    proc->regs.rax = frame->rax;
    proc->regs.rdx = frame->rdx;
    proc->regs.rsi = frame->rsi;
    proc->regs.rdi = frame->rdi;
    proc->regs.r10 = frame->r10;
    proc->regs.r8 = frame->r8;
    proc->regs.r9 = frame->r9;
    proc->regs.r15 = frame->r15;
    proc->regs.r14 = frame->r14;
    proc->regs.r13 = frame->r13;
    proc->regs.r12 = frame->r12;
    proc->regs.rbx = frame->rbx;
    proc->regs.rbp = frame->rbp;
    proc->regs.rcx = frame->rip;
    proc->regs.r11 = frame->rflags;
    proc->regs.rsp = frame->rsp;
    proc->interrupted_rcx = frame->rcx;
    proc->interrupted_r11 = frame->r11;
    proc->has_interrupt_context = 1;
}

static void process_restore_interrupt_context(
    struct process_interrupt_frame* frame, const process_t* proc) {
    frame->rax = proc->regs.rax;
    frame->rdx = proc->regs.rdx;
    frame->rsi = proc->regs.rsi;
    frame->rdi = proc->regs.rdi;
    frame->r10 = proc->regs.r10;
    frame->r8 = proc->regs.r8;
    frame->r9 = proc->regs.r9;
    frame->r15 = proc->regs.r15;
    frame->r14 = proc->regs.r14;
    frame->r13 = proc->regs.r13;
    frame->r12 = proc->regs.r12;
    frame->rbx = proc->regs.rbx;
    frame->rbp = proc->regs.rbp;
    frame->rcx = proc->has_interrupt_context
                     ? proc->interrupted_rcx
                     : proc->regs.rcx;
    frame->r11 = proc->has_interrupt_context
                     ? proc->interrupted_r11
                     : proc->regs.r11;
    frame->rip = proc->regs.rcx;
    frame->rflags = proc->regs.r11 | 0x200ULL;
    frame->rsp = proc->regs.rsp;
}

static void process_preempt_from_interrupt(
    struct process_interrupt_frame* frame) {
    process_t* previous;
    process_t* next;

    if (!frame || (frame->cs & 3U) != 3U || !current_process ||
        current_process->status != PROCESS_STATUS_RUNNING) {
        return;
    }

    previous = current_process;
    process_save_interrupt_context(previous, frame);
    previous->status = PROCESS_STATUS_READY;
    next = process_select_next_ready(previous);
    if (!next || next == previous) {
        previous->status = PROCESS_STATUS_RUNNING;
        return;
    }

    if (next->regs.rsp != 0 &&
        !vmm_page_present(next->p4_table, next->regs.rsp)) {
        serial_print("AOS: scheduler selected unmapped user stack ");
        serial_print_hex64(next->regs.rsp);
        serial_print("\n");
    }

    process_restore_interrupt_context(frame, next);
    current_process = next;
    next->status = PROCESS_STATUS_RUNNING;
    switch_page_table(next->p4_table);
    process_load_fs_base(next->fs_base);
}

void process_release_address_space(uint64_t* address_space,
                                   const process_t* releasing_process) {
    if (!address_space) return;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        const process_t* other = &process_list[i];
        if (other == releasing_process ||
            other->status == PROCESS_STATUS_DEAD) {
            continue;
        }
        if (other->p4_table == address_space) return;
    }
    vmm_destroy_user_space(address_space);
}

void process_complete_vfork(process_t* child) {
    uint32_t parent_pid;

    if (!child || child->vfork_parent_pid == 0) return;
    parent_pid = child->vfork_parent_pid;
    child->vfork_parent_pid = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* parent = &process_list[i];

        if (parent->status == PROCESS_STATUS_DEAD ||
            parent->pid != parent_pid ||
            parent->vfork_child_pid != child->pid) {
            continue;
        }
        parent->vfork_child_pid = 0;
        if (parent->status == PROCESS_STATUS_WAITING) {
            parent->status = PROCESS_STATUS_READY;
        }
        return;
    }
}

void schedule(struct syscall_regs* regs) {
    if (!current_process) return;

    // Save current state
    local_memcpy(&current_process->regs, regs, sizeof(struct syscall_regs));
    current_process->has_interrupt_context = 0;
    if (current_process->status == PROCESS_STATUS_RUNNING) {
        current_process->status = PROCESS_STATUS_READY;
    }

    // Pick next process
    process_t* next = process_select_next_ready(current_process);

    if (next && next != current_process) {
        current_process = next;
        next->status = PROCESS_STATUS_RUNNING;
        process_load_fs_base(next->fs_base);
        serial_print("AOS: Switching to PID=");
        // Manual hex print for PID (simplified)
        char pid_c = (char)('0' + (next->pid % 10));
        char pid_str[2] = {pid_c, 0};
        serial_print(pid_str);
        serial_print("\n");
        switch_to_process(next);
    } else {
        // No other process ready, continue with current
        if (current_process) current_process->status = PROCESS_STATUS_RUNNING;
    }
}

void timer_handler(struct process_interrupt_frame* frame) {
    timer_tick();
    if (current_process && current_process->status == PROCESS_STATUS_RUNNING) {
        current_process->cpu_ticks++;
    }
    if (thermal_timer_tick()) {
        process_update_thermal_policy(thermal_emergency_active());
    }
    xhci_poll_keyboard();
    process_preempt_from_interrupt(frame);
}

void init_process() {
    g_thermal_policy_active = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_list[i].status = PROCESS_STATUS_DEAD;
    }

    // Create the first process (the one already running)
    process_list[0].pid = next_pid++;
    process_list[0].parent_pid = 0;
    process_list[0].session_id = process_list[0].pid;
    process_list[0].process_group_id = process_list[0].pid;
    process_list[0].thread_group_id = process_list[0].pid;
    process_list[0].status = PROCESS_STATUS_RUNNING;
    process_list[0].exit_status = 0;
    process_list[0].p4_table = p4_table;
    local_memset(&process_list[0].regs, 0, sizeof(struct syscall_regs));
    process_list[0].cwd[0] = '\0';
    process_list[0].wait_target_pid = -1;
    process_list[0].wait_status_ptr = NULL;
    syscall_init_process_fd_table(process_list[0].fd_table, PROCESS_FD_MAX);
    process_list[0].fs_base = 0;
    process_list[0].brk_base = 0;
    process_list[0].brk_current = 0;
    process_list[0].brk_mapped_end = 0;
    process_list[0].mmap_next = 0;
    process_list[0].clear_child_tid = 0;
    /*
     * Live ISO mode starts as root until the installer creates the real user.
     * AOS_LIVE_PERMISSIVE keeps the live environment writable without sudo.
     */
    process_list[0].uid = 0;
    process_list[0].gid = 0;
    process_list[0].euid = 0;
    process_list[0].egid = 0;
    local_strcpy_bounded(process_list[0].username, sizeof(process_list[0].username), "root");
    local_strcpy_bounded(process_list[0].home, sizeof(process_list[0].home), "root");
    local_strcpy_bounded(process_list[0].command, sizeof(process_list[0].command), "desktop.elf");
    current_process = &process_list[0];
}

int64_t sys_fork(struct syscall_regs* regs) {
    serial_print("AOS: sys_fork called\n");

    int child_idx = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status == PROCESS_STATUS_DEAD) {
            child_idx = i;
            break;
        }
    }

    if (child_idx == -1) return -1;

    process_t* child = &process_list[child_idx];
    local_memset(child, 0, sizeof(*child));
    child->pid = next_pid++;
    child->parent_pid = current_process->pid;
    child->session_id = current_process->session_id;
    child->process_group_id = current_process->process_group_id;
    child->thread_group_id = child->pid;
    child->exit_status = 0;

    // 1. Copy Address Space
    child->p4_table = vmm_copy_p4(current_process->p4_table);
    if (!child->p4_table) {
        local_memset(child, 0, sizeof(*child));
        child->status = PROCESS_STATUS_DEAD;
        return -(int64_t)LINUX_ENOMEM;
    }

    // 2. Copy Registers
    local_memcpy(&child->regs, regs, sizeof(struct syscall_regs));
    local_memcpy(child->cwd, current_process->cwd, sizeof(child->cwd));
    child->wait_target_pid = -1;
    child->wait_status_ptr = NULL;
    local_memcpy(child->fd_table, current_process->fd_table, sizeof(child->fd_table));
    syscall_retain_fd_table_entries(child->fd_table, PROCESS_FD_MAX);
    child->fs_base = current_process->fs_base;
    child->brk_base = current_process->brk_base;
    child->brk_current = current_process->brk_current;
    child->brk_mapped_end = current_process->brk_mapped_end;
    child->mmap_next = current_process->mmap_next;
    /* clear_child_tid is task-local state established by set_tid_address(2)
     * or CLONE_CHILD_CLEARTID; a plain fork does not inherit it. */
    child->clear_child_tid = 0;
    child->nice_value = current_process->nice_value;
    child->signal_stack_pointer = current_process->signal_stack_pointer;
    child->signal_stack_size = current_process->signal_stack_size;
    child->signal_stack_flags = current_process->signal_stack_flags;
    child->uid = current_process->uid;
    child->gid = current_process->gid;
    child->euid = current_process->euid;
    child->egid = current_process->egid;
    local_memcpy(child->username, current_process->username, sizeof(child->username));
    local_memcpy(child->home, current_process->home, sizeof(child->home));
    local_memcpy(child->command, current_process->command, sizeof(child->command));
    child->thermal_throttled =
        g_thermal_policy_active && !process_is_thermal_essential(child);

    // Child returns 0
    child->regs.rax = 0;
    child->status = PROCESS_STATUS_READY;

    // Parent returns child PID
    serial_print("AOS: sys_fork success, child PID=");
    // (Serial print PID logic omitted for brevity, just returning it)
    
    return child->pid;
}

int64_t sys_clone(struct syscall_regs* regs) {
    uint64_t flags = regs->rdi;
    process_t* parent = current_process;
    process_t* child = NULL;
    int child_idx = -1;

    if ((flags & LINUX_CLONE_VM) == 0) {
        serial_print("AOS: sys_clone using fork address space\n");
        return sys_fork(regs);
    }
    if (!parent || ((flags & LINUX_CLONE_THREAD) != 0 &&
                    (flags & LINUX_CLONE_VM) == 0)) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((flags & LINUX_CLONE_PARENT_SETTID) != 0 && regs->rdx == 0) {
        return -(int64_t)LINUX_EFAULT;
    }
    if ((flags & LINUX_CLONE_CHILD_SETTID) != 0 && regs->r10 == 0) {
        return -(int64_t)LINUX_EFAULT;
    }
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status == PROCESS_STATUS_DEAD) {
            child_idx = i;
            break;
        }
    }
    if (child_idx < 0) return -(int64_t)LINUX_EAGAIN;

    child = &process_list[child_idx];
    local_memset(child, 0, sizeof(*child));
    child->pid = next_pid++;
    child->parent_pid = (flags & LINUX_CLONE_THREAD)
                            ? parent->parent_pid
                            : parent->pid;
    child->session_id = parent->session_id;
    child->process_group_id = parent->process_group_id;
    child->thread_group_id = (flags & LINUX_CLONE_THREAD)
                                 ? parent->thread_group_id
                                 : child->pid;
    child->is_thread = (flags & LINUX_CLONE_THREAD) != 0;
    child->shares_address_space = 1;
    parent->shares_address_space = 1;
    child->p4_table = parent->p4_table;
    local_memcpy(&child->regs, regs, sizeof(child->regs));
    child->regs.rax = 0;
    if (regs->rsi != 0) child->regs.rsp = regs->rsi;
    if (child->regs.rsp != 0 &&
        !vmm_page_present(parent->p4_table, child->regs.rsp)) {
        serial_print("AOS: clone received unmapped child stack ");
        serial_print_hex64(child->regs.rsp);
        serial_print("\n");
    }
    local_memcpy(child->cwd, parent->cwd, sizeof(child->cwd));
    child->wait_target_pid = -1;
    child->wait_status_ptr = NULL;
    local_memcpy(child->fd_table, parent->fd_table, sizeof(child->fd_table));
    syscall_retain_fd_table_entries(child->fd_table, PROCESS_FD_MAX);
    child->fs_base = (flags & LINUX_CLONE_SETTLS) ? regs->r8 : parent->fs_base;
    child->brk_base = parent->brk_base;
    child->brk_current = parent->brk_current;
    child->brk_mapped_end = parent->brk_mapped_end;
    child->mmap_next = parent->mmap_next;
    child->clear_child_tid = (flags & LINUX_CLONE_CHILD_CLEARTID)
                                 ? regs->r10
                                 : 0;
    child->nice_value = parent->nice_value;
    if ((flags & LINUX_CLONE_VM) == 0 || (flags & LINUX_CLONE_VFORK) != 0) {
        child->signal_stack_pointer = parent->signal_stack_pointer;
        child->signal_stack_size = parent->signal_stack_size;
        child->signal_stack_flags = parent->signal_stack_flags;
    }
    child->uid = parent->uid;
    child->gid = parent->gid;
    child->euid = parent->euid;
    child->egid = parent->egid;
    local_memcpy(child->username, parent->username, sizeof(child->username));
    local_memcpy(child->home, parent->home, sizeof(child->home));
    local_memcpy(child->command, parent->command, sizeof(child->command));
    child->thermal_throttled =
        g_thermal_policy_active && !process_is_thermal_essential(child);

    if (flags & LINUX_CLONE_PARENT_SETTID) {
        *(uint32_t*)(uintptr_t)regs->rdx = child->pid;
    }
    if (flags & LINUX_CLONE_CHILD_SETTID) {
        *(uint32_t*)(uintptr_t)regs->r10 = child->pid;
    }
    child->status = PROCESS_STATUS_READY;
    serial_print("AOS: sys_clone shared address space\n");
    if (flags & LINUX_CLONE_VFORK) {
        child->vfork_parent_pid = parent->pid;
        parent->vfork_child_pid = child->pid;
        local_memcpy(&parent->regs, regs, sizeof(parent->regs));
        parent->regs.rax = child->pid;
        parent->has_interrupt_context = 0;
        parent->status = PROCESS_STATUS_WAITING;
        child->status = PROCESS_STATUS_RUNNING;
        current_process = child;
        switch_page_table(child->p4_table);
        process_load_fs_base(child->fs_base);
        switch_to_process(child);
    }
    return child->pid;
}

process_t* get_current_process(void) {
    return current_process;
}

const char* process_get_cwd(void) {
    if (!current_process) {
        return "";
    }
    return current_process->cwd;
}

const char* process_get_command(void) {
    return current_process ? current_process->command : "";
}

void process_set_cwd(const char* path) {
    size_t i = 0;

    if (!current_process) {
        return;
    }
    if (!path) {
        current_process->cwd[0] = '\0';
        return;
    }

    while (path[i] != '\0' && i + 1 < sizeof(current_process->cwd)) {
        current_process->cwd[i] = path[i];
        i++;
    }
    current_process->cwd[i] = '\0';
}

struct fd_entry* process_get_fd_table(void) {
    if (!current_process) {
        return NULL;
    }
    return current_process->fd_table;
}

uint32_t process_get_uid(void) {
    return current_process ? current_process->uid : 0;
}

uint32_t process_get_gid(void) {
    return current_process ? current_process->gid : 0;
}

uint32_t process_get_euid(void) {
    return current_process ? current_process->euid : 0;
}

uint32_t process_get_egid(void) {
    return current_process ? current_process->egid : 0;
}

const char* process_get_username(void) {
    if (!current_process || current_process->username[0] == '\0') {
        return "root";
    }
    return current_process->username;
}

const char* process_get_home(void) {
    if (!current_process || current_process->home[0] == '\0') {
        return "root";
    }
    return current_process->home;
}

int process_is_root(void) {
    return process_get_euid() == 0;
}

void process_update_thermal_policy(int emergency_active) {
    g_thermal_policy_active = emergency_active ? 1U : 0U;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* proc = &process_list[i];
        if (proc->status == PROCESS_STATUS_DEAD ||
            proc->status == PROCESS_STATUS_ZOMBIE) {
            proc->thermal_throttled = 0;
            proc->thermal_skip_phase = 0;
            continue;
        }
        proc->thermal_throttled =
            g_thermal_policy_active && !process_is_thermal_essential(proc);
        if (!proc->thermal_throttled) proc->thermal_skip_phase = 0;
    }
}

void process_thermal_gate_current(void) {
    uint32_t frequency;
    uint64_t wait_ticks;
    uint64_t start;

    if (!current_process || !current_process->thermal_throttled) return;
    frequency = timer_get_frequency();
    if (frequency == 0) frequency = 100;
    wait_ticks = frequency / 20U;
    if (wait_ticks == 0) wait_ticks = 1;

    current_process->thermal_delay_events++;
    start = timer_get_ticks();
    while (timer_get_ticks() - start < wait_ticks) {
        __asm__ volatile("sti; hlt; cli" ::: "memory");
    }
}

uint32_t process_thermal_throttled_count(void) {
    uint32_t count = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_list[i].status != PROCESS_STATUS_DEAD &&
            process_list[i].status != PROCESS_STATUS_ZOMBIE &&
            process_list[i].thermal_throttled) {
            count++;
        }
    }
    return count;
}

void process_become_root(void) {
    if (!current_process) {
        return;
    }
    current_process->euid = 0;
    current_process->egid = 0;
}

void process_set_identity(uint32_t uid, uint32_t gid,
                          const char* username, const char* home) {
    if (!current_process) return;
    current_process->uid = uid;
    current_process->gid = gid;
    current_process->euid = uid;
    current_process->egid = gid;
    local_strcpy_bounded(current_process->username,
                         sizeof(current_process->username), username);
    local_strcpy_bounded(current_process->home,
                         sizeof(current_process->home), home);
}
