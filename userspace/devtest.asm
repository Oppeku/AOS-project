; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_OPEN 2
%define SYS_CLOSE 3
%define SYS_EXIT 60
%define O_WRONLY 1
%define O_RDWR 2

_start:
    lea rdi, [rel path_console]
    mov rsi, O_WRONLY
    call open_path
    test rax, rax
    js fail
    mov r12, rax

    mov rdi, r12
    lea rsi, [rel console_msg]
    mov rdx, console_msg_end - console_msg
    call write_fd
    cmp rax, console_msg_end - console_msg
    jne fail
    mov rdi, r12
    call close_fd

    lea rdi, [rel path_tty0]
    mov rsi, O_WRONLY
    call open_path
    test rax, rax
    js fail
    mov rdi, rax
    call close_fd

    lea rdi, [rel path_null]
    mov rsi, O_RDWR
    call open_path
    test rax, rax
    js fail
    mov r12, rax

    mov rdi, r12
    lea rsi, [rel null_msg]
    mov rdx, null_msg_end - null_msg
    call write_fd
    cmp rax, null_msg_end - null_msg
    jne fail

    mov rdi, r12
    lea rsi, [rel one_byte]
    mov rdx, 1
    mov rax, SYS_READ
    syscall
    test rax, rax
    jne fail

    mov rdi, r12
    call close_fd

    lea rsi, [rel ok_msg]
    mov rdx, ok_msg_end - ok_msg
    call write_stdout
    xor rdi, rdi
    mov rax, SYS_EXIT
    syscall

fail:
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rdi, 1
    mov rax, SYS_EXIT
    syscall

open_path:
    mov rax, SYS_OPEN
    syscall
    ret

write_fd:
    mov rax, SYS_WRITE
    syscall
    ret

close_fd:
    mov rax, SYS_CLOSE
    syscall
    ret

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .rodata
path_console: db "/dev/console", 0
path_tty0: db "/dev/tty0", 0
path_null: db "/dev/null", 0
console_msg:
    db "devtest: /dev/console write ok", 10
console_msg_end:
null_msg:
    db "discarded by null", 10
null_msg_end:
ok_msg:
    db "devtest: /dev/tty0 open ok", 10
    db "devtest: /dev/null read/write ok", 10
    db "devtest: all device checks passed", 10
ok_msg_end:
fail_msg:
    db "devtest: failed", 10
fail_msg_end:

section .bss
one_byte: resb 1
