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

not_allowed:
    lea rsi, [rel not_allowed_prefix]
    mov rdx, not_allowed_prefix_end - not_allowed_prefix
    call write_stdout
    lea rsi, [rel user_info + USER_INFO_USERNAME]
    call write_cstring_stdout
    lea rsi, [rel not_allowed_suffix]
    mov rdx, not_allowed_suffix_end - not_allowed_suffix
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
    mov rax, AOS_SYS_SUDO_AUTH
    lea rdi, [rel empty_password]
    syscall
    test rax, rax
    jz .authenticated
    cmp rax, -1
    je not_allowed

    lea rsi, [rel password_prefix]
    mov rdx, password_prefix_end - password_prefix
    call write_stdout
    lea rsi, [rel user_info + USER_INFO_USERNAME]
    call write_cstring_stdout
    lea rsi, [rel password_suffix]
    mov rdx, password_suffix_end - password_suffix
    call write_stdout

    call read_password_line
    test rax, rax
    js .read_failed

    mov rax, AOS_SYS_SUDO_AUTH
    lea rdi, [rel password_buffer]
    syscall
    mov r15, rax
    call clear_password
    mov rax, r15
    ret

.authenticated:
    xor rax, rax
    ret

.read_failed:
    call clear_password
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

read_password_line:
    xor r14, r14
    lea rbx, [rel password_buffer]

.read:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rbx + r14]
    mov rdx, 1
    syscall
    test rax, rax
    js .failed
    jz .read

    mov al, [rbx + r14]
    cmp al, 10
    je .terminate
    cmp al, 13
    je .terminate
    test al, al
    je .terminate
    inc r14
    cmp r14, 127
    jb .read

.terminate:
    mov byte [rbx + r14], 0
    xor rax, rax
    ret

.failed:
    mov byte [rbx + r14], 0
    mov rax, -1
    ret

clear_password:
    lea rdi, [rel password_buffer]
    mov rcx, 128
    xor eax, eax

.clear:
    mov [rdi], al
    inc rdi
    loop .clear
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

not_allowed_prefix:
    db "sudo: "
not_allowed_prefix_end:

not_allowed_suffix:
    db " is not an administrator", 10
not_allowed_suffix_end:

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
