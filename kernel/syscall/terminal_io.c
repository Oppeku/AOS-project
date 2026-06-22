#include "syscall_internal.h"

struct linux_termios {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t c_line;
    uint8_t c_cc[19];
};

struct linux_winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

struct linux_iovec {
    uint64_t iov_base;
    uint64_t iov_len;
};

int64_t sys_write(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    struct fd_entry* entry = get_fd_entry(fd);
    const char* buf = (const char*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;

    if (!entry) return -(int64_t)LINUX_EBADF;
    if (!buf) return -(int64_t)LINUX_EFAULT;
    if (entry->kind == FD_KIND_PIPE_WRITER) {
        struct pipe_object* pipe = get_pipe_for_fd(fd, FD_KIND_PIPE_WRITER);
        uint64_t bytes_written = 0;
        if (!pipe) return -(int64_t)LINUX_EBADF;
        while (bytes_written < len && pipe->size < sizeof(pipe->buffer)) {
            pipe->buffer[pipe->write_pos] = (uint8_t)buf[bytes_written++];
            pipe->write_pos = (pipe->write_pos + 1) % sizeof(pipe->buffer);
            pipe->size++;
        }
        return (int64_t)bytes_written;
    }
    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        uint64_t bytes_written = 0;
        uint32_t new_size = 0;

        if (!file) return -(int64_t)LINUX_EBADF;
        if (file->node.type != VFS_NODE_TYPE_REGULAR) return -(int64_t)LINUX_EISDIR;
        if (file->node.backend != VFS_BACKEND_AOSFS && file->node.backend != VFS_BACKEND_TMPFS && file->node.backend != VFS_BACKEND_FAT32 && file->node.backend != VFS_BACKEND_EXT4) return -(int64_t)LINUX_EACCES;
        if (!user_can_mutate_path(file->node.path)) return -(int64_t)LINUX_EACCES;
        if (vfs_write_node(&file->node, file->offset, (const uint8_t*)buf, len, &bytes_written, &new_size) != 0) {
            return -(int64_t)LINUX_EIO;
        }
        file->offset += bytes_written;
        file->node.size = new_size;
        return (int64_t)bytes_written;
    }
    if (entry->kind == FD_KIND_NULL) {
        return (int64_t)len;
    }
    if (entry->kind == FD_KIND_SOCKET) {
        return socket_send_data(get_socket_by_index(entry->handle_index),
                                (const uint8_t*)buf,
                                len);
    }
    if (entry->kind != FD_KIND_STDOUT && entry->kind != FD_KIND_STDERR && entry->kind != FD_KIND_TTY) {
        return -(int64_t)LINUX_EBADF;
    }

    return (int64_t)tty_write(buf, len);
}

static int is_tty_fd_kind(uint8_t kind) {
    return kind == FD_KIND_STDIN || kind == FD_KIND_STDOUT || kind == FD_KIND_STDERR || kind == FD_KIND_TTY;
}

static void fill_default_termios(struct linux_termios* term) {
    local_memset(term, 0, sizeof(*term));
    term->c_iflag = LINUX_TERMIOS_ICRNL;
    term->c_oflag = LINUX_TERMIOS_OPOST;
    term->c_cflag = LINUX_TERMIOS_CREAD | LINUX_TERMIOS_CS8;
    term->c_lflag = LINUX_TERMIOS_ISIG | LINUX_TERMIOS_ICANON | LINUX_TERMIOS_ECHO |
        LINUX_TERMIOS_ECHOE | LINUX_TERMIOS_ECHOK | LINUX_TERMIOS_IEXTEN;
    term->c_cc[0] = 3;     /* VINTR: Ctrl-C */
    term->c_cc[1] = 28;    /* VQUIT: Ctrl-\ */
    term->c_cc[2] = 127;   /* VERASE */
    term->c_cc[3] = 21;    /* VKILL: Ctrl-U */
    term->c_cc[4] = 4;     /* VEOF: Ctrl-D */
    term->c_cc[5] = 0;     /* VTIME */
    term->c_cc[6] = 1;     /* VMIN */
    term->c_cc[8] = 17;    /* VSTART: Ctrl-Q */
    term->c_cc[9] = 19;    /* VSTOP: Ctrl-S */
    term->c_cc[10] = 26;   /* VSUSP: Ctrl-Z */
}

int64_t sys_ioctl(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    uint64_t request = regs->rsi;
    void* arg = (void*)(uintptr_t)regs->rdx;
    struct fd_entry* entry = get_fd_entry(fd);

    if (!entry) return -(int64_t)LINUX_EBADF;

    switch (request) {
        case LINUX_TCGETS:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            fill_default_termios((struct linux_termios*)arg);
            return 0;
        case LINUX_TCSETS:
        case LINUX_TCSETSW:
        case LINUX_TCSETSF:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            return 0;
        case LINUX_TIOCGWINSZ:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            tty_get_winsize(&((struct linux_winsize*)arg)->ws_row,
                            &((struct linux_winsize*)arg)->ws_col);
            ((struct linux_winsize*)arg)->ws_xpixel = 0;
            ((struct linux_winsize*)arg)->ws_ypixel = 0;
            return 0;
        case LINUX_TIOCSWINSZ:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            return 0;
        case LINUX_FIONREAD:
            if (!arg) return -(int64_t)LINUX_EFAULT;
            *(int*)(uintptr_t)arg = (int)tty_pending();
            return 0;
        case LINUX_TIOCGPGRP:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            *(int*)(uintptr_t)arg = 1;
            return 0;
        case LINUX_TIOCSPGRP:
            if (!is_tty_fd_kind(entry->kind)) return -(int64_t)LINUX_ENOTTY;
            if (!arg) return -(int64_t)LINUX_EFAULT;
            return 0;
        default:
            return -(int64_t)LINUX_ENOTTY;
    }
}

int64_t sys_writev(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    const struct linux_iovec* iov = (const struct linux_iovec*)(uintptr_t)regs->rsi;
    uint64_t iovcnt = regs->rdx;
    int64_t total = 0;

    if (!iov && iovcnt != 0) return -(int64_t)LINUX_EFAULT;
    if (iovcnt > 64) return -(int64_t)LINUX_EINVAL;

    for (uint64_t i = 0; i < iovcnt; i++) {
        struct syscall_regs write_regs = *regs;
        if (!iov[i].iov_base && iov[i].iov_len != 0) return -(int64_t)LINUX_EFAULT;
        write_regs.rdi = fd;
        write_regs.rsi = iov[i].iov_base;
        write_regs.rdx = iov[i].iov_len;
        int64_t rc = sys_write(&write_regs);
        if (rc < 0) {
            return total > 0 ? total : rc;
        }
        total += rc;
        if ((uint64_t)rc != iov[i].iov_len) {
            break;
        }
    }

    return total;
}

int64_t sys_readv(struct syscall_regs* regs);

extern void keyboard_handler_main();
