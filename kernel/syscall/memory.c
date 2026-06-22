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
    int64_t fd = (int64_t)regs->r8;
    uint64_t page_flags = PAGE_PRESENT | PAGE_USER;
    uint64_t base = 0;
    uint64_t end = 0;

    (void)regs->r9;

    if (!proc || !proc->p4_table || len == 0) {
        return -(int64_t)LINUX_EINVAL;
    }
    if ((flags & LINUX_MAP_PRIVATE) == 0 || (flags & LINUX_MAP_ANONYMOUS) == 0 || fd != -1) {
        return -(int64_t)LINUX_EINVAL;
    }

    len = align_up_page(len);
    if (prot & LINUX_PROT_WRITE) {
        page_flags |= PAGE_WRITABLE;
    }

    if (addr != 0) {
        base = addr & ~0xFFFULL;
    } else {
        if (proc->mmap_next == 0) {
            proc->mmap_next = USER_MMAP_BASE;
        }
        base = proc->mmap_next;
    }
    end = base + len;

    if (map_zeroed_user_pages(proc->p4_table, base, end, page_flags) != 0) {
        return -(int64_t)LINUX_EIO;
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
