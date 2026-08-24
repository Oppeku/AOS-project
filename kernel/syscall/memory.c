#include "syscall_internal.h"

/* Included by kernel/syscall.c. Feature syscall group. */

static void memory_trace_u64(uint64_t value) {
    char digits[24];
    size_t at = sizeof(digits);

    digits[--at] = '\0';
    do {
        digits[--at] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0 && at != 0);
    serial_print(&digits[at]);
}

static void memory_trace_hex(uint64_t value) {
    char hex[19];

    hex[0] = '0';
    hex[1] = 'x';
    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (uint8_t)((value >> (60 - i * 4)) & 0xF);
        hex[2 + i] = (char)(nibble < 10 ? '0' + nibble
                                       : 'A' + nibble - 10);
    }
    hex[18] = '\0';
    serial_print(hex);
}

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

static void release_user_pages(uint64_t* user_p4,
                               uint64_t start, uint64_t end) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* process = &process_list[i];

        if (process->status == PROCESS_STATUS_DEAD ||
            process->status == PROCESS_STATUS_ZOMBIE ||
            process->p4_table != user_p4 || process->regs.rsp < start ||
            process->regs.rsp >= end) {
            continue;
        }
        serial_print("AOS: unmapping live PID ");
        memory_trace_u64(process->pid);
        serial_print(" stack at ");
        memory_trace_hex(process->regs.rsp);
        serial_print(" by PID ");
        memory_trace_u64(get_current_process()
                             ? get_current_process()->pid
                             : 0);
        serial_print("\n");
    }
    for (uint64_t va = start; va < end; va += 4096ULL) {
        uint64_t phys = vmm_unmap_page(user_p4, va);
        if (phys) pmm_free_block((void*)phys);
    }
}

static int shares_address_space(const process_t* process,
                                const process_t* owner) {
    return process && owner && process->status != PROCESS_STATUS_DEAD &&
           process->p4_table && process->p4_table == owner->p4_table;
}

static uint64_t shared_mmap_cursor(const process_t* owner) {
    uint64_t cursor = USER_MMAP_BASE;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        const process_t* process = &process_list[i];
        if (shares_address_space(process, owner) &&
            process->mmap_next > cursor) {
            cursor = process->mmap_next;
        }
    }
    return align_up_page(cursor);
}

static void publish_mmap_cursor(const process_t* owner, uint64_t cursor) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* process = &process_list[i];
        if (shares_address_space(process, owner) &&
            process->mmap_next < cursor) {
            process->mmap_next = cursor;
        }
    }
}

static void synchronize_shared_brk(process_t* owner) {
    const process_t* source = owner;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        const process_t* process = &process_list[i];
        if (!shares_address_space(process, owner)) continue;
        if (process->brk_mapped_end > source->brk_mapped_end ||
            (process->brk_mapped_end == source->brk_mapped_end &&
             process->brk_current > source->brk_current)) {
            source = process;
        }
    }
    owner->brk_base = source->brk_base;
    owner->brk_current = source->brk_current;
    owner->brk_mapped_end = source->brk_mapped_end;
}

static void publish_shared_brk(const process_t* owner) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t* process = &process_list[i];
        if (!shares_address_space(process, owner)) continue;
        process->brk_base = owner->brk_base;
        process->brk_current = owner->brk_current;
        process->brk_mapped_end = owner->brk_mapped_end;
    }
}

int64_t sys_brk(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t requested = regs->rdi;
    uint64_t target = 0;

    if (!proc || !proc->p4_table) {
        return 0;
    }

    synchronize_shared_brk(proc);

    if (proc->brk_base == 0) {
        proc->brk_base = align_up_page(0x450000ULL);
        proc->brk_current = proc->brk_base;
        proc->brk_mapped_end = proc->brk_base;
        publish_shared_brk(proc);
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
    publish_shared_brk(proc);
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
    uint64_t cursor = 0;
    struct file_handle* file = NULL;
    int anonymous = (flags & LINUX_MAP_ANONYMOUS) != 0;
    int fixed = (flags & LINUX_MAP_FIXED) != 0;
    const uint64_t supported_flags =
        LINUX_MAP_SHARED | LINUX_MAP_PRIVATE | LINUX_MAP_FIXED |
        LINUX_MAP_ANONYMOUS | LINUX_MAP_32BIT |
        LINUX_MAP_GROWSDOWN | LINUX_MAP_DENYWRITE |
        LINUX_MAP_EXECUTABLE | LINUX_MAP_NORESERVE |
        LINUX_MAP_POPULATE | LINUX_MAP_STACK |
        LINUX_MAP_FIXED_NOREPLACE;

    if (!proc || !proc->p4_table || len == 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((prot & ~(LINUX_PROT_READ | LINUX_PROT_WRITE |
                  LINUX_PROT_EXEC)) != 0 ||
        (flags & ~supported_flags) != 0 ||
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
    if (len == 0) return -(int64_t)LINUX_EINVAL;
    if (prot & LINUX_PROT_WRITE) {
        page_flags |= PAGE_WRITABLE;
    }

    if (fixed) {
        if ((addr & 0xFFFULL) != 0) return -(int64_t)LINUX_EINVAL;
        base = addr;
    } else {
        uint64_t hint = addr & ~0xFFFULL;
        cursor = shared_mmap_cursor(proc);
        base = addr != 0 && hint >= cursor ? hint : cursor;
    }
    end = base + len;
    if (base < AOS_USER_SPACE_START || end < base ||
        end > 0x00007FFFFFFFF000ULL ||
        (base < USER_STACK_BASE + USER_STACK_SIZE &&
         end > USER_STACK_BASE)) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (!fixed) publish_mmap_cursor(proc, end);

    if (len >= 64ULL * 1024ULL * 1024ULL) {
        serial_print("linux-compat: mmap bytes=");
        memory_trace_u64(len);
        serial_print(" prot=");
        memory_trace_u64(prot);
        serial_print(" flags=");
        memory_trace_u64(flags);
        serial_print("\n");
    }
    if (file && syscall_linux_trace_enabled()) {
        serial_print("linux-compat: mmap ");
        serial_print(file->node.path);
        serial_print(" base=");
        memory_trace_hex(base);
        serial_print(" bytes=");
        memory_trace_u64(len);
        serial_print(" offset=");
        memory_trace_u64(offset);
        serial_print("\n");
    }

    /* Modern runtimes reserve very large PROT_NONE ranges, then commit
     * smaller pieces with mprotect or MAP_FIXED. A reservation consumes
     * virtual address space only. */
    if (prot == 0) {
        if (fixed) release_user_pages(proc->p4_table, base, end);
        return (int64_t)base;
    }

    for (uint64_t va = base; va < end; va += 4096ULL) {
        uint8_t* phys;

        if (fixed) {
            uint64_t old_phys = vmm_unmap_page(proc->p4_table, va);
            if (old_phys) pmm_free_block((void*)old_phys);
        }
        phys = (uint8_t*)pmm_alloc_block();
        if (!phys) {
            release_user_pages(proc->p4_table, base, va);
            return -(int64_t)LINUX_ENOMEM;
        }
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
                    release_user_pages(proc->p4_table, base, va);
                    return -(int64_t)LINUX_EIO;
                }
            }
        }
        vmm_map_page(proc->p4_table, va, (uint64_t)phys, page_flags);
    }

    return (int64_t)base;
}

int64_t sys_mprotect(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    uint64_t address = regs->rdi;
    uint64_t length = regs->rsi;
    uint64_t prot = regs->rdx;
    uint64_t end;
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;

    if (!proc || !proc->p4_table || length == 0 ||
        (address & 0xFFFULL) != 0 ||
        (prot & ~(LINUX_PROT_READ | LINUX_PROT_WRITE |
                  LINUX_PROT_EXEC)) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    end = align_up_page(address + length);
    if (end < address || address < AOS_USER_SPACE_START ||
        end > 0x00007FFFFFFFF000ULL) {
        return -(int64_t)LINUX_ENOMEM;
    }
    if (prot & LINUX_PROT_WRITE) page_flags |= PAGE_WRITABLE;

    if (prot == 0) {
        release_user_pages(proc->p4_table, address, end);
        return 0;
    }
    for (uint64_t va = address; va < end; va += 4096ULL) {
        if (!vmm_page_present(proc->p4_table, va)) {
            uint64_t phys = (uint64_t)pmm_alloc_block();
            if (!phys) return -(int64_t)LINUX_ENOMEM;
            local_memset((void*)phys, 0, 4096);
            vmm_map_page(proc->p4_table, va, phys, page_flags);
        } else if (vmm_protect_page(proc->p4_table, va, page_flags) != 0) {
            return -(int64_t)LINUX_ENOMEM;
        }
    }
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

    release_user_pages(proc->p4_table, start, end);
    return 0;
}

int64_t sys_madvise(struct syscall_regs* regs) {
    uint64_t address = regs->rdi;
    uint64_t length = regs->rsi;
    uint64_t advice = regs->rdx;

    if ((address & 0xFFFULL) != 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if (length == 0) return 0;

    switch (advice) {
        case 0:  /* MADV_NORMAL */
        case 1:  /* MADV_RANDOM */
        case 2:  /* MADV_SEQUENTIAL */
        case 3:  /* MADV_WILLNEED */
        case 4:  /* MADV_DONTNEED */
        case 8:  /* MADV_FREE */
        case 10: /* MADV_DONTFORK */
        case 11: /* MADV_DOFORK */
        case 12: /* MADV_MERGEABLE */
        case 13: /* MADV_UNMERGEABLE */
        case 14: /* MADV_HUGEPAGE */
        case 15: /* MADV_NOHUGEPAGE */
        case 16: /* MADV_DONTDUMP */
        case 17: /* MADV_DODUMP */
        case 18: /* MADV_WIPEONFORK */
        case 19: /* MADV_KEEPONFORK */
        case 20: /* MADV_COLD */
        case 21: /* MADV_PAGEOUT */
        case 22: /* MADV_POPULATE_READ */
        case 23: /* MADV_POPULATE_WRITE */
            return 0;
        default:
            return -(int64_t)LINUX_EINVAL;
    }
}

int64_t sys_set_tid_address(struct syscall_regs* regs) {
    process_t* proc = get_current_process();
    if (proc) {
        proc->clear_child_tid = regs->rdi;
        return (int64_t)proc->pid;
    }
    return 1;
}
