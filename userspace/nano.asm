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
%define AOS_SYS_DISPLAY_INFO 511

%define AT_FDCWD -100
%define O_RDONLY 0
%define O_WRONLY 1
%define O_CREAT 64
%define O_TRUNC 512
%define EDIT_BUF_SIZE 4096
%define CUT_BUF_SIZE 256
%define SCREEN_COLS 80
%define EDIT_COLS 79
%define EDIT_ROWS 20

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]
    lea r15, [rel unnamed_file]
    cmp r12, 2
    jb .have_file
    mov r15, [r13 + 8]

.have_file:
    xor r14, r14
    xor rbx, rbx
    mov qword [rel cursor_row], 2
    mov qword [rel cursor_col], 1
    mov byte [rel modified_flag], 0
    call configure_display
    call load_file
    call recompute_line_start
    lea rsi, [rel opened_status]
    mov [rel status_ptr], rsi
    call draw_screen

editor_loop:
    call read_char_blocking

    cmp al, 24              ; ^X
    je exit_editor
    cmp al, 15              ; ^O
    je save_and_continue
    cmp al, 11              ; ^K
    je cut_line
    cmp al, 21              ; ^U
    je uncut_line
    cmp al, 7               ; ^G
    je show_help_status
    cmp al, 8
    je handle_backspace
    cmp al, 127
    je handle_backspace
    cmp al, 10
    je insert_newline
    cmp al, 13
    je insert_newline
    cmp al, 32
    jb editor_loop

    call insert_char_al
    jmp editor_loop

show_help_status:
    lea rsi, [rel help_status]
    mov [rel status_ptr], rsi
    call draw_screen
    jmp editor_loop

save_and_continue:
    call save_file
    call draw_screen
    jmp editor_loop

exit_editor:
    lea rsi, [rel clear_msg]
    mov rdx, clear_msg_end - clear_msg
    call write_stdout
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

insert_newline:
    mov al, 10
    call insert_char_al
    mov rbx, r14
    jmp editor_loop

insert_char_al:
    cmp r14, EDIT_BUF_SIZE
    jae .full
    lea rdi, [rel edit_buffer]
    mov [rdi + r14], al
    inc r14
    mov byte [rel modified_flag], 1
    lea rsi, [rel modified_status]
    mov [rel status_ptr], rsi
    cmp al, 10
    je .draw_newline
    mov [rel one_char], al
    lea rsi, [rel one_char]
    mov rdx, 1
    call write_stdout
    inc qword [rel cursor_col]
    mov rax, [rel edit_cols]
    cmp qword [rel cursor_col], rax
    jbe .done
.draw_newline:
    call write_newline
    inc qword [rel cursor_row]
    mov qword [rel cursor_col], 1
    mov rax, [rel edit_last_row]
    cmp qword [rel cursor_row], rax
    jbe .done
    call draw_screen
.done:
    ret
.full:
    lea rsi, [rel full_status]
    mov [rel status_ptr], rsi
    call draw_screen
    ret

handle_backspace:
    test r14, r14
    jz editor_loop
    dec r14
    mov byte [rel modified_flag], 1
    call recompute_line_start
    lea rsi, [rel modified_status]
    mov [rel status_ptr], rsi
    lea rsi, [rel backspace_msg]
    mov rdx, backspace_msg_end - backspace_msg
    call write_stdout
    cmp qword [rel cursor_col], 1
    jbe .done
    dec qword [rel cursor_col]
.done:
    jmp editor_loop

cut_line:
    cmp r14, rbx
    jbe editor_loop
    mov rcx, r14
    sub rcx, rbx
    cmp rcx, CUT_BUF_SIZE
    jbe .copy_len_ok
    mov rcx, CUT_BUF_SIZE
.copy_len_ok:
    mov [rel cut_len], rcx
    lea rsi, [rel edit_buffer]
    add rsi, rbx
    lea rdi, [rel cut_buffer]
.copy_loop:
    test rcx, rcx
    jz .cut_done
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jmp .copy_loop
.cut_done:
    mov r14, rbx
    mov byte [rel modified_flag], 1
    lea rsi, [rel cut_status]
    mov [rel status_ptr], rsi
    call draw_screen
    jmp editor_loop

uncut_line:
    mov rcx, [rel cut_len]
    test rcx, rcx
    jz editor_loop
    lea rsi, [rel cut_buffer]
.paste_loop:
    test rcx, rcx
    jz .paste_done
    cmp r14, EDIT_BUF_SIZE
    jae .paste_done
    mov al, [rsi]
    lea rdi, [rel edit_buffer]
    mov [rdi + r14], al
    inc r14
    inc rsi
    dec rcx
    jmp .paste_loop
.paste_done:
    mov byte [rel modified_flag], 1
    call recompute_line_start
    lea rsi, [rel paste_status]
    mov [rel status_ptr], rsi
    call draw_screen
    jmp editor_loop

recompute_line_start:
    xor rbx, rbx
    test r14, r14
    jz .done
    lea rsi, [rel edit_buffer]
    mov rcx, r14
.scan:
    cmp byte [rsi], 10
    jne .next
    mov rbx, r14
    sub rbx, rcx
    inc rbx
.next:
    inc rsi
    dec rcx
    jnz .scan
.done:
    ret

load_file:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, r15
    mov rdx, O_RDONLY
    xor r10, r10
    syscall
    test rax, rax
    js .new_file

    mov r12, rax
.read_loop:
    mov rax, SYS_READ
    mov rdi, r12
    lea rsi, [rel edit_buffer]
    add rsi, r14
    mov rdx, EDIT_BUF_SIZE
    sub rdx, r14
    syscall
    test rax, rax
    jle .close
    add r14, rax
    cmp r14, EDIT_BUF_SIZE
    jb .read_loop
.close:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall
    ret
.new_file:
    ret

save_file:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, r15
    mov rdx, O_WRONLY | O_CREAT | O_TRUNC
    xor r10, r10
    syscall
    test rax, rax
    js .failed

    mov r12, rax
    test r14, r14
    jz .close_saved
    mov rax, SYS_WRITE
    mov rdi, r12
    lea rsi, [rel edit_buffer]
    mov rdx, r14
    syscall
    test rax, rax
    js .failed_open

.close_saved:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall
    mov byte [rel modified_flag], 0
    lea rsi, [rel saved_status]
    mov [rel status_ptr], rsi
    ret

.failed_open:
    mov rax, SYS_CLOSE
    mov rdi, r12
    syscall
.failed:
    lea rsi, [rel failed_status]
    mov [rel status_ptr], rsi
    ret

draw_screen:
    lea rsi, [rel clear_msg]
    mov rdx, clear_msg_end - clear_msg
    call write_stdout

    lea rsi, [rel cursor_title_msg]
    mov rdx, cursor_title_msg_end - cursor_title_msg
    call write_stdout
    lea rsi, [rel inverse_msg]
    mov rdx, inverse_msg_end - inverse_msg
    call write_stdout
    lea rsi, [rel title_left]
    mov rdx, title_left_end - title_left
    call write_stdout
    mov rsi, r15
    call write_cstring_stdout
    cmp byte [rel modified_flag], 0
    je .title_pad
    lea rsi, [rel title_modified]
    mov rdx, title_modified_end - title_modified
    call write_stdout
.title_pad:
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    lea rsi, [rel normal_msg]
    mov rdx, normal_msg_end - normal_msg
    call write_stdout
    call write_newline

    call draw_buffer_rows

    mov rdi, [rel status_row]
    call write_cursor_row
    lea rsi, [rel inverse_msg]
    mov rdx, inverse_msg_end - inverse_msg
    call write_stdout
    mov rsi, [rel status_ptr]
    call write_cstring_stdout
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    lea rsi, [rel normal_msg]
    mov rdx, normal_msg_end - normal_msg
    call write_stdout

    mov rdi, [rel footer1_row]
    call write_cursor_row
    lea rsi, [rel footer1]
    mov rdx, footer1_end - footer1
    call write_stdout
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    mov rdi, [rel footer2_row]
    call write_cursor_row
    lea rsi, [rel footer2]
    mov rdx, footer2_end - footer2
    call write_stdout
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    lea rsi, [rel cursor_edit_msg]
    mov rdx, cursor_edit_msg_end - cursor_edit_msg
    call write_stdout
    mov qword [rel cursor_row], 2
    mov qword [rel cursor_col], 1
    ret

draw_buffer_rows:
    lea rsi, [rel edit_buffer]
    mov r8, r14              ; remaining bytes
    mov r9, [rel edit_rows]  ; rows

.row_loop:
    test r9, r9
    jz .done
    mov r10, [rel edit_cols]
.col_loop:
    test r10, r10
    jz .finish_row
    test r8, r8
    jz .finish_row
    mov al, [rsi]
    inc rsi
    dec r8
    cmp al, 10
    je .finish_row
    mov [rel one_char], al
    push rsi
    push r8
    push r9
    push r10
    lea rsi, [rel one_char]
    mov rdx, 1
    call write_stdout
    pop r10
    pop r9
    pop r8
    pop rsi
    dec r10
    jmp .col_loop
