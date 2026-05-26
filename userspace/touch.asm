; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_OPENAT 257
%define SYS_EXIT 60
%define AT_FDCWD -100
%define O_WRONLY 1
%define O_CREAT 64

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]
    cmp r12, 2
    jb usage

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, [r13 + 8]
    mov rdx, O_WRONLY | O_CREAT
    xor r10, r10
    syscall
    test rax, rax
    js failed

    mov rdi, rax
    mov rax, SYS_CLOSE
    syscall

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

failed:
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .rodata
usage_msg:
    db "usage: touch FILE", 10
usage_msg_end:
fail_msg:
    db "touch: failed", 10
fail_msg_end:
