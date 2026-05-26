; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_TIME_INFO 515

%define TIME_YEAR_OFF 0
%define TIME_MONTH_OFF 2
%define TIME_DAY_OFF 3
%define TIME_HOUR_OFF 4
%define TIME_MINUTE_OFF 5
%define TIME_SECOND_OFF 6
%define TIME_WEEKDAY_OFF 7

_start:
    mov rax, AOS_SYS_TIME_INFO
    lea rdi, [rel time_info]
    syscall
    test rax, rax
    js fail

    movzx rdi, byte [rel time_info + TIME_WEEKDAY_OFF]
    call write_weekday
    call write_space

    movzx rdi, byte [rel time_info + TIME_MONTH_OFF]
    call write_month
    call write_space

    movzx rdi, byte [rel time_info + TIME_DAY_OFF]
    call write_two_digits
    call write_space

    movzx rdi, byte [rel time_info + TIME_HOUR_OFF]
    call write_two_digits
    call write_colon

    movzx rdi, byte [rel time_info + TIME_MINUTE_OFF]
    call write_two_digits
    call write_colon

    movzx rdi, byte [rel time_info + TIME_SECOND_OFF]
    call write_two_digits
    call write_space

    movzx rdi, word [rel time_info + TIME_YEAR_OFF]
    call write_u64_decimal
    call write_newline

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

write_weekday:
    cmp rdi, 1
    jb .unknown
    cmp rdi, 7
    ja .unknown
    dec rdi
    lea rax, [rel weekday_table]
    mov rsi, [rax + rdi * 8]
    mov rdx, 3
    jmp write_stdout
.unknown:
    lea rsi, [rel unknown_weekday]
    mov rdx, 3
    jmp write_stdout

write_month:
    cmp rdi, 1
    jb .unknown
    cmp rdi, 12
    ja .unknown
    dec rdi
    lea rax, [rel month_table]
    mov rsi, [rax + rdi * 8]
    mov rdx, 3
    jmp write_stdout
.unknown:
    lea rsi, [rel unknown_month]
    mov rdx, 3
    jmp write_stdout

write_two_digits:
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

write_u64_decimal:
    lea rbx, [rel decimal_buffer + 20]
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
    lea rdx, [rel decimal_buffer + 20]
    sub rdx, rbx
    jmp write_stdout

write_space:
    lea rsi, [rel space_msg]
    mov rdx, 1
    jmp write_stdout

write_colon:
    lea rsi, [rel colon_msg]
    mov rdx, 1
    jmp write_stdout

write_newline:
    lea rsi, [rel newline_msg]
    mov rdx, 1
    jmp write_stdout

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
time_info:
    resb 10
two_digit_buf:
    resb 2
decimal_buffer:
    resb 21

section .rodata
weekday_table:
    dq weekday_sun, weekday_mon, weekday_tue, weekday_wed, weekday_thu, weekday_fri, weekday_sat
month_table:
    dq month_jan, month_feb, month_mar, month_apr, month_may, month_jun
    dq month_jul, month_aug, month_sep, month_oct, month_nov, month_dec

weekday_sun: db "Sun"
weekday_mon: db "Mon"
weekday_tue: db "Tue"
weekday_wed: db "Wed"
weekday_thu: db "Thu"
weekday_fri: db "Fri"
weekday_sat: db "Sat"
unknown_weekday: db "???"

month_jan: db "Jan"
month_feb: db "Feb"
month_mar: db "Mar"
month_apr: db "Apr"
month_may: db "May"
month_jun: db "Jun"
month_jul: db "Jul"
month_aug: db "Aug"
month_sep: db "Sep"
month_oct: db "Oct"
month_nov: db "Nov"
month_dec: db "Dec"
unknown_month: db "???"

space_msg: db " "
colon_msg: db ":"
newline_msg: db 10
fail_msg:
    db "date: failed to read RTC time", 10
fail_msg_end:
