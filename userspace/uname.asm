; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_UNAME 63
%define SYS_EXIT 60

%define UTS_FIELD_SIZE 65
%define UTS_SYSNAME_OFF 0
%define UTS_NODENAME_OFF 65
%define UTS_RELEASE_OFF 130
%define UTS_VERSION_OFF 195
%define UTS_MACHINE_OFF 260
%define UTS_DOMAIN_OFF 325
%define UTS_SIZE 390

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]
    xor r14d, r14d

    cmp r12, 2
    jb .read_uname

    mov rdi, [r13 + 8]
    lea rsi, [rel all_arg]
    call strcmp
    test eax, eax
    jz .enable_all

    mov rdi, [r13 + 8]
    lea rsi, [rel long_all_arg]
    call strcmp
    test eax, eax
    jz .enable_all

    cmp r12, 3
    jb usage
    mov rdi, [r13 + 8]
    lea rsi, [rel dash_arg]
    call strcmp
    test eax, eax
    jnz usage
    mov rdi, [r13 + 16]
    lea rsi, [rel a_arg]
    call strcmp
    test eax, eax
    jnz usage

.enable_all:
    mov r14d, 1

.read_uname:
    mov rax, SYS_UNAME
    lea rdi, [rel uts_buf]
    syscall
    test rax, rax
    js fail

    test r14d, r14d
    jnz print_all

    lea rsi, [rel uts_buf + UTS_SYSNAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    jmp done

print_all:
    lea rsi, [rel uts_buf + UTS_SYSNAME_OFF]
    call write_cstring_stdout
    call write_space
    lea rsi, [rel uts_buf + UTS_NODENAME_OFF]
    call write_cstring_stdout
    call write_space
    lea rsi, [rel uts_buf + UTS_RELEASE_OFF]
    call write_cstring_stdout
    call write_space
    lea rsi, [rel uts_buf + UTS_VERSION_OFF]
    call write_cstring_stdout
    call write_space
    lea rsi, [rel uts_buf + UTS_MACHINE_OFF]
    call write_cstring_stdout
    call write_space
    lea rsi, [rel uts_buf + UTS_DOMAIN_OFF]
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

fail:
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

write_space:
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
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

strcmp:
    xor eax, eax
.loop:
    mov dl, [rdi]
    mov cl, [rsi]
    cmp dl, cl
    jne .diff
    test dl, dl
    jz .equal
    inc rdi
    inc rsi
    jmp .loop
.diff:
    mov eax, 1
.equal:
    ret

section .bss
uts_buf:
    resb UTS_SIZE

section .rodata
all_arg:
    db "-a", 0

long_all_arg:
    db "--all", 0

dash_arg:
    db "-", 0

a_arg:
    db "a", 0

space_msg:
    db " "
space_msg_end:

newline_msg:
    db 10
newline_msg_end:

usage_msg:
    db "usage: uname [-a]", 10
usage_msg_end:

fail_msg:
    db "uname: failed to read system name", 10
fail_msg_end:
