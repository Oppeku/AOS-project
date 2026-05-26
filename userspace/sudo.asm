; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_READ 0
%define SYS_EXECVE 59
%define SYS_EXIT 60
%define AOS_SYS_USER_INFO 516
%define AOS_SYS_SUDO_AUTH 517
%define USER_INFO_EUID 8
%define USER_INFO_USERNAME 16

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]
    cmp r12, 2
    jb usage

    call authenticate
    test rax, rax
    js auth_failed

run_command:
    mov rax, SYS_EXECVE
    mov rdi, [r13 + 8]
    lea rsi, [r13 + 8]
    xor rdx, rdx
    syscall
    test rax, rax
    jns exec_failed

    call build_root_command_path
    mov rax, SYS_EXECVE
    lea rdi, [rel path_buffer]
    lea rsi, [r13 + 8]
    xor rdx, rdx
    syscall
    test rax, rax
    jns exec_failed

    call build_commands_command_path
    mov rax, SYS_EXECVE
    lea rdi, [rel path_buffer]
    lea rsi, [r13 + 8]
    xor rdx, rdx
    syscall

exec_failed:
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 127
    syscall

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

auth_failed:
    lea rsi, [rel auth_failed_msg]
    mov rdx, auth_failed_msg_end - auth_failed_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

authenticate:
    mov rax, AOS_SYS_USER_INFO
    lea rdi, [rel user_info]
    syscall
    test rax, rax
    js .need_password

    mov eax, [rel user_info + USER_INFO_EUID]
    test eax, eax
    jnz .need_password

    lea rsi, [rel live_msg]
    mov rdx, live_msg_end - live_msg
    call write_stdout
    mov rax, AOS_SYS_SUDO_AUTH
    lea rdi, [rel empty_password]
    syscall
    ret

.need_password:
    lea rsi, [rel password_prefix]
    mov rdx, password_prefix_end - password_prefix
    call write_stdout
    lea rsi, [rel user_info + USER_INFO_USERNAME]
    call write_cstring_stdout
    lea rsi, [rel password_suffix]
    mov rdx, password_suffix_end - password_suffix
    call write_stdout

    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel password_buffer]
    mov rdx, 127
    syscall
    test rax, rax
    jle .read_failed

    mov rdi, rax
    call trim_password
    mov rax, AOS_SYS_SUDO_AUTH
    lea rdi, [rel password_buffer]
    syscall
    ret

.read_failed:
    mov rax, -1
    ret

build_root_command_path:
    lea rdi, [rel path_buffer]
    mov byte [rdi], '/'
    inc rdi
    mov rsi, [r13 + 8]

.copy:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz .copy
    ret

build_commands_command_path:
    lea rdi, [rel path_buffer]
    lea rsi, [rel commands_prefix]

.prefix:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz .prefix

    dec rdi
    mov rsi, [r13 + 8]

.command:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz .command
    ret

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

write_cstring_stdout:
    push rsi
    xor rdx, rdx

.len:
    cmp byte [rsi + rdx], 0
    je .write
    inc rdx
    jmp .len

.write:
    pop rsi
    test rdx, rdx
    jz .done
    call write_stdout
.done:
    ret

trim_password:
    lea rsi, [rel password_buffer]
    xor rcx, rcx

.loop:
    cmp rcx, rdi
    jae .terminate
    mov al, [rsi + rcx]
    cmp al, 10
    je .terminate
    cmp al, 13
    je .terminate
    test al, al
    je .terminate
    inc rcx
    jmp .loop

.terminate:
    mov byte [rsi + rcx], 0
    ret

section .bss
path_buffer:
    resb 256
password_buffer:
    resb 128
user_info:
    resb 304

section .rodata
usage_msg:
    db "usage: sudo COMMAND [ARGS...]", 10
usage_msg_end:

live_msg:
    db "sudo: root session, no password required", 10
live_msg_end:

fail_msg:
    db "sudo: command failed", 10
fail_msg_end:

auth_failed_msg:
    db "sudo: authentication failed", 10
auth_failed_msg_end:

password_prefix:
    db "[sudo] password for "
password_prefix_end:

password_suffix:
    db ": "
password_suffix_end:

empty_password:
    db 0

commands_prefix:
    db "/commands/", 0
