; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define SYS_ACCESS 21
%define SYS_FACCESSAT 269

%define AT_FDCWD -100
%define F_OK 0
%define X_OK 1
%define W_OK 2
%define R_OK 4

_start:
    mov rax, SYS_ACCESS
    lea rdi, [rel hello_path]
    mov rsi, F_OK
    syscall
    test rax, rax
    js .access_failed

    mov rax, SYS_ACCESS
    lea rdi, [rel hello_path]
    mov rsi, R_OK
    syscall
    test rax, rax
    js .access_failed

    mov rax, SYS_ACCESS
    lea rdi, [rel hello_path]
    mov rsi, W_OK
    syscall
    test rax, rax
    jns .unexpected_write_access

    mov rax, SYS_FACCESSAT
    mov rdi, AT_FDCWD
    lea rsi, [rel hello_path]
    mov rdx, X_OK
    syscall
    test rax, rax
    js .faccessat_failed

    mov rax, SYS_ACCESS
    lea rdi, [rel missing_path]
    mov rsi, F_OK
    syscall
    test rax, rax
    jns .missing_should_fail

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel success_msg]
    mov rdx, success_msg_end - success_msg
    syscall

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.access_failed:
    lea rsi, [rel access_failed_msg]
    mov rdx, access_failed_msg_end - access_failed_msg
    jmp .fail

.unexpected_write_access:
    lea rsi, [rel write_should_fail_msg]
    mov rdx, write_should_fail_msg_end - write_should_fail_msg
    jmp .fail

.faccessat_failed:
    lea rsi, [rel faccessat_failed_msg]
    mov rdx, faccessat_failed_msg_end - faccessat_failed_msg
    jmp .fail

.missing_should_fail:
    lea rsi, [rel missing_should_fail_msg]
    mov rdx, missing_should_fail_msg_end - missing_should_fail_msg

.fail:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall

    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

section .rodata
hello_path:
    db "hello.txt", 0

missing_path:
    db "missing.txt", 0

success_msg:
    db "accesstest: ok", 10
success_msg_end:

access_failed_msg:
    db "accesstest: access failed", 10
access_failed_msg_end:

write_should_fail_msg:
    db "accesstest: write access unexpectedly succeeded", 10
write_should_fail_msg_end:

faccessat_failed_msg:
    db "accesstest: faccessat failed", 10
faccessat_failed_msg_end:

missing_should_fail_msg:
    db "accesstest: missing file unexpectedly accessible", 10
missing_should_fail_msg_end:
