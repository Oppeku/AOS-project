; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define MAX_ARGC_DIGITS 20

_start:
    mov r12, [rsp]          ; argc
    lea r13, [rsp + 8]      ; argv

    lea rsi, [rel argc_prefix]
    mov rdx, argc_prefix_end - argc_prefix
    call write_stdout

    mov rdi, r12
    call write_u64_decimal

    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    xor r14, r14

argv_loop:
    cmp r14, r12
    jae argv_done

    lea rsi, [rel argv_prefix]
    mov rdx, argv_prefix_end - argv_prefix
    call write_stdout

    mov rdi, r14
    call write_u64_decimal

    lea rsi, [rel argv_mid]
    mov rdx, argv_mid_end - argv_mid
    call write_stdout

    mov rsi, [r13 + r14 * 8]
    call write_cstring_stdout

    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    inc r14
    jmp argv_loop

argv_done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

hang:
    jmp hang

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

write_cstring_stdout:
    xor rdx, rdx

count_cstring:
    cmp byte [rsi + rdx], 0
    je emit_cstring
    inc rdx
    jmp count_cstring

emit_cstring:
    jmp write_stdout

write_u64_decimal:
    lea rbx, [rel decimal_buffer + MAX_ARGC_DIGITS]
    mov byte [rbx], 0
    mov rax, rdi

    test rax, rax
    jnz decimal_convert

    dec rbx
    mov byte [rbx], '0'
    jmp decimal_emit

decimal_convert:
    mov rcx, 10

decimal_loop:
    xor rdx, rdx
    div rcx
    dec rbx
    add dl, '0'
    mov [rbx], dl
    test rax, rax
    jnz decimal_loop

decimal_emit:
    mov rsi, rbx
    call write_cstring_stdout
    ret

section .bss
decimal_buffer:
    resb MAX_ARGC_DIGITS + 1

section .rodata
argc_prefix:
    db "argc="
argc_prefix_end:

argv_prefix:
    db "argv["
argv_prefix_end:

argv_mid:
    db "]="
argv_mid_end:

newline_msg:
    db 10
newline_msg_end:
