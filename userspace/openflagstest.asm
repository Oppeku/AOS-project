; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define SYS_OPENAT 257
%define SYS_CLOSE 3
%define SYS_READ 0

%define AT_FDCWD -100
%define O_RDONLY 0
%define O_CREAT 64
%define O_DIRECTORY 0x10000

_start:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel root_path]
    mov rdx, O_DIRECTORY
    xor r10, r10
    syscall
    test rax, rax
    js .root_open_failed
    mov r12, rax

    mov rax, SYS_OPENAT
    mov rdi, r12
    lea rsi, [rel hello_path]
    mov rdx, O_RDONLY
    xor r10, r10
    syscall
    test rax, rax
    js .relative_open_failed
    mov r13, rax

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel hello_path]
    mov rdx, O_DIRECTORY
    xor r10, r10
    syscall
    test rax, rax
    jns .dir_should_fail

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel hello_path]
    mov rdx, O_CREAT
    xor r10, r10
    syscall
    test rax, rax
    jns .create_should_fail

    mov rax, SYS_READ
    mov rdi, r13
    lea rsi, [rel buffer]
    mov rdx, buffer_end - buffer
    syscall
    test rax, rax
    jle .read_failed

    mov r14, rax
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel buffer]
    mov rdx, r14
    syscall

    mov rax, SYS_CLOSE
    mov rdi, r13
    syscall
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel success_msg]
    mov rdx, success_msg_end - success_msg
    syscall

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.root_open_failed:
    lea rsi, [rel root_open_failed_msg]
    mov rdx, root_open_failed_msg_end - root_open_failed_msg
    jmp .fail

.relative_open_failed:
    lea rsi, [rel relative_open_failed_msg]
    mov rdx, relative_open_failed_msg_end - relative_open_failed_msg
    jmp .fail

.dir_should_fail:
    lea rsi, [rel dir_should_fail_msg]
    mov rdx, dir_should_fail_msg_end - dir_should_fail_msg
    jmp .fail

.create_should_fail:
    lea rsi, [rel create_should_fail_msg]
    mov rdx, create_should_fail_msg_end - create_should_fail_msg
    jmp .fail

.read_failed:
    lea rsi, [rel read_failed_msg]
    mov rdx, read_failed_msg_end - read_failed_msg

.fail:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

section .bss
buffer:
    resb 64
buffer_end:

section .rodata
root_path:
    db ".", 0

hello_path:
    db "hello.txt", 0

success_msg:
    db "openflagstest: ok", 10
success_msg_end:

root_open_failed_msg:
    db "openflagstest: root open failed", 10
root_open_failed_msg_end:

relative_open_failed_msg:
    db "openflagstest: relative open failed", 10
relative_open_failed_msg_end:

dir_should_fail_msg:
    db "openflagstest: O_DIRECTORY on file unexpectedly succeeded", 10
dir_should_fail_msg_end:

create_should_fail_msg:
    db "openflagstest: O_CREAT unexpectedly succeeded", 10
create_should_fail_msg_end:

read_failed_msg:
    db "openflagstest: read failed", 10
read_failed_msg_end:
