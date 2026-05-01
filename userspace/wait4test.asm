; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_FORK 57
%define SYS_EXIT 60
%define SYS_WAIT4 61

_start:
    mov rax, SYS_FORK
    syscall
    test rax, rax
    js .fail
    jz .child

    mov r12, rax
    mov rax, SYS_WAIT4
    mov rdi, r12
    lea rsi, [rel child_status]
    xor rdx, rdx
    xor r10, r10
    syscall
    cmp rax, r12
    jne .fail

    mov eax, [rel child_status]
    cmp eax, (42 << 8)
    jne .fail

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel ok_msg]
    mov rdx, ok_msg_end - ok_msg
    syscall
    xor rdi, rdi
    mov rax, SYS_EXIT
    syscall

.child:
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel child_msg]
    mov rdx, child_msg_end - child_msg
    syscall
    mov rdi, 42
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
child_status:
    resd 1

section .rodata
child_msg:
    db "child ran", 10
child_msg_end:
ok_msg:
    db "wait4: ok", 10
ok_msg_end:
fail_msg:
    db "wait4: failed", 10
fail_msg_end:
