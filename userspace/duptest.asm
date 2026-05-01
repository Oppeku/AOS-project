; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_DUP 32
%define SYS_DUP2 33
%define SYS_EXIT 60
%define SYS_OPENAT 257

%define AT_FDCWD -100

_start:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel hello_path]
    xor rdx, rdx
    xor r10, r10
    syscall
    test rax, rax
    js .fail
    mov r12, rax

    mov rax, SYS_DUP
    mov rdi, r12
    syscall
    test rax, rax
    js .fail
    mov r13, rax

    mov rax, SYS_READ
    mov rdi, r12
    lea rsi, [rel buf1]
    mov rdx, 7
    syscall
    cmp rax, 7
    jne .fail

    mov rax, SYS_READ
    mov rdi, r13
    lea rsi, [rel buf2]
    mov rdx, 7
    syscall
    cmp rax, 7
    jne .fail

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel buf1]
    mov rdx, 7
    syscall

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel buf2]
    mov rdx, 7
    syscall

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel newline]
    mov rdx, 1
    syscall

    mov rax, SYS_DUP2
    mov rdi, 1
    mov rsi, 9
    syscall
    cmp rax, 9
    jne .fail

    mov rax, SYS_WRITE
    mov rdi, 9
    lea rsi, [rel dup2_msg]
    mov rdx, dup2_msg_end - dup2_msg
    syscall

    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall
    mov rax, SYS_CLOSE
    mov rdi, r13
    syscall
    mov rax, SYS_CLOSE
    mov rdi, 9
    syscall

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel ok_msg]
    mov rdx, ok_msg_end - ok_msg
    syscall
    xor rdi, rdi
    mov rax, SYS_EXIT
    syscall

.fail:
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    syscall
    mov rdi, 1
    mov rax, SYS_EXIT
    syscall

section .bss
buf1:
    resb 8
buf2:
    resb 8

section .rodata
hello_path:
    db "hello.txt", 0
newline:
    db 10
dup2_msg:
    db "dup2 ok", 10
dup2_msg_end:
ok_msg:
    db "duptest: ok", 10
ok_msg_end:
fail_msg:
    db "duptest: failed", 10
fail_msg_end:
