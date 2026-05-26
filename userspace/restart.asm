; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_RESTART 514

_start:
    lea rsi, [rel restart_msg]
    mov rdx, restart_msg_end - restart_msg
    call write_stdout

    mov rax, AOS_SYS_RESTART
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
restart_msg:
    db "AOS: restarting...", 10
restart_msg_end:
fail_msg:
    db "restart: firmware did not reset this machine", 10
fail_msg_end:
