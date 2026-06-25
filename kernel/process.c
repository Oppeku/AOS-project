#include <process.h>
#include <pmm.h>
#include <vmm.h>
#include <timer.h>
#include <xhci.h>
#include <stdint.h>
#include <stddef.h>

extern void serial_print(const char* s);
extern uint64_t p4_table[];
extern void syscall_retain_fd_table_entries(struct fd_entry* table, size_t count);
extern void syscall_init_process_fd_table(struct fd_entry* table, size_t count);
extern void process_load_fs_base(uint64_t fs_base);

process_t process_list[MAX_PROCESSES];
process_t* current_process = NULL;
static uint32_t next_pid = 1;

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

void schedule(struct syscall_regs* regs) {
    if (!current_process) return;

    // Save current state
    local_memcpy(&current_process->regs, regs, sizeof(struct syscall_regs));
    if (current_process->status == PROCESS_STATUS_RUNNING) {
        current_process->status = PROCESS_STATUS_READY;
    }

    // Pick next process
    int start_idx = 0;
    for(int i=0; i<MAX_PROCESSES; i++) {
        if(&process_list[i] == current_process) {
            start_idx = i + 1;
            break;
        }
    }

    process_t* next = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        int idx = (start_idx + i) % MAX_PROCESSES;
        if (process_list[idx].status == PROCESS_STATUS_READY) {
            next = &process_list[idx];
            break;
        }
    }

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

void timer_handler(struct syscall_regs* regs) {
    (void)regs;
    timer_tick();
    xhci_poll_keyboard();
    /*
     * The timer IRQ currently arrives through an interrupt gate, not the
     * SYSCALL entry path, so we do not have a syscall_regs frame here.
     * Treating the incoming register state as syscall_regs caused the kernel
     * to read from arbitrary user-space addresses and fault on timer ticks.
     *
     * Leave preemptive scheduling disabled until the timer stub saves an
     * interrupt-compatible frame and switch_to_process can resume from it.
     */
}

void init_process() {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_list[i].status = PROCESS_STATUS_DEAD;
    }

    // Create the first process (the one already running)
    process_list[0].pid = next_pid++;
    process_list[0].parent_pid = 0;
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
    local_strcpy_bounded(process_list[0].command, sizeof(process_list[0].command), "shell.elf");
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
    child->status = PROCESS_STATUS_READY;
    child->exit_status = 0;

    // 1. Copy Address Space
    child->p4_table = vmm_copy_p4(current_process->p4_table);
    if (!child->p4_table) return -1;

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
    child->clear_child_tid = current_process->clear_child_tid;
    child->uid = current_process->uid;
    child->gid = current_process->gid;
    child->euid = current_process->euid;
    child->egid = current_process->egid;
    local_memcpy(child->username, current_process->username, sizeof(child->username));
    local_memcpy(child->home, current_process->home, sizeof(child->home));
    local_memcpy(child->command, current_process->command, sizeof(child->command));

    // Child returns 0
    child->regs.rax = 0;

    // Parent returns child PID
    serial_print("AOS: sys_fork success, child PID=");
    // (Serial print PID logic omitted for brevity, just returning it)
    
    return child->pid;
}

int64_t sys_clone(struct syscall_regs* regs) {
    serial_print("AOS: sys_clone called (behaving like fork)\n");
    return sys_fork(regs);
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

void process_become_root(void) {
    if (!current_process) {
        return;
    }
    current_process->euid = 0;
    current_process->egid = 0;
}
