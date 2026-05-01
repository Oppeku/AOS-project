; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

_start:
    ; Stage 1 banner.
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel welcome_msg]
    mov rdx, welcome_msg_end - welcome_msg
    syscall

    ; Call fork() - Syscall 57
    mov rax, 57
    syscall

    test rax, rax
    jz child_process

parent_process:
    ; Parent loop
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel parent_msg]
    mov rdx, parent_msg_end - parent_msg
    syscall

    ; Busy wait to see the switch
    mov ecx, 0x1FFFFFF
.wait:
    loop .wait
    jmp parent_process

child_process:
    ; Child loop
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel child_msg]
    mov rdx, child_msg_end - child_msg
    syscall

    ; Busy wait to see the switch
    mov ecx, 0x1FFFFFF
.wait:
    loop .wait
    jmp child_process

section .rodata
welcome_msg:
    db "AOS: Multitasking Test Started", 10
welcome_msg_end:

parent_msg:
    db "P", 0
parent_msg_end:

child_msg:
    db "C", 0
child_msg_end:
