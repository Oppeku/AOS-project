; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_GETUID 102
%define SYS_GETGID 104
%define SYS_GETEUID 107
%define SYS_GETEGID 108
%define SYS_EXIT 60
%define AOS_SYS_USER_INFO 516

_start:
    mov rax, AOS_SYS_USER_INFO
    lea rdi, [rel user_info]
    syscall
    test rax, rax
    js .fallback_ids

    mov eax, [rel user_info]
    mov [rel uid_value], rax
    mov eax, [rel user_info + 4]
    mov [rel gid_value], rax
    mov eax, [rel user_info + 8]
    mov [rel euid_value], rax
    mov eax, [rel user_info + 12]
    mov [rel egid_value], rax
    jmp .print

.fallback_ids:
    mov rax, SYS_GETUID
    syscall
    mov [rel uid_value], rax

    mov rax, SYS_GETGID
    syscall
    mov [rel gid_value], rax

    mov rax, SYS_GETEUID
    syscall
    mov [rel euid_value], rax

    mov rax, SYS_GETEGID
    syscall
    mov [rel egid_value], rax

.print:
    lea rsi, [rel uid_label]
    mov rdx, uid_label_end - uid_label
    call write_stdout
    mov rdi, [rel uid_value]
    call write_u64_decimal
    mov rdi, [rel uid_value]
    call write_name

    lea rsi, [rel gid_label]
    mov rdx, gid_label_end - gid_label
    call write_stdout
    mov rdi, [rel gid_value]
    call write_u64_decimal
    mov rdi, [rel gid_value]
    call write_group

    lea rsi, [rel euid_label]
    mov rdx, euid_label_end - euid_label
    call write_stdout
    mov rdi, [rel euid_value]
    call write_u64_decimal
    mov rdi, [rel euid_value]
    call write_name

    lea rsi, [rel egid_label]
    mov rdx, egid_label_end - egid_label
    call write_stdout
    mov rdi, [rel egid_value]
    call write_u64_decimal
    mov rdi, [rel egid_value]
    call write_group

    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

write_name:
    cmp byte [rel user_info + 16], 0
    je .fallback
    lea rsi, [rel open_paren]
    mov rdx, open_paren_end - open_paren
    call write_stdout
    lea rsi, [rel user_info + 16]
    call write_cstring_stdout
    lea rsi, [rel close_paren]
    mov rdx, close_paren_end - close_paren
    jmp write_stdout
.fallback:
    cmp rdi, 0
    jne .done
    lea rsi, [rel root_user_suffix]
    mov rdx, root_user_suffix_end - root_user_suffix
    jmp write_stdout
.done:
    ret

write_group:
    cmp rdi, 0
    je .root
    cmp rdi, 1000
    je .users
    ret
.root:
    lea rsi, [rel root_group_suffix]
    mov rdx, root_group_suffix_end - root_group_suffix
    jmp write_stdout
.users:
    lea rsi, [rel users_group_suffix]
    mov rdx, users_group_suffix_end - users_group_suffix
    jmp write_stdout

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

write_cstring_stdout:
    xor rdx, rdx
.count:
    cmp byte [rsi + rdx], 0
    je .emit
    inc rdx
    jmp .count
.emit:
    jmp write_stdout

write_u64_decimal:
    lea rbx, [rel decimal_buffer + 20]
    mov byte [rbx], 0
    mov rax, rdi
    test rax, rax
    jnz .convert
    dec rbx
    mov byte [rbx], '0'
    jmp .emit_number
.convert:
    mov rcx, 10
.loop:
    xor rdx, rdx
    div rcx
    dec rbx
    add dl, '0'
    mov [rbx], dl
    test rax, rax
    jnz .loop
.emit_number:
    mov rsi, rbx
    call write_cstring_stdout
    ret

section .bss
uid_value:
    resq 1
gid_value:
    resq 1
euid_value:
    resq 1
egid_value:
    resq 1
user_info:
    resb 304
decimal_buffer:
    resb 21

section .rodata
uid_label:
    db "uid="
uid_label_end:

gid_label:
    db " gid="
gid_label_end:

euid_label:
    db " euid="
euid_label_end:

egid_label:
    db " egid="
egid_label_end:

root_user_suffix:
    db "(root)"
root_user_suffix_end:

root_group_suffix:
    db "(root)"
root_group_suffix_end:

users_group_suffix:
    db "(users)"
users_group_suffix_end:

open_paren:
    db "("
open_paren_end:

close_paren:
    db ")"
close_paren_end:

newline_msg:
    db 10
newline_msg_end:
