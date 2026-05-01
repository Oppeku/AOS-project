; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel stage2_message]
    mov rdx, stage2_message_end - stage2_message
    syscall

    mov rax, 60
    xor rdi, rdi
    syscall

section .rodata
stage2_message:
    db "Stage2: hello from execve payload", 10
stage2_message_end:
