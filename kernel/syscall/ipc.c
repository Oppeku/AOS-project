#include "syscall_internal.h"

struct linux_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

int64_t sys_pipe(struct syscall_regs* regs) {
    int32_t* user_pipefd = (int32_t*)(uintptr_t)regs->rdi;
    int pipe_index = -1;
    int read_fd = -1;
    int write_fd = -1;
    struct fd_entry* table = current_fd_table();

    if (!user_pipefd || !table) return -(int64_t)LINUX_EFAULT;

    for (int i = 0; i < MAX_PIPE_OBJECTS; i++) {
        if (!g_pipe_objects[i].in_use) {
            pipe_index = i;
            break;
        }
    }
    if (pipe_index < 0) return -(int64_t)LINUX_EMFILE;

    read_fd = allocate_fd_slot(3);
    if (read_fd < 0) return -(int64_t)LINUX_EMFILE;
    table[read_fd].kind = FD_KIND_FREE;
    write_fd = allocate_fd_slot(read_fd + 1);
    if (write_fd < 0) return -(int64_t)LINUX_EMFILE;

    local_memset(&g_pipe_objects[pipe_index], 0, sizeof(g_pipe_objects[pipe_index]));
    g_pipe_objects[pipe_index].in_use = 1;
    g_pipe_objects[pipe_index].read_refs = 1;
    g_pipe_objects[pipe_index].write_refs = 1;

    table[read_fd].kind = FD_KIND_PIPE_READER;
    table[read_fd].handle_index = pipe_index;
    table[write_fd].kind = FD_KIND_PIPE_WRITER;
    table[write_fd].handle_index = pipe_index;

    user_pipefd[0] = read_fd;
    user_pipefd[1] = write_fd;
    return 0;
}

int64_t sys_poll(struct syscall_regs* regs) {
    struct linux_pollfd* fds = (struct linux_pollfd*)(uintptr_t)regs->rdi;
    uint64_t nfds = regs->rsi;
    int64_t ready = 0;

    (void)regs->rdx;

    if (!fds && nfds > 0) return -(int64_t)LINUX_EFAULT;
    if (nfds > 1024) return -(int64_t)LINUX_EINVAL;

    for (uint64_t i = 0; i < nfds; i++) {
        struct linux_pollfd* pfd = &fds[i];
        struct fd_entry* entry = NULL;
        int16_t revents = 0;

        if (pfd->fd < 0) {
            pfd->revents = 0;
            continue;
        }

        entry = get_fd_entry((uint64_t)pfd->fd);
        if (!entry) {
            pfd->revents = LINUX_POLLNVAL;
            ready++;
            continue;
        }

        if (entry->kind == FD_KIND_STDIN || entry->kind == FD_KIND_TTY) {
            if (pfd->events & LINUX_POLLIN) revents |= LINUX_POLLIN;
            if (pfd->events & LINUX_POLLOUT) revents |= LINUX_POLLOUT;
        } else if (entry->kind == FD_KIND_STDOUT || entry->kind == FD_KIND_STDERR || entry->kind == FD_KIND_VNODE || entry->kind == FD_KIND_NULL) {
            if (pfd->events & LINUX_POLLOUT) revents |= LINUX_POLLOUT;
            if (pfd->events & LINUX_POLLIN) revents |= LINUX_POLLIN;
        } else if (entry->kind == FD_KIND_PIPE_READER) {
            struct pipe_object* pipe = get_pipe_object_by_index(entry->handle_index);
            if (!pipe) {
                revents |= LINUX_POLLERR;
            } else {
                if ((pfd->events & LINUX_POLLIN) && pipe->size > 0) revents |= LINUX_POLLIN;
                if (pipe->write_refs == 0) revents |= LINUX_POLLHUP;
            }
        } else if (entry->kind == FD_KIND_PIPE_WRITER) {
            struct pipe_object* pipe = get_pipe_object_by_index(entry->handle_index);
            if (!pipe || pipe->read_refs == 0) {
                revents |= LINUX_POLLERR;
            } else if ((pfd->events & LINUX_POLLOUT) && pipe->size < sizeof(pipe->buffer)) {
                revents |= LINUX_POLLOUT;
            }
        } else if (entry->kind == FD_KIND_SOCKET) {
            struct socket_object* sock = get_socket_by_index(entry->handle_index);
            if (!sock) {
                revents |= LINUX_POLLERR;
            } else {
                if (pfd->events & LINUX_POLLOUT) revents |= LINUX_POLLOUT;
                if ((pfd->events & LINUX_POLLIN) && sock->rx_off < sock->rx_len) {
                    revents |= LINUX_POLLIN;
                }
                if (sock->state == SOCKET_STATE_CLOSED) revents |= LINUX_POLLHUP;
            }
        }

        pfd->revents = revents;
        if (revents) ready++;
    }

    return ready;
}
