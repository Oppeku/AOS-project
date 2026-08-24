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

struct linux_statfs {
    int64_t f_type;
    int64_t f_bsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    int32_t f_fsid[2];
    int64_t f_namelen;
    int64_t f_frsize;
    int64_t f_flags;
    int64_t f_spare[4];
};

_Static_assert(sizeof(struct linux_statfs) == 120,
               "Linux x86_64 statfs ABI size changed");

static int64_t linux_fs_magic(uint8_t backend) {
    switch (backend) {
        case VFS_BACKEND_FAT32:
            return 0x4D44;
        case VFS_BACKEND_TMPFS:
            return 0x01021994;
        case VFS_BACKEND_EXT4:
            return 0xEF53;
        case VFS_BACKEND_AOSFS:
            return 0x414F5346;
        case VFS_BACKEND_PROCFS:
            return 0x9FA0;
        case VFS_BACKEND_INITRD:
            return 0x858458F6;
        default:
            return 0x62656572;
    }
}

static void fill_linux_statfs(struct linux_statfs* st, uint8_t backend) {
    const uint64_t block_size = 4096;
    const uint64_t default_blocks = 262144;

    local_memset(st, 0, sizeof(*st));
    st->f_type = linux_fs_magic(backend);
    st->f_bsize = (int64_t)block_size;
    st->f_blocks = default_blocks;
    st->f_bfree = default_blocks / 2;
    st->f_bavail = st->f_bfree;
    st->f_files = 65536;
    st->f_ffree = 32768;
    st->f_fsid[0] = (int32_t)backend;
    st->f_namelen = 255;
    st->f_frsize = (int64_t)block_size;
    if (backend == VFS_BACKEND_INITRD ||
        backend == VFS_BACKEND_SYNTHETIC ||
        backend == VFS_BACKEND_PROCFS) {
        st->f_bfree = 0;
        st->f_bavail = 0;
        st->f_ffree = 0;
        st->f_flags = 1;
    }
}

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

static void fill_linux_node_stat(struct linux_stat* st,
                                 const struct vfs_node* node) {
    uint32_t type_mode = LINUX_S_IFREG;

    if (node->type == VFS_NODE_TYPE_DIRECTORY) type_mode = LINUX_S_IFDIR;
    else if (node->type == VFS_NODE_TYPE_CHAR_DEVICE) type_mode = LINUX_S_IFCHR;
    else if (node->type == VFS_NODE_TYPE_SYMLINK) type_mode = LINUX_S_IFLNK;
    fill_linux_stat(st, node->inode,
                    node->type == VFS_NODE_TYPE_DIRECTORY ? 0 : node->size,
                    type_mode | (node->mode & 07777U));
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    if (node->type == VFS_NODE_TYPE_DIRECTORY) st->st_nlink = 2;
}

int64_t sys_open(struct syscall_regs* regs) {
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    return open_path_with_flags(resolved_path, (uint64_t)regs->rsi,
                                (uint16_t)regs->rdx);
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
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    rc = open_path_with_flags(resolved_path, regs->rdx,
                              (uint16_t)regs->r10);
    syscall_linux_trace_path("openat", resolved_path, rc);
    return rc;
}

int64_t sys_mkdirat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
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
    if (vfs_mkdir_path(resolved_path, (uint16_t)regs->rdx,
                       process_get_euid(), process_get_egid()) != 0) {
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
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
    char path_buf[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node node;
    int64_t rc;

    if (regs->rdx != 0 && regs->rdx != LINUX_AT_REMOVEDIR) {
        return -(int64_t)LINUX_EINVAL;
    }

    rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi, path_buf, sizeof(path_buf));
    if (rc < 0) return rc;

    rc = resolve_path_from_dirfd(dirfd, path_buf, resolved_path, sizeof(resolved_path));
    if (rc < 0) return rc;

    if (vfs_lstat(resolved_path, &node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (!user_can_mutate_path(resolved_path)) {
        return -(int64_t)LINUX_EACCES;
    }
    if (regs->rdx == LINUX_AT_REMOVEDIR) {
        if (node.type != VFS_NODE_TYPE_DIRECTORY) {
            return -(int64_t)LINUX_ENOTDIR;
        }
        if (vfs_rmdir_path(resolved_path) != 0) {
            return -(int64_t)LINUX_EACCES;
        }
        return 0;
    }
    if (node.type == VFS_NODE_TYPE_DIRECTORY) {
        return -(int64_t)LINUX_EISDIR;
    }
    if (vfs_unlink_path(resolved_path) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

int64_t sys_unlink(struct syscall_regs* regs) {
    struct syscall_regs unlinkat_regs = *regs;
    unlinkat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    unlinkat_regs.rsi = regs->rdi;
    unlinkat_regs.rdx = 0;
    return sys_unlinkat(&unlinkat_regs);
}

int64_t sys_rmdir(struct syscall_regs* regs) {
    struct syscall_regs unlinkat_regs = *regs;
    unlinkat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    unlinkat_regs.rsi = regs->rdi;
    unlinkat_regs.rdx = LINUX_AT_REMOVEDIR;
    return sys_unlinkat(&unlinkat_regs);
}

int64_t sys_symlinkat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rsi);
    char target[MAX_EXEC_STRING];
    char path[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    int64_t rc;

    rc = copy_user_cstr((const char*)(uintptr_t)regs->rdi,
                        target, sizeof(target));
    if (rc < 0) return rc;
    rc = copy_user_cstr((const char*)(uintptr_t)regs->rdx,
                        path, sizeof(path));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(dirfd, path, resolved_path,
                                 sizeof(resolved_path));
    if (rc < 0) return rc;
    if (!user_can_mutate_path(resolved_path)) {
        return -(int64_t)LINUX_EACCES;
    }
    {
        struct vfs_node existing;
        if (vfs_lstat(resolved_path, &existing) == 0) {
            return -(int64_t)LINUX_EEXIST;
        }
    }
    if (vfs_symlink_path(target, resolved_path,
                         process_get_euid(), process_get_egid()) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

int64_t sys_symlink(struct syscall_regs* regs) {
    struct syscall_regs linkat_regs = *regs;
    linkat_regs.rdi = regs->rdi;
    linkat_regs.rsi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    linkat_regs.rdx = regs->rsi;
    return sys_symlinkat(&linkat_regs);
}

int64_t sys_readlinkat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
    char path[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    char* buffer = (char*)(uintptr_t)regs->rdx;
    struct vfs_node node;
    size_t size = 0;
    int64_t rc;

    if (regs->r10 == 0) return -(int64_t)LINUX_EINVAL;
    if (!buffer) return -(int64_t)LINUX_EFAULT;
    rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi,
                        path, sizeof(path));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(dirfd, path, resolved_path,
                                 sizeof(resolved_path));
    if (rc < 0) return rc;
    if (vfs_lstat(resolved_path, &node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (node.type != VFS_NODE_TYPE_SYMLINK) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (vfs_readlink_path(resolved_path, buffer, (size_t)regs->r10,
                          &size) != 0) {
        return -(int64_t)LINUX_EIO;
    }
    return (int64_t)size;
}

int64_t sys_readlink(struct syscall_regs* regs) {
    struct syscall_regs linkat_regs = *regs;
    linkat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    linkat_regs.rsi = regs->rdi;
    linkat_regs.rdx = regs->rsi;
    linkat_regs.r10 = regs->rdx;
    return sys_readlinkat(&linkat_regs);
}

int64_t sys_fchmodat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
    char path[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node node;
    int64_t rc;

    rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi,
                        path, sizeof(path));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(dirfd, path, resolved_path,
                                 sizeof(resolved_path));
    if (rc < 0) return rc;
    if (vfs_lookup(resolved_path, &node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (process_get_euid() != 0 && process_get_euid() != node.uid) {
        return -(int64_t)LINUX_EPERM;
    }
    if (vfs_chmod_path(resolved_path, (uint16_t)regs->rdx, 0) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

int64_t sys_chmod(struct syscall_regs* regs) {
    struct syscall_regs chmodat_regs = *regs;
    chmodat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    chmodat_regs.rsi = regs->rdi;
    chmodat_regs.rdx = regs->rsi;
    return sys_fchmodat(&chmodat_regs);
}

int64_t sys_fchownat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
    char path[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node node;
    uint32_t uid = (uint32_t)regs->rdx;
    uint32_t gid = (uint32_t)regs->r10;
    uint64_t flags = regs->r8;
    int64_t rc;

    if (flags & ~((uint64_t)LINUX_AT_SYMLINK_NOFOLLOW)) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (process_get_euid() != 0) return -(int64_t)LINUX_EPERM;
    rc = copy_user_cstr((const char*)(uintptr_t)regs->rsi,
                        path, sizeof(path));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(dirfd, path, resolved_path,
                                 sizeof(resolved_path));
    if (rc < 0) return rc;
    if (((flags & LINUX_AT_SYMLINK_NOFOLLOW)
             ? vfs_lstat(resolved_path, &node)
             : vfs_lookup(resolved_path, &node)) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    if (uid == UINT32_MAX) uid = node.uid;
    if (gid == UINT32_MAX) gid = node.gid;
    if (vfs_chown_path(resolved_path, uid, gid,
                       (flags & LINUX_AT_SYMLINK_NOFOLLOW) != 0) != 0) {
        return -(int64_t)LINUX_EACCES;
    }
    return 0;
}

int64_t sys_chown(struct syscall_regs* regs) {
    struct syscall_regs chownat_regs = *regs;
    chownat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    chownat_regs.rsi = regs->rdi;
    chownat_regs.rdx = regs->rsi;
    chownat_regs.r10 = regs->rdx;
    chownat_regs.r8 = 0;
    return sys_fchownat(&chownat_regs);
}

int64_t sys_faccessat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
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
    if (!entry) {
        syscall_linux_trace("read bad fd", (int64_t)fd);
        return -(int64_t)LINUX_EBADF;
    }

    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        if (!file) {
            syscall_linux_trace("read bad handle", entry->handle_index);
            return -(int64_t)LINUX_EBADF;
        }
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
        if (pipe->size == 0 && pipe->write_refs > 0) {
            if (regs->rcx >= 2) regs->rcx -= 2;
            schedule(regs);
        }
        while (bytes_read < len && pipe->size > 0) {
            buf[bytes_read++] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % sizeof(pipe->buffer);
            pipe->size--;
        }
        return (int64_t)bytes_read;
    }
    if (entry->kind == FD_KIND_NULL) return 0;
    if (entry->kind == FD_KIND_RANDOM) {
        linux_random_fill((uint8_t*)buf, len);
        return (int64_t)len;
    }
    if (entry->kind == FD_KIND_SOCKET) {
        return socket_recv_data(get_socket_by_index(entry->handle_index),
                                (uint8_t*)buf,
                                len);
    }
    if (entry->kind == FD_KIND_EVENTFD) {
        struct eventfd_object* eventfd =
            get_eventfd_object_by_index(entry->handle_index);
        uint64_t value;

        if (!eventfd) return -(int64_t)LINUX_EBADF;
        if (len < sizeof(value)) return -(int64_t)LINUX_EINVAL;
        if (eventfd->counter == 0) {
            if (eventfd->nonblocking) return -(int64_t)LINUX_EAGAIN;
            if (regs->rcx >= 2) regs->rcx -= 2;
            schedule(regs);
            return -(int64_t)LINUX_EAGAIN;
        }
        value = eventfd->semaphore ? 1 : eventfd->counter;
        eventfd->counter -= value;
        local_memcpy(buf, &value, sizeof(value));
        return (int64_t)sizeof(value);
    }
    if (entry->kind != FD_KIND_STDIN && entry->kind != FD_KIND_TTY) return -(int64_t)LINUX_EBADF;
    if (len == 0) return 0;

    return (int64_t)tty_read(buf, len);
}

int64_t sys_pread64(struct syscall_regs* regs) {
    uint64_t fd = regs->rdi;
    struct fd_entry* entry = get_fd_entry(fd);
    uint8_t* buffer = (uint8_t*)(uintptr_t)regs->rsi;
    uint64_t length = regs->rdx;
    uint64_t offset = regs->r10;
    struct file_handle* file;
    uint64_t available;

    if (!buffer && length != 0) return -(int64_t)LINUX_EFAULT;
    if (!entry || entry->kind != FD_KIND_VNODE) {
        syscall_linux_trace("pread64 bad fd", (int64_t)fd);
        return -(int64_t)LINUX_EBADF;
    }
    file = get_file_handle_by_index(entry->handle_index);
    if (!file) {
        syscall_linux_trace("pread64 bad handle", entry->handle_index);
        return -(int64_t)LINUX_EBADF;
    }
    if (file->node.type != VFS_NODE_TYPE_REGULAR) {
        return -(int64_t)LINUX_EISDIR;
    }
    if (offset >= file->node.size) return 0;
    available = (uint64_t)file->node.size - offset;
    if (length > available) length = available;
    if (vfs_read_node(&file->node, offset, buffer, length) != 0) {
        return -(int64_t)LINUX_EIO;
    }
    return (int64_t)length;
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

    if (!entry) {
        syscall_linux_trace("fstat bad fd", (int64_t)regs->rdi);
        return -(int64_t)LINUX_EBADF;
    }
    if (!st) return -(int64_t)LINUX_EFAULT;

    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file = get_file_handle_by_index(entry->handle_index);
        if (!file) {
            syscall_linux_trace("fstat bad handle", entry->handle_index);
            return -(int64_t)LINUX_EBADF;
        }
        fill_linux_node_stat(st, &file->node);
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

int64_t sys_lstat(struct syscall_regs* regs) {
    struct syscall_regs statat_regs = *regs;
    statat_regs.rdi = (uint64_t)(int64_t)LINUX_AT_FDCWD;
    statat_regs.rsi = regs->rdi;
    statat_regs.rdx = regs->rsi;
    statat_regs.r10 = LINUX_AT_SYMLINK_NOFOLLOW;
    return sys_newfstatat(&statat_regs);
}

int64_t sys_flock(struct syscall_regs* regs) {
    struct file_handle* file = get_vnode_handle(regs->rdi);
    uint32_t operation = (uint32_t)regs->rsi;
    uint32_t mode = operation & ~LINUX_LOCK_NB;

    if (!file) return -(int64_t)LINUX_EBADF;
    if (mode != LINUX_LOCK_SH && mode != LINUX_LOCK_EX &&
        mode != LINUX_LOCK_UN) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (mode == LINUX_LOCK_UN) {
        file->flock_mode = 0;
        return 0;
    }

    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        struct file_handle* other = &g_file_handles[i];
        if (!other->in_use || other == file || other->flock_mode == 0) continue;
        if (other->node.backend != file->node.backend ||
            other->node.inode != file->node.inode) {
            continue;
        }
        if (mode == LINUX_LOCK_EX || other->flock_mode == LINUX_LOCK_EX) {
            return -(int64_t)LINUX_EAGAIN;
        }
    }
    file->flock_mode = (uint8_t)mode;
    return 0;
}

int64_t sys_ftruncate(struct syscall_regs* regs) {
    struct file_handle* file = get_vnode_handle(regs->rdi);
    int64_t requested_size = (int64_t)regs->rsi;
    uint32_t new_size;
    int result = -1;

    if (!file) return -(int64_t)LINUX_EBADF;
    if (file->node.type != VFS_NODE_TYPE_REGULAR) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((file->open_flags & LINUX_O_ACCMODE) == 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (requested_size < 0 || (uint64_t)requested_size > UINT32_MAX) {
        return -(int64_t)LINUX_EINVAL;
    }
    new_size = (uint32_t)requested_size;
    if (file->node.backend == VFS_BACKEND_AOSFS) {
        result = aosfs_resize_path(file->node.path, new_size);
    } else if (file->node.backend == VFS_BACKEND_TMPFS) {
        result = tmpfs_resize_path(file->node.path, new_size);
    } else if (new_size == 0 && file->node.backend == VFS_BACKEND_FAT32) {
        uint32_t first_cluster = 0;
        uint32_t size = 0;
        result = fat32_truncate_path(file->node.path, &first_cluster, &size);
    } else if (new_size == 0 && file->node.backend == VFS_BACKEND_EXT4) {
        uint32_t size = 0;
        result = ext4_truncate_path(file->node.path, &size);
    }
    if (result != 0) return -(int64_t)LINUX_EIO;

    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (!g_file_handles[i].in_use ||
            g_file_handles[i].node.backend != file->node.backend ||
            g_file_handles[i].node.inode != file->node.inode) {
            continue;
        }
        g_file_handles[i].node.size = new_size;
    }
    return 0;
}

int64_t sys_statfs(struct syscall_regs* regs) {
    const char* path_user = (const char*)(uintptr_t)regs->rdi;
    struct linux_statfs* st =
        (struct linux_statfs*)(uintptr_t)regs->rsi;
    char path[MAX_EXEC_STRING];
    char resolved_path[MAX_EXEC_STRING];
    struct vfs_node node;
    int64_t rc;

    if (!st) return -(int64_t)LINUX_EFAULT;
    rc = copy_user_cstr(path_user, path, sizeof(path));
    if (rc < 0) return rc;
    rc = resolve_path_from_dirfd(LINUX_AT_FDCWD, path, resolved_path,
                                 sizeof(resolved_path));
    if (rc < 0) return rc;
    if (vfs_lookup(resolved_path, &node) != 0) {
        return -(int64_t)LINUX_ENOENT;
    }
    fill_linux_statfs(st, node.backend);
    return 0;
}

int64_t sys_fstatfs(struct syscall_regs* regs) {
    struct fd_entry* entry = get_fd_entry(regs->rdi);
    struct linux_statfs* st =
        (struct linux_statfs*)(uintptr_t)regs->rsi;
    uint8_t backend = VFS_BACKEND_SYNTHETIC;

    if (!entry) return -(int64_t)LINUX_EBADF;
    if (!st) return -(int64_t)LINUX_EFAULT;
    if (entry->kind == FD_KIND_VNODE) {
        struct file_handle* file =
            get_file_handle_by_index(entry->handle_index);
        if (!file) return -(int64_t)LINUX_EBADF;
        backend = file->node.backend;
    }
    fill_linux_statfs(st, backend);
    return 0;
}

int64_t sys_newfstatat(struct syscall_regs* regs) {
    int64_t dirfd = linux_signed_int_arg(regs->rdi);
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
            fill_linux_node_stat(st, &file->node);
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
        if (((flags & LINUX_AT_SYMLINK_NOFOLLOW)
                 ? vfs_lstat(resolved_path, &node)
                 : vfs_lookup(resolved_path, &node)) != 0) {
            return -(int64_t)LINUX_ENOENT;
        }
        fill_linux_node_stat(st, &node);
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
    if (!get_fd_entry(fd)) {
        syscall_linux_trace("close bad fd", (int64_t)fd);
        return -(int64_t)LINUX_EBADF;
    }
    close_fd_internal(fd);
    syscall_linux_trace("close", (int64_t)fd);
    return 0;
}
