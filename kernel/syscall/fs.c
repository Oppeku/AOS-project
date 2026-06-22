#include "syscall_internal.h"

struct linux_iovec {
    uint64_t iov_base;
    uint64_t iov_len;
};

struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t __unused[3];
};

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

static void fill_linux_stat(struct linux_stat* st, uint64_t inode_seed, uint32_t size, uint32_t mode) {
    local_memset(st, 0, sizeof(*st));
    st->st_dev = 1;
    st->st_ino = inode_seed ? inode_seed : 1;
    st->st_nlink = 1;
    st->st_mode = mode;
    st->st_rdev = 0;
    st->st_size = (int64_t)size;
    st->st_blksize = 512;
    st->st_blocks = (int64_t)((size + 511U) / 512U);
}

int64_t sys_open(struct syscall_regs* regs) {
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    return open_path_with_flags(resolved_path, (uint64_t)regs->rsi);
}

int64_t sys_access(struct syscall_regs* regs) {
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (regs->rsi & ~(LINUX_R_OK | LINUX_W_OK | LINUX_X_OK)) {
        return -(int64_t)LINUX_EINVAL;
    }
    {
        struct vfs_node node;
        if (vfs_lookup(resolved_path, &node) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if ((regs->rsi & LINUX_W_OK) && node.backend != VFS_BACKEND_AOSFS && node.backend != VFS_BACKEND_TMPFS && node.backend != VFS_BACKEND_FAT32 && node.backend != VFS_BACKEND_EXT4) {
            return -(int64_t)LINUX_EACCES;
        }
    }
    return 0;
}

int64_t sys_openat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    return open_path_with_flags(resolved_path, regs->rdx);
}

