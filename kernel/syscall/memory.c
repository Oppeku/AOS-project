#include "syscall_internal.h"

/* Included by kernel/syscall.c. Feature syscall group. */

static int map_zeroed_user_pages(uint64_t* user_p4, uint64_t start, uint64_t end, uint64_t page_flags) {
    for (uint64_t va = start; va < end; va += 4096ULL) {
        void* phys = pmm_alloc_block();
        if (!phys) {
            return -1;
        }
        local_memset(phys, 0, 4096);
        vmm_map_page(user_p4, va, (uint64_t)phys, page_flags);
    }
    return 0;
}

int64_t sys_brk(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t requested = regs->rdi;
    uint64_t target = 0;

    if (!proc || !proc->p4_table) {
        return 0;
    }

    if (proc->brk_base == 0) {
        proc->brk_base = align_up_page(0x450000ULL);
        proc->brk_current = proc->brk_base;
        proc->brk_mapped_end = proc->brk_base;
    }

    if (requested == 0) {
        return (int64_t)proc->brk_current;
    }
    if (requested < proc->brk_base) {
        return (int64_t)proc->brk_current;
    }

    target = align_up_page(requested);
    if (target > proc->brk_mapped_end) {
        if (map_zeroed_user_pages(proc->p4_table, proc->brk_mapped_end, target, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) != 0) {
            return (int64_t)proc->brk_current;
        }
        proc->brk_mapped_end = target;
    } else if (target < proc->brk_mapped_end) {
        for (uint64_t va = target; va < proc->brk_mapped_end; va += 4096ULL) {
            uint64_t phys = vmm_unmap_page(proc->p4_table, va);
            if (phys) {
                pmm_free_block((void*)phys);
            }
        }
        proc->brk_mapped_end = target;
    }

    proc->brk_current = requested;
    return (int64_t)proc->brk_current;
}

int64_t sys_mmap(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t addr = regs->rdi;
    uint64_t len = regs->rsi;
    uint64_t prot = regs->rdx;
    uint64_t flags = regs->r10;
    int64_t fd = linux_signed_int_arg(regs->r8);
    uint64_t offset = regs->r9;
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    uint64_t base = 0;
    uint64_t end = 0;
    struct file_handle* file = NULL;
    int anonymous = (flags & LINUX_MAP_ANONYMOUS) != 0;
    int fixed = (flags & LINUX_MAP_FIXED) != 0;
    const uint64_t supported_flags =
        LINUX_MAP_SHARED | LINUX_MAP_PRIVATE | LINUX_MAP_FIXED |
        LINUX_MAP_ANONYMOUS | LINUX_MAP_DENYWRITE |
        LINUX_MAP_EXECUTABLE | LINUX_MAP_FIXED_NOREPLACE;

    if (!proc || !proc->p4_table || len == 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((flags & ~supported_flags) != 0 ||
        ((flags & LINUX_MAP_PRIVATE) == 0 &&
         (flags & LINUX_MAP_SHARED) == 0) ||
        ((flags & LINUX_MAP_PRIVATE) != 0 &&
         (flags & LINUX_MAP_SHARED) != 0) ||
        (offset & 0xFFFULL) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (flags & LINUX_MAP_FIXED_NOREPLACE) {
        return -(int64_t)LINUX_ENOSYS;
    }
    if (anonymous) {
        if (fd != -1 || offset != 0) return -(int64_t)LINUX_EINVAL;
    } else {
        struct fd_entry* entry;
        if (fd < 0) return -(int64_t)LINUX_EBADF;
        entry = get_fd_entry((uint64_t)fd);
        if (!entry || entry->kind != FD_KIND_VNODE) {
            syscall_linux_trace("mmap bad fd", fd);
            return -(int64_t)LINUX_EBADF;
        }
        file = get_file_handle_by_index(entry->handle_index);
        if (!file || file->node.type != VFS_NODE_TYPE_REGULAR) {
            syscall_linux_trace("mmap bad handle",
                                entry ? entry->handle_index : -1);
            return -(int64_t)LINUX_EBADF;
        }
    }

    len = align_up_page(len);
    if (prot & LINUX_PROT_WRITE) {
        page_flags |= PAGE_WRITABLE;
    }

    if (fixed) {
        if ((addr & 0xFFFULL) != 0) return -(int64_t)LINUX_EINVAL;
        base = addr;
    } else if (addr != 0) {
        base = addr & ~0xFFFULL;
    } else {
        if (proc->mmap_next == 0) {
            proc->mmap_next = USER_MMAP_BASE;
        }
        base = proc->mmap_next;
    }
    end = base + len;
    if (base < AOS_USER_SPACE_START || end < base ||
        end > 0x00007FFFFFFFF000ULL ||
        (base < USER_STACK_BASE + USER_STACK_SIZE &&
         end > USER_STACK_BASE)) {
        return -(int64_t)LINUX_EINVAL;
    }

    for (uint64_t va = base; va < end; va += 4096ULL) {
        uint8_t* phys;

        if (fixed) {
            uint64_t old_phys = vmm_unmap_page(proc->p4_table, va);
            if (old_phys) pmm_free_block((void*)old_phys);
        }
        phys = (uint8_t*)pmm_alloc_block();
        if (!phys) return -(int64_t)LINUX_EIO;
        local_memset(phys, 0, 4096);
        if (file) {
            uint64_t file_offset = offset + (va - base);
            if (file_offset < file->node.size) {
                uint64_t available =
                    (uint64_t)file->node.size - file_offset;
                uint64_t to_read = available < 4096ULL
                                       ? available
                                       : 4096ULL;
                if (vfs_read_node(&file->node, file_offset, phys,
                                  to_read) != 0) {
                    pmm_free_block(phys);
                    return -(int64_t)LINUX_EIO;
                }
            }
        }
        vmm_map_page(proc->p4_table, va, (uint64_t)phys, page_flags);
    }

    if (addr == 0) {
        proc->mmap_next = end;
    }
    return (int64_t)base;
}

int64_t sys_mprotect(struct syscall_regs* regs) {
    (void)regs;
    return 0;
}

int64_t sys_munmap(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t addr = regs->rdi;
    uint64_t len = regs->rsi;
    uint64_t start = addr & ~0xFFFULL;
    uint64_t end = align_up_page(addr + len);

    if (!proc || !proc->p4_table || len == 0 || end < start) {
        return -(int64_t)LINUX_EINVAL;
    }

    for (uint64_t va = start; va < end; va += 4096ULL) {
        uint64_t phys = vmm_unmap_page(proc->p4_table, va);
        if (phys) {
            pmm_free_block((void*)phys);
        }
    }
    return 0;
}

int64_t sys_set_tid_address(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    if (proc) {
        proc->clear_child_tid = regs->rdi;
        return (int64_t)proc->pid;
    }
    return 1;
}