.finish_row:
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call write_newline
    dec r9
    jmp .row_loop
.done:
    ret

configure_display:
    mov qword [rel display_cols], SCREEN_COLS
    mov qword [rel display_rows], 25

    mov rax, AOS_SYS_DISPLAY_INFO
    lea rdi, [rel display_info]
    syscall
    test rax, rax
    js .derive

    mov eax, [rel display_info]
    cmp eax, 40
    jb .derive
    cmp eax, SCREEN_COLS
    ja .derive
    mov [rel display_cols], rax

    mov eax, [rel display_info + 4]
    cmp eax, 10
    jb .derive
    cmp eax, 25
    ja .derive
    mov [rel display_rows], rax

.derive:
    mov rax, [rel display_cols]
    dec rax
    mov [rel edit_cols], rax

    mov rax, [rel display_rows]
    sub rax, 5
    cmp rax, 5
    jae .rows_ok
    mov rax, 5
.rows_ok:
    mov [rel edit_rows], rax
    inc rax
    mov [rel edit_last_row], rax

    mov rax, [rel display_rows]
    sub rax, 3
    mov [rel status_row], rax
    inc rax
    mov [rel footer1_row], rax
    inc rax
    mov [rel footer2_row], rax
    ret

write_cursor_row:
    mov rax, rdi
    xor rdx, rdx
    mov rcx, 10
    div rcx
    add al, '0'
    add dl, '0'
    mov [rel cursor_row_msg + 2], al
    mov [rel cursor_row_msg + 3], dl
    lea rsi, [rel cursor_row_msg]
    mov rdx, cursor_row_msg_end - cursor_row_msg
    jmp write_stdout

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

write_spaces:
    test rcx, rcx
    jz .done
.loop:
    push rcx
    lea rsi, [rel space_msg]
    mov rdx, 1
    call write_stdout
    pop rcx
    dec rcx
    jnz .loop
.done:
    ret

write_newline:
    lea rsi, [rel newline_msg]
    mov rdx, 1
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

section .bss
edit_buffer:
    resb EDIT_BUF_SIZE
cut_buffer:
    resb CUT_BUF_SIZE
cut_len:
    resq 1
one_char:
    resb 1
modified_flag:
    resb 1
status_ptr:
    resq 1
cursor_row:
    resq 1
cursor_col:
    resq 1
display_cols:
    resq 1
display_rows:
    resq 1
edit_cols:
    resq 1
edit_rows:
    resq 1
edit_last_row:
    resq 1
status_row:
    resq 1
footer1_row:
    resq 1
footer2_row:
    resq 1
display_info:
    resb 24

section .rodata
unnamed_file:
    db "nano.txt", 0

clear_msg:
    db 27, "[2J", 27, "[H"
clear_msg_end:

cursor_title_msg:
    db 27, "[1;1H"
cursor_title_msg_end:

cursor_status_msg:
    db 27, "[22;1H"
cursor_status_msg_end:

cursor_footer1_msg:
    db 27, "[23;1H"
cursor_footer1_msg_end:

cursor_footer2_msg:
    db 27, "[24;1H"
cursor_footer2_msg_end:

cursor_edit_msg:
    db 27, "[2;1H"
cursor_edit_msg_end:

cursor_row_msg:
    db 27, "[00;1H"
cursor_row_msg_end:

inverse_msg:
    db 27, "[7m"
inverse_msg_end:

normal_msg:
    db 27, "[0m"
normal_msg_end:

clear_eol_msg:
    db 27, "[K"
clear_eol_msg_end:

title_left:
    db "  GNU nano 9.0  "
title_left_end:

title_modified:
    db "        Modified"
title_modified_end:

opened_status:
    db " Read 0 lines", 0

modified_status:
    db " Modified", 0

saved_status:
    db " Wrote file", 0

failed_status:
    db " Error writing file", 0

cut_status:
    db " Cut line", 0

paste_status:
    db " Uncut text", 0

help_status:
    db " ^O writes, ^X exits, ^K cuts, ^U pastes", 0

full_status:
    db " Buffer full", 0

footer1:
    db "^G Help      ^O Write Out ^W Where Is  ^K Cut       ^T Execute   ^C Location"
footer1_end:

footer2:
    db "^X Exit      ^R Read File ^\ Replace   ^U Paste     ^J Justify   ^/ Go To Line"
footer2_end:

space_msg:
    db " "

newline_msg:
    db 10

backspace_msg:
    db 8, " ", 8
backspace_msg_end:
