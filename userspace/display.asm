; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

bits 64
default rel
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_DISPLAY_INFO 511
%define AOS_SYS_DISPLAY_SET 512

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]

    cmp r12, 2
    jb show_info

    mov rdi, [r13 + 8]
    lea rsi, [rel auto_arg]
    call streq
    test rax, rax
    jnz set_auto

    mov rdi, [r13 + 8]
    lea rsi, [rel set_arg]
    call streq
    test rax, rax
    jz usage

    cmp r12, 4
    jb usage

    mov rdi, [r13 + 16]
    call parse_u32
    test rdx, rdx
    jz usage
    mov r14, rax

    mov rdi, [r13 + 24]
    call parse_u32
    test rdx, rdx
    jz usage
    mov r15, rax

    mov rax, AOS_SYS_DISPLAY_SET
    mov rdi, r14
    mov rsi, r15
    syscall
    test rax, rax
    js set_failed

    lea rsi, [rel set_ok_msg]
    mov rdx, set_ok_msg_end - set_ok_msg
    call write_stdout
    jmp show_info

set_auto:
    mov rax, AOS_SYS_DISPLAY_SET
    xor rdi, rdi
    xor rsi, rsi
    syscall
    test rax, rax
    js set_failed

    lea rsi, [rel auto_ok_msg]
    mov rdx, auto_ok_msg_end - auto_ok_msg
    call write_stdout
    jmp show_info

show_info:
    mov rax, AOS_SYS_DISPLAY_INFO
    lea rdi, [rel display_info]
    syscall
    test rax, rax
    js info_failed

    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    lea rsi, [rel detected_msg]
    mov rdx, detected_msg_end - detected_msg
    call write_stdout
    mov edi, [rel display_info + 8]
    call print_u64
    lea rsi, [rel x_msg]
    mov rdx, x_msg_end - x_msg
    call write_stdout
    mov edi, [rel display_info + 12]
    call print_u64
    call newline

    lea rsi, [rel current_msg]
    mov rdx, current_msg_end - current_msg
    call write_stdout
    mov edi, [rel display_info]
    call print_u64
    lea rsi, [rel x_msg]
    mov rdx, x_msg_end - x_msg
    call write_stdout
    mov edi, [rel display_info + 4]
    call print_u64
    call newline

    lea rsi, [rel max_msg]
    mov rdx, max_msg_end - max_msg
    call write_stdout
    mov edi, [rel display_info + 16]
    call print_u64
    lea rsi, [rel x_msg]
    mov rdx, x_msg_end - x_msg
    call write_stdout
    mov edi, [rel display_info + 20]
    call print_u64
    call newline

    lea rsi, [rel note_msg]
    mov rdx, note_msg_end - note_msg
    call write_stdout
    xor rdi, rdi
    jmp exit

set_failed:
    lea rsi, [rel set_failed_msg]
    mov rdx, set_failed_msg_end - set_failed_msg
    call write_stdout
    mov rdi, 1
    jmp exit

info_failed:
    lea rsi, [rel info_failed_msg]
    mov rdx, info_failed_msg_end - info_failed_msg
    call write_stdout
    mov rdi, 1
    jmp exit

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rdi, 1
    jmp exit

streq:
    mov al, [rdi]
    mov bl, [rsi]
    cmp al, bl
    jne .no
    test al, al
    jz .yes
    inc rdi
    inc rsi
    jmp streq
.yes:
    mov rax, 1
    ret
.no:
    xor rax, rax
    ret

parse_u32:
    xor rax, rax
    xor rdx, rdx
.loop:
    mov cl, [rdi]
    test cl, cl
    jz .done
    cmp cl, '0'
    jb .bad
    cmp cl, '9'
    ja .bad
    imul rax, rax, 10
    movzx rcx, cl
    sub rcx, '0'
    add rax, rcx
    mov rdx, 1
    inc rdi
    jmp .loop
.bad:
    xor rdx, rdx
.done:
    ret

print_u64:
    mov rax, rdi
    lea rsi, [rel num_buf_end]
    mov byte [rsi - 1], 0
    lea rbx, [rsi - 1]
    mov rcx, 10
    test rax, rax
    jnz .digits
    dec rbx
    mov byte [rbx], '0'
    jmp .emit
.digits:
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rbx
    mov [rbx], dl
    test rax, rax
    jnz .digits
.emit:
    mov rsi, rbx
    lea rdx, [rel num_buf_end - 1]
    sub rdx, rbx
    jmp write_stdout

newline:
    lea rsi, [rel newline_msg]
    mov rdx, 1
    jmp write_stdout

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

exit:
    mov rax, SYS_EXIT
    syscall

section .bss
display_info:
    resb 24
num_buf:
    resb 32
num_buf_end:

section .rodata
auto_arg:
    db "auto", 0
set_arg:
    db "set", 0
title_msg:
    db "AOS display", 10
title_msg_end:
detected_msg:
    db "detected: "
detected_msg_end:
current_msg:
    db "current:  "
current_msg_end:
max_msg:
    db "max:      "
max_msg_end:
x_msg:
    db "x"
x_msg_end:
note_msg:
    db "editable: display set <cols> <rows>, display auto", 10
note_msg_end:
set_ok_msg:
    db "display: mode updated", 10
set_ok_msg_end:
auto_ok_msg:
    db "display: auto mode restored", 10
auto_ok_msg_end:
usage_msg:
    db "usage: display [auto|set <cols> <rows>]", 10
usage_msg_end:
set_failed_msg:
    db "display: invalid mode; VGA text supports 40x10 through 80x25", 10
set_failed_msg_end:
info_failed_msg:
    db "display: failed to read display info", 10
info_failed_msg_end:
newline_msg:
    db 10
