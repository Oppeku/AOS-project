; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_PIPE 22
%define SYS_DUP 32
%define SYS_EXIT 60

_start:
    mov rax, SYS_PIPE
    lea rdi, [rel pipefds]
    syscall
    test rax, rax
    js .fail

    mov rax, SYS_DUP
    mov edi, [rel pipefds + 4]
    syscall
    test rax, rax
    js .fail
    mov r12, rax

    mov rax, SYS_WRITE
    mov edi, [rel pipefds + 4]
    lea rsi, [rel msg1]
    mov rdx, msg1_end - msg1
    syscall
    cmp rax, msg1_end - msg1
    jne .fail

    mov rax, SYS_WRITE
    mov rdi, r12
    lea rsi, [rel msg2]
    mov rdx, msg2_end - msg2
    syscall
    cmp rax, msg2_end - msg2
    jne .fail

    mov rax, SYS_CLOSE
    mov edi, [rel pipefds + 4]
    syscall
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall

    mov rax, SYS_READ
    mov edi, [rel pipefds]
    lea rsi, [rel buffer]
    mov rdx, msg_total_len
    syscall
    cmp rax, msg_total_len
    jne .fail

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel buffer]
    mov rdx, msg_total_len
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
pipefds:
    resd 2
buffer:
    resb 32

section .rodata
msg1:
    db "pipe "
msg1_end:
msg2:
    db "ok", 10
msg2_end:
msg_total_len equ (msg1_end - msg1) + (msg2_end - msg2)
ok_msg:
    db "pipetest: ok", 10
ok_msg_end:
fail_msg:
    db "pipetest: failed", 10
fail_msg_end:
