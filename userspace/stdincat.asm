; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_EXIT 60
%define IO_BUF_SIZE 512

_start:
stdincat_read_loop:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel io_buffer]
    mov rdx, IO_BUF_SIZE
    syscall

    test rax, rax
    jle stdincat_done

    mov rdx, rax
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel io_buffer]
    syscall
    jmp stdincat_read_loop

stdincat_done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

stdincat_spin:
    jmp stdincat_spin

section .bss
io_buffer:
    resb IO_BUF_SIZE