int64_t sys_mkdirat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node existing;
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    (void)regs->rdx;
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (vfs_lookup(resolved_path, &existing) == 0) {
        return -(int64_t)LINUX_EEXIST;
    }
    if (!user_can_mutate_path(resolved_path)) {
        return -(int64_t)LINUX_EACCES;
    }
    if (aosfs_mkdir_path(resolved_path) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

int64_t sys_mkdir(struct syscall_regs* regs) {
    struct syscall_regs mkdirat_regs = *regs;
    mkdirat_regs.rdi = (uint64_t)LINUX_AT_FDCWD;
    mkdirat_regs.rsi = regs->rdi;
    mkdirat_regs.rdx = regs->rsi;
    return sys_mkdirat(&mkdirat_regs);
}

int64_t sys_unlinkat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc;

    if (regs->rdx != 0 && regs->rdx != LINUX_AT_REMOVEDIR) {
        return -(int64_t)LINUX_EINVAL;
    }

    rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (!user_can_mutate_path(resolved_path)) {
        return -(int64_t)LINUX_EACCES;
    }
    if (regs->rdx == LINUX_AT_REMOVEDIR) {
        if (vfs_rmdir_path(resolved_path) != 0) {
            return -(int64_t)LINUX_EACCES;
        }
        return 0;
    }
    if (vfs_unlink_path(resolved_path) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

int64_t sys_faccessat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (regs->rdx & ~(LINUX_R_OK | LINUX_W_OK | LINUX_X_OK)) {
        return -(int64_t)LINUX_EINVAL;
    }
    {
        struct vfs_node node;
        if (vfs_lookup(resolved_path, &node) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if ((regs->rdx & LINUX_W_OK) && node.backend != VFS_BACKEND_AOSFS && node.backend != VFS_BACKEND_TMPFS && node.backend != VFS_BACKEND_FAT32 && node.backend != VFS_BACKEND_EXT4) {
            return -(int64_t)LINUX_EACCES;
        }
    }
    return 0;
}

int64_t sys_read(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    struct fd_entry* entry = get_fd_entry(fd);
    char* buf = (char*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;

    if (!buf) return -(int64_t)LINUX_EFAULT;
    if (!entry) return -(int64_t)LINUX_EBADF;

    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        if (!file) return -(int64_t)LINUX_EBADF;
        if (file->node.type != VFS_NODE_TYPE_REGULAR) return -(int64_t)LINUX_EISDIR;

        if (file->offset >= file->node.size) {
            return 0;
        }

        uint64_t available = (uint64_t)file->node.size - file->offset;
        if (len > available) {
            len = available;
        }

        if (vfs_read_node(&file->node, file->offset, (uint8_t*)buf, len) != 0) {
            return -(int64_t)LINUX_EIO;
        }
        file->offset += len;
        return (int64_t)len;
    }
    if (entry->kind == FD_KIND_PIPE_READER) {
        struct pipe_object* pipe = get_pipe_for_fd(fd, FD_KIND_PIPE_READER);
        uint64_t bytes_read = 0;
        if (!pipe) return -(int64_t)LINUX_EBADF;
        while (bytes_read < len && pipe->size > 0) {
            buf[bytes_read++] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % sizeof(pipe->buffer);
            pipe->size--;
        }
        return (int64_t)bytes_read;
    }
    if (entry->kind == FD_KIND_NULL) return 0;
    if (entry->kind == FD_KIND_SOCKET) {
        return socket_recv_data(get_socket_by_index(entry->handle_index),
                                (uint8_t*)buf,
                                len);
    }
    if (entry->kind != FD_KIND_STDIN && entry->kind != FD_KIND_TTY) return -(int64_t)LINUX_EBADF;
    if (len == 0) return 0;

    return (int64_t)tty_read(buf, len);
}

int64_t sys_readv(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    const struct linux_iovec* iov = (const struct linux_iovec*)(uintptr_t)regs->rsi;
    uint64_t iovcnt = regs->rdx;
    int64_t total = 0;

    if (!iov && iovcnt != 0) return -(int64_t)LINUX_EFAULT;
    if (iovcnt > 64) return -(int64_t)LINUX_EINVAL;

    for (uint64_t i = 0; i < iovcnt; i++) {
        struct syscall_regs read_regs = *regs;
        if (!iov[i].iov_base && iov[i].iov_len != 0) return -(int64_t)LINUX_EFAULT;
        read_regs.rdi = fd;
        read_regs.rsi = iov[i].iov_base;
        read_regs.rdx = iov[i].iov_len;
        int64_t rc = sys_read(&read_regs);
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

int64_t sys_lseek(struct syscall_regs* regs) {
    struct file_handle* file = get_vnode_handle(regs->rdi);
    int64_t offset = (int64_t)regs->rsi;
    uint64_t whence = regs->rdx;
    int64_t base = 0;

    if (!file) return -(int64_t)LINUX_EBADF;
    if (file->node.type != VFS_NODE_TYPE_REGULAR) return -(int64_t)LINUX_EINVAL;

    switch (whence) {
        case SEEK_SET:
            base = 0;
            break;
        case SEEK_CUR:
            base = (int64_t)file->offset;
            break;
        case SEEK_END:
            base = (int64_t)file->node.size;
            break;
        default:
            return -(int64_t)LINUX_EINVAL;
    }

    int64_t new_offset = base + offset;
    if (new_offset < 0) {
        return -(int64_t)LINUX_EINVAL;
    }

    file->offset = (uint64_t)new_offset;
    return new_offset;
}

int64_t sys_newfstatat(struct syscall_regs* regs);

int64_t sys_fstat(struct syscall_regs* regs) {
    struct fd_entry* entry = get_fd_entry(regs->rdi);
    struct linux_stat* st = (struct linux_stat*)(uintptr_t)regs->rsi;

    if (!entry) return -(int64_t)LINUX_EBADF;
    if (!st) return -(int64_t)LINUX_EFAULT;

    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        if (!file) return -(int64_t)LINUX_EBADF;
        if (file->node.type == VFS_NODE_TYPE_DIRECTORY) {
            uint64_t inode_seed = file->node.inode;
            fill_linux_stat(st, inode_seed, 0, LINUX_S_IFDIR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        } else {
            fill_linux_stat(st, file->node.inode, file->node.size, LINUX_S_IFREG | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        }
        return 0;
    }
    if (entry->kind == FD_KIND_PIPE_READER || entry->kind == FD_KIND_PIPE_WRITER) {
        fill_linux_stat(st, regs->rdi + 1, 0, LINUX_S_IFIFO | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        return 0;
    }

    fill_linux_stat(st, regs->rdi + 1, 0, LINUX_S_IFCHR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
    return 0;
}

int64_t sys_stat(struct syscall_regs* regs) {
    struct syscall_regs statat_regs = *regs;
    statat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    statat_regs.rsi = regs->rdi;
    statat_regs.rdx = regs->rsi;
    statat_regs.r10 = 0;
    return sys_newfstatat(&statat_regs);
}

int64_t sys_newfstatat(struct syscall_regs* regs) {
    int64_t dirfd = (int64_t)regs->rdi;
    const char* path = (const char*)(uintptr_t)regs->rsi;
    struct linux_stat* st = (struct linux_stat*)(uintptr_t)regs->rdx;
    uint64_t flags = regs->r10;
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = 0;

    if (!st) return -(int64_t)LINUX_EFAULT;

    if (path && path[0] == '\0' && (flags & LINUX_AT_EMPTY_PATH)) {
        struct fd_entry* entry = get_fd_entry((uint64_t)dirfd);
        if (!entry) return -(int64_t)LINUX_EBADF;
        if (entry->kind == FD_KIND_VNODE) {
            struct file_handle* file = get_file_handle_by_index(entry->handle_index);
            if (!file) return -(int64_t)LINUX_EBADF;
            if (file->node.type == VFS_NODE_TYPE_DIRECTORY) {
                uint64_t inode_seed = file->node.inode;
                fill_linux_stat(st, inode_seed, 0, LINUX_S_IFDIR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
            } else {
                fill_linux_stat(st, file->node.inode, file->node.size, LINUX_S_IFREG | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
            }
        } else {
            uint32_t mode = (entry->kind == FD_KIND_PIPE_READER || entry->kind == FD_KIND_PIPE_WRITER)
                ? (LINUX_S_IFIFO | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH)
                : (LINUX_S_IFCHR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
            fill_linux_stat(st, (uint64_t)dirfd + 1, 0, mode);
        }
        return 0;
    }

    if (!path) return -(int64_t)LINUX_EFAULT;

    rc = resolve_path_from_dirfd(dirfd, path, resolved_path, sizeof(resolved_path));
    if (rc < 0) {
        return rc;
    }

    {
        struct vfs_node node;
        if (vfs_lookup(resolved_path, &node) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        if (node.type == VFS_NODE_TYPE_DIRECTORY) {
            fill_linux_stat(st, node.inode, 0, LINUX_S_IFDIR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        } else if (node.type == VFS_NODE_TYPE_CHAR_DEVICE) {
            fill_linux_stat(st, node.inode, 0, LINUX_S_IFCHR | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        } else {
            fill_linux_stat(st, node.inode, node.size, LINUX_S_IFREG | LINUX_S_IRUSR | LINUX_S_IWUSR | LINUX_S_IRGRP | LINUX_S_IROTH);
        }
    }

    return 0;
}

int64_t sys_getdents64(struct syscall_regs* regs) {
    struct file_handle* file = get_vnode_handle(regs->rdi);
    uint8_t* buf = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t len = regs->rdx;
    uint64_t bytes_written = 0;

    if (!file) return -(int64_t)LINUX_EBADF;
    if (!buf) return -(int64_t)LINUX_EFAULT;
    if (file->node.type != VFS_NODE_TYPE_DIRECTORY) return -(int64_t)LINUX_ENOTDIR;

    while (1) {
        char name[64];
        uint32_t entry_size = 0;
        uint8_t d_type = LINUX_DTYPE_REG;
        uint64_t logical_index = file->offset;

        if (vfs_dirent_at(&file->node, logical_index, name, sizeof(name), &entry_size, &d_type) != 0) {
            break;
        }

        uint64_t name_len = 0;
        while (name[name_len] != '\0') {
            name_len++;
        }

        uint16_t reclen = (uint16_t)((offsetof(struct linux_dirent64, d_name) + name_len + 2 + 7) & ~7ULL);
        if (bytes_written + reclen > len) {
            break;
        }

        struct linux_dirent64* ent = (struct linux_dirent64*)(buf + bytes_written);
        ent->d_ino = logical_index + 1;
        ent->d_off = (int64_t)(logical_index + 1);
        ent->d_reclen = reclen;
        ent->d_type = d_type;

        for (uint64_t i = 0; i < name_len; i++) {
            ent->d_name[i] = name[i];
        }
        ent->d_name[name_len] = '\0';

        uint64_t pad_start = offsetof(struct linux_dirent64, d_name) + name_len + 1;
        while (pad_start < reclen) {
            ((uint8_t*)ent)[pad_start++] = 0;
        }

        bytes_written += reclen;
        file->offset = logical_index + 1;
    }

    return (int64_t)bytes_written;
}

int64_t sys_close(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    if (!get_fd_entry(fd)) return -(int64_t)LINUX_EBADF;
    close_fd_internal(fd);
    return 0;
}
