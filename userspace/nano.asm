; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_EXIT 60
%define SYS_OPENAT 257

%define AT_FDCWD -100
%define O_RDONLY 0
%define O_WRONLY 1
%define O_CREAT 64
%define O_TRUNC 512
%define EDIT_BUF_SIZE 4096

_start:
    mov r12, [rsp]          ; argc
    lea r13, [rsp + 8]      ; argv
    cmp r12, 2
    jb usage

    mov r15, [r13 + 8]      ; argv[1]

    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    lea rsi, [rel file_msg]
    mov rdx, file_msg_end - file_msg
    call write_stdout
    mov rsi, r15
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    call show_existing_file

    lea rsi, [rel edit_msg]
    mov rdx, edit_msg_end - edit_msg
    call write_stdout

    xor r14, r14            ; edit buffer length
    xor rbx, rbx            ; current line start

read_input_loop:
    call read_char_blocking
    cmp al, 8
    je handle_backspace
    cmp al, 127
    je handle_backspace
    cmp al, 10
    je handle_newline
    cmp al, 13
    je handle_newline

    cmp r14, EDIT_BUF_SIZE
    jae input_full
    lea rdi, [rel edit_buffer]
    mov [rdi + r14], al
    inc r14
    mov [rel one_char], al
    lea rsi, [rel one_char]
    mov rdx, 1
    call write_stdout
    jmp read_input_loop

handle_backspace:
    cmp r14, rbx
    jbe read_input_loop
    dec r14
    lea rsi, [rel backspace_msg]
    mov rdx, backspace_msg_end - backspace_msg
    call write_stdout
    jmp read_input_loop

handle_newline:
    lea rdi, [rel edit_buffer]
    mov rcx, r14
    sub rcx, rbx
    cmp rcx, 1
    jne append_newline
    cmp byte [rdi + rbx], '.'
    jne append_newline

    mov r14, rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    jmp save_file

append_newline:
    cmp r14, EDIT_BUF_SIZE
    jae input_full
    mov byte [rdi + r14], 10
    inc r14
    mov rbx, r14
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    jmp read_input_loop

input_full:
    lea rsi, [rel full_msg]
    mov rdx, full_msg_end - full_msg
    call write_stdout
    jmp save_file

save_file:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, r15
    mov rdx, O_WRONLY | O_CREAT | O_TRUNC
    xor r10, r10
    syscall
    test rax, rax
    js save_failed

    mov r12, rax
    test r14, r14
    jz close_saved_file

    mov rax, SYS_WRITE
    mov rdi, r12
    lea rsi, [rel edit_buffer]
    mov rdx, r14
    syscall
    test rax, rax
    js save_failed_open

close_saved_file:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall

    lea rsi, [rel saved_msg]
    mov rdx, saved_msg_end - saved_msg
    call write_stdout
    xor rdi, rdi
    jmp exit_process

save_failed_open:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall

save_failed:
    lea rsi, [rel save_failed_msg]
    mov rdx, save_failed_msg_end - save_failed_msg
    call write_stdout
    mov rdi, 1
    jmp exit_process

show_existing_file:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, r15
    mov rdx, O_RDONLY
    xor r10, r10
    syscall
    test rax, rax
    js show_new_file

    mov r12, rax
    lea rsi, [rel existing_msg]
    mov rdx, existing_msg_end - existing_msg
    call write_stdout

show_read_loop:
    mov rax, SYS_READ
    mov rdi, r12
    lea rsi, [rel io_buffer]
    mov rdx, io_buffer_end - io_buffer
    syscall
    test rax, rax
    jle show_close

    mov rdx, rax
    lea rsi, [rel io_buffer]
    call write_stdout
    jmp show_read_loop

show_close:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall
    lea rsi, [rel existing_end_msg]
    mov rdx, existing_end_msg_end - existing_end_msg
    call write_stdout
    ret

show_new_file:
    lea rsi, [rel new_file_msg]
    mov rdx, new_file_msg_end - new_file_msg
    call write_stdout
    ret

read_char_blocking:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel one_char]
    mov rdx, 1
    syscall
    cmp rax, 1
    jne read_char_blocking
    mov al, [rel one_char]
    ret

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rdi, 1
    jmp exit_process

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

exit_process:
    mov rax, SYS_EXIT
    syscall

section .bss
edit_buffer:
    resb EDIT_BUF_SIZE
io_buffer:
    resb 256
io_buffer_end:
one_char:
    resb 1

section .rodata
usage_msg:
    db "usage: nano <file>", 10
usage_msg_end:
title_msg:
    db "AOS nano", 10
title_msg_end:
file_msg:
    db "file: "
file_msg_end:
existing_msg:
    db "--- existing content ---", 10
existing_msg_end:
existing_end_msg:
    db 10, "--- end existing content ---", 10
existing_end_msg_end:
new_file_msg:
    db "new file", 10
new_file_msg_end:
edit_msg:
    db "enter text. single . on a line saves.", 10
edit_msg_end:
saved_msg:
    db "nano: saved", 10
saved_msg_end:
save_failed_msg:
    db "nano: save failed", 10
save_failed_msg_end:
full_msg:
    db 10, "nano: buffer full, saving", 10
full_msg_end:
backspace_msg:
    db 8, " ", 8
backspace_msg_end:
newline_msg:
    db 10
newline_msg_end:
