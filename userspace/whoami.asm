; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_USER_INFO 516

_start:
    mov rax, AOS_SYS_USER_INFO
    lea rdi, [rel user_info]
    syscall
    test rax, rax
    js .failed

    lea rsi, [rel user_info + 16]
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

.done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.failed:
    lea rsi, [rel unknown_name]
    mov rdx, unknown_name_end - unknown_name
    call write_stdout
    jmp .done

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

section .bss
user_info:
    resb 304

section .rodata
unknown_name:
    db "unknown", 10
unknown_name_end:

newline_msg:
    db 10
newline_msg_end:
