<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2026 Oppeko -->

# Linux Compatibility Roadmap

This kernel now has a Linux-style syscall ABI entry path and basic syscalls (`read`, `write`, `close`, `getpid`, `uname`, `exit`).

To run BusyBox and GNU coreutils in earnest, implement these phases:

1. Userspace Program Format
- Load ELF64 binaries from initrd (not flat binaries).
- Map PT_LOAD segments with correct R/W/X protections.
- Set initial user stack layout (`argc`, `argv`, `envp`, `auxv`).

2. Process Model
- Add process struct, PID allocation, and per-process address spaces.
- Implement `fork`/`vfork`, `execve`, `wait4`, and signal basics.

3. Memory Syscalls
- Implement `brk`, `mmap`, `munmap`, `mprotect`, and `arch_prctl`.
- Add copy-to/from-user helpers and pointer validation.

4. VFS and File Descriptors
- Replace stubs with a real VFS backend (initrd tmpfs/ext2).
- Implement `openat`, `newfstatat`, `statx`/`fstat`, `lseek`, `readv`, `writev`, `getdents64`, `dup`, `pipe`, `ioctl`.

5. Time, Misc, and Runtime Expectations
- Implement `clock_gettime`, `nanosleep`, `getrandom`, `prlimit64`, `set_tid_address`, `set_robust_list`, `rseq` (stubbed where acceptable).

6. Userland Bring-up
- Boot `/bin/busybox` as PID 1 from initrd.
- Provide `/dev/console`, `/proc`, and minimal `/etc`.
- Validate with `busybox sh`, then test coreutils one-by-one.
