; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_UPTIME_INFO 510
%define MAX_DEC_DIGITS 20

%define UPTIME_TICKS_OFF 0
%define UPTIME_SECONDS_OFF 8

_start:
    mov rax, AOS_SYS_UPTIME_INFO
    lea rdi, [rel uptime_info]
    syscall
    test rax, rax
    js fail

    lea rsi, [rel uptime_prefix]
    mov rdx, uptime_prefix_end - uptime_prefix
    call write_stdout

    mov rax, [rel uptime_info + UPTIME_SECONDS_OFF]
    xor rdx, rdx
    mov rcx, 3600
    div rcx
    mov r12, rax             ; hours
    mov rax, rdx
    xor rdx, rdx
    mov rcx, 60
    div rcx
    mov r13, rax             ; minutes
    mov r14, rdx             ; seconds

    mov rdi, r12
    call write_two_digits
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    mov rdi, r13
    call write_two_digits
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    mov rdi, r14
    call write_two_digits
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel ticks_prefix]
    mov rdx, ticks_prefix_end - ticks_prefix
    call write_stdout
    mov rdi, [rel uptime_info + UPTIME_TICKS_OFF]
    call write_u64_decimal
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

fail:
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

write_two_digits:
    cmp rdi, 100
    jb .two
    call write_u64_decimal
    ret
.two:
    mov rax, rdi
    xor rdx, rdx
    mov rcx, 10
    div rcx
    add al, '0'
    add dl, '0'
    mov [rel two_digit_buf], al
    mov [rel two_digit_buf + 1], dl
    lea rsi, [rel two_digit_buf]
    mov rdx, 2
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
    lea rbx, [rel decimal_buffer + MAX_DEC_DIGITS]
    mov byte [rbx], 0
    mov rax, rdi

    test rax, rax
    jnz .convert

    dec rbx
    mov byte [rbx], '0'
    jmp .emit

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

.emit:
    mov rsi, rbx
    call write_cstring_stdout
    ret

section .bss
uptime_info:
    resb 24
decimal_buffer:
    resb MAX_DEC_DIGITS + 1

section .data
two_digit_buf:
    db "00"

section .rodata
uptime_prefix:
    db "AOS uptime: "
uptime_prefix_end:

ticks_prefix:
    db "ticks: "
ticks_prefix_end:

colon_msg:
    db ":"
colon_msg_end:

newline_msg:
    db 10
newline_msg_end:

fail_msg:
    db "uptime: failed to read AOS uptime", 10
fail_msg_end:
