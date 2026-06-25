#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <syscall.h>

#define MAX_PROCESSES 64
#define PROCESS_CWD_MAX 256
#define PROCESS_FD_MAX 32
#define PROCESS_USERNAME_MAX 32
#define PROCESS_COMMAND_MAX 64

struct fd_entry {
    uint8_t kind;
    uint8_t reserved[3];
    int32_t handle_index;
};

typedef enum {
    PROCESS_STATUS_DEAD = 0,
    PROCESS_STATUS_READY,
    PROCESS_STATUS_RUNNING,
    PROCESS_STATUS_WAITING,
    PROCESS_STATUS_ZOMBIE,
} process_status_t;

typedef struct {
    uint32_t pid;
    uint32_t parent_pid;
    process_status_t status;
    int32_t exit_status;
    uint64_t* p4_table;
    struct syscall_regs regs;
    uint64_t kernel_stack;
    char cwd[PROCESS_CWD_MAX];
    int64_t wait_target_pid;
    int32_t* wait_status_ptr;
    struct fd_entry fd_table[PROCESS_FD_MAX];
    uint64_t fs_base;
    uint64_t brk_base;
    uint64_t brk_current;
    uint64_t brk_mapped_end;
    uint64_t mmap_next;
    uint64_t clear_child_tid;
    uint32_t uid;
    uint32_t gid;
    uint32_t euid;
    uint32_t egid;
    char username[PROCESS_USERNAME_MAX];
    char home[PROCESS_CWD_MAX];
    char command[PROCESS_COMMAND_MAX];
} process_t;

extern process_t process_list[MAX_PROCESSES];
extern process_t* current_process;

void init_process();
void schedule(struct syscall_regs* regs);
void switch_to_process(process_t* proc);
int64_t sys_fork(struct syscall_regs* regs);
int64_t sys_clone(struct syscall_regs* regs);
process_t* get_current_process(void);
const char* process_get_cwd(void);
void process_set_cwd(const char* path);
struct fd_entry* process_get_fd_table(void);
uint32_t process_get_uid(void);
uint32_t process_get_gid(void);
uint32_t process_get_euid(void);
uint32_t process_get_egid(void);
const char* process_get_username(void);
const char* process_get_home(void);
int process_is_root(void);
void process_become_root(void);

#endif
