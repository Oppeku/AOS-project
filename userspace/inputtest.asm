; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_INPUT_POLL 525

%define EV_KEY 0
%define EV_FLAGS 4
%define EV_ASCII 8
%define EV_SOURCE 12

_start:
    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    lea rsi, [rel prompt_msg]
    mov rdx, prompt_msg_end - prompt_msg
    call write_stdout

.poll:
    mov rax, AOS_SYS_INPUT_POLL
    lea rdi, [rel input_event]
    syscall
    cmp rax, 0
    jle .poll

    lea rsi, [rel key_msg]
    mov rdx, key_msg_end - key_msg
    call write_stdout
    mov edi, [rel input_event + EV_KEY]
    call write_hex32_line

    lea rsi, [rel ascii_msg]
    mov rdx, ascii_msg_end - ascii_msg
    call write_stdout
    mov edi, [rel input_event + EV_ASCII]
    call write_hex32_line

    lea rsi, [rel source_msg]
    mov rdx, source_msg_end - source_msg
    call write_stdout
    mov edi, [rel input_event + EV_SOURCE]
    call write_hex32_line

    lea rsi, [rel flags_msg]
    mov rdx, flags_msg_end - flags_msg
    call write_stdout
    mov edi, [rel input_event + EV_FLAGS]
    call write_hex32_line

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

write_hex32_line:
    push rdi
    lea rsi, [rel hex_prefix]
    mov rdx, hex_prefix_end - hex_prefix
    call write_stdout
    pop rdi
    call write_hex32
    lea rsi, [rel newline_msg]
    mov rdx, 1
    jmp write_stdout

write_hex32:
    push rdi
    shr edi, 16
    call write_hex16
    pop rdi
    call write_hex16
    ret

write_hex16:
    push rdi
    shr edi, 8
    call write_hex8
    pop rdi
    call write_hex8
    ret

write_hex8:
    push rdi
    shr edi, 4
    call write_hex_nibble
    pop rdi
    call write_hex_nibble
    ret

write_hex_nibble:
    and edi, 0xF
    lea rsi, [rel hex_digits]
    mov al, [rsi + rdi]
    mov [rel one_char], al
    lea rsi, [rel one_char]
    mov rdx, 1
    jmp write_stdout

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
input_event:
    resb 16
one_char:
    resb 1

section .rodata
title_msg:
    db "AOS inputtest", 10
title_msg_end:
prompt_msg:
    db "Press one key...", 10
prompt_msg_end:
key_msg:
    db "key: "
key_msg_end:
ascii_msg:
    db "ascii: "
ascii_msg_end:
source_msg:
    db "source: "
source_msg_end:
flags_msg:
    db "flags: "
flags_msg_end:
hex_prefix:
    db "0x"
hex_prefix_end:
newline_msg:
    db 10
hex_digits:
    db "0123456789abcdef"
