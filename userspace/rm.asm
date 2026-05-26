; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_UNLINKAT 263
%define SYS_EXIT 60
%define AT_FDCWD -100

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]
    cmp r12, 2
    jb usage

    mov rax, SYS_UNLINKAT
    mov rdi, AT_FDCWD
    mov rsi, [r13 + 8]
    xor rdx, rdx
    syscall
    test rax, rax
    js failed

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
    db "usage: rm FILE", 10
usage_msg_end:
fail_msg:
    db "rm: failed", 10
fail_msg_end:
