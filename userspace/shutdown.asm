; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_SHUTDOWN 513

_start:
    lea rsi, [rel shutdown_msg]
    mov rdx, shutdown_msg_end - shutdown_msg
    call write_stdout

    mov rax, AOS_SYS_SHUTDOWN
    syscall

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
shutdown_msg:
    db "AOS: shutting down...", 10
shutdown_msg_end:
fail_msg:
    db "shutdown: firmware did not power off this machine", 10
fail_msg_end:
