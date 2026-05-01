; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_FSTAT 5
%define SYS_LSEEK 8
%define SYS_EXIT 60
%define SYS_OPENAT 257
%define SYS_NEWFSTATAT 262
%define AT_FDCWD -100
%define SEEK_SET 0
%define SEEK_END 2
%define STAT_SIZE_OFFSET 48

_start:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel file_path]
    xor rdx, rdx
    xor r10, r10
    syscall

    test rax, rax
    js .open_failed

    mov r12, rax

    mov rax, SYS_FSTAT
    mov rdi, r12
    lea rsi, [rel statbuf]
    syscall
    test rax, rax
    js .fstat_failed

    mov r14, [rel statbuf + STAT_SIZE_OFFSET]

    mov rax, SYS_NEWFSTATAT
    mov rdi, AT_FDCWD
    lea rsi, [rel file_path]
    lea rdx, [rel path_statbuf]
    xor r10, r10
    syscall
    test rax, rax
    js .newfstatat_failed

    cmp r14, [rel path_statbuf + STAT_SIZE_OFFSET]
    jne .size_mismatch

    mov rax, SYS_LSEEK
    mov rdi, r12
    xor rsi, rsi
    mov rdx, SEEK_END
    syscall
    test rax, rax
    js .lseek_failed

    cmp rax, r14
    jne .size_mismatch

    mov rax, SYS_LSEEK
    mov rdi, r12
    xor rsi, rsi
    mov rdx, SEEK_SET
    syscall
    test rax, rax
    js .lseek_failed

.read_loop:
    mov rax, SYS_READ
    mov rdi, r12
    lea rsi, [rel buffer]
    mov rdx, buffer_end - buffer
    syscall

    test rax, rax
    jle .close_file

    mov r13, rax
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel buffer]
    mov rdx, r13
    syscall
    jmp .read_loop

.close_file:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.open_failed:
    lea rsi, [rel open_failed_msg]
    mov rdx, open_failed_msg_end - open_failed_msg
    jmp .write_failure

.fstat_failed:
    lea rsi, [rel fstat_failed_msg]
    mov rdx, fstat_failed_msg_end - fstat_failed_msg
    jmp .write_failure

.newfstatat_failed:
    lea rsi, [rel newfstatat_failed_msg]
    mov rdx, newfstatat_failed_msg_end - newfstatat_failed_msg
    jmp .write_failure

.lseek_failed:
    lea rsi, [rel lseek_failed_msg]
    mov rdx, lseek_failed_msg_end - lseek_failed_msg
    jmp .write_failure

.size_mismatch:
    lea rsi, [rel size_mismatch_msg]
    mov rdx, size_mismatch_msg_end - size_mismatch_msg

.write_failure:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall

    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

section .bss
buffer:
    resb 64
buffer_end:
statbuf:
    resb 144
path_statbuf:
    resb 144

section .rodata
file_path:
    db "hello.txt", 0

open_failed_msg:
    db "filetest: open failed", 10
open_failed_msg_end:
fstat_failed_msg:
    db "filetest: fstat failed", 10
fstat_failed_msg_end:
newfstatat_failed_msg:
    db "filetest: newfstatat failed", 10
newfstatat_failed_msg_end:
lseek_failed_msg:
    db "filetest: lseek failed", 10
lseek_failed_msg_end:
size_mismatch_msg:
    db "filetest: size mismatch", 10
size_mismatch_msg_end:
