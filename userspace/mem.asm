; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_MEM_INFO 509
%define MAX_DEC_DIGITS 20

%define MEM_TOTAL_OFF 0
%define MEM_FREE_OFF 8
%define MEM_USED_OFF 16

_start:
    mov r12, [rsp]          ; argc
    lea r13, [rsp + 8]      ; argv
    xor r14d, r14d          ; verbose flag

    cmp r12, 2
    jb .read_info

    mov rdi, [r13 + 8]
    lea rsi, [rel short_verbose_arg]
    call strcmp
    test eax, eax
    jz .enable_verbose

    mov rdi, [r13 + 8]
    lea rsi, [rel long_verbose_arg]
    call strcmp
    test eax, eax
    jnz usage

.enable_verbose:
    mov r14d, 1

.read_info:
    mov rax, AOS_SYS_MEM_INFO
    lea rdi, [rel mem_info]
    syscall
    test rax, rax
    js fail

    lea rsi, [rel header_msg]
    mov rdx, header_msg_end - header_msg
    call write_stdout

    lea rsi, [rel total_msg]
    mov rdx, total_msg_end - total_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_TOTAL_OFF]
    call write_mib

    lea rsi, [rel used_msg]
    mov rdx, used_msg_end - used_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_USED_OFF]
    call write_mib

    lea rsi, [rel free_msg]
    mov rdx, free_msg_end - free_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_FREE_OFF]
    call write_mib

    test r14d, r14d
    jz done

    lea rsi, [rel detail_header_msg]
    mov rdx, detail_header_msg_end - detail_header_msg
    call write_stdout

    lea rsi, [rel total_bytes_msg]
    mov rdx, total_bytes_msg_end - total_bytes_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_TOTAL_OFF]
    call write_u64_line

    lea rsi, [rel used_bytes_msg]
    mov rdx, used_bytes_msg_end - used_bytes_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_USED_OFF]
    call write_u64_line

    lea rsi, [rel free_bytes_msg]
    mov rdx, free_bytes_msg_end - free_bytes_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_FREE_OFF]
    call write_u64_line

    lea rsi, [rel total_pages_msg]
    mov rdx, total_pages_msg_end - total_pages_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_TOTAL_OFF]
    shr rdi, 12
    call write_u64_line

    lea rsi, [rel used_pages_msg]
    mov rdx, used_pages_msg_end - used_pages_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_USED_OFF]
    shr rdi, 12
    call write_u64_line

    lea rsi, [rel free_pages_msg]
    mov rdx, free_pages_msg_end - free_pages_msg
    call write_stdout
    mov rdi, [rel mem_info + MEM_FREE_OFF]
    shr rdi, 12
    call write_u64_line

    lea rsi, [rel used_percent_msg]
    mov rdx, used_percent_msg_end - used_percent_msg
    call write_stdout
    call write_used_percent

done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

fail:
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

write_mib:
    add rdi, 1048575
    shr rdi, 20
    call write_u64_decimal
    lea rsi, [rel mib_suffix]
    mov rdx, mib_suffix_end - mib_suffix
    jmp write_stdout

write_u64_line:
    call write_u64_decimal
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    jmp write_stdout

write_used_percent:
    mov rax, [rel mem_info + MEM_USED_OFF]
    mov rcx, [rel mem_info + MEM_TOTAL_OFF]
    test rcx, rcx
    jz .zero
    mov rbx, 100
    mul rbx
    div rcx
    mov rdi, rax
    call write_u64_decimal
    lea rsi, [rel percent_suffix]
    mov rdx, percent_suffix_end - percent_suffix
    jmp write_stdout
.zero:
    xor rdi, rdi
    call write_u64_decimal
    lea rsi, [rel percent_suffix]
    mov rdx, percent_suffix_end - percent_suffix
    jmp write_stdout

strcmp:
    xor eax, eax
.loop:
    mov dl, [rdi]
    mov cl, [rsi]
    cmp dl, cl
    jne .diff
    test dl, dl
    jz .equal
    inc rdi
    inc rsi
    jmp .loop
.diff:
    mov eax, 1
.equal:
    ret

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
mem_info:
    resb 24
decimal_buffer:
    resb MAX_DEC_DIGITS + 1

section .rodata
header_msg:
    db "AOS memory", 10
header_msg_end:

total_msg:
    db "total: "
total_msg_end:

used_msg:
    db "used:  "
used_msg_end:

free_msg:
    db "free:  "
free_msg_end:

detail_header_msg:
    db "details:", 10
detail_header_msg_end:

total_bytes_msg:
    db "total bytes: "
total_bytes_msg_end:

used_bytes_msg:
    db "used bytes:  "
used_bytes_msg_end:

free_bytes_msg:
    db "free bytes:  "
free_bytes_msg_end:

total_pages_msg:
    db "total pages: "
total_pages_msg_end:

used_pages_msg:
    db "used pages:  "
used_pages_msg_end:

free_pages_msg:
    db "free pages:  "
free_pages_msg_end:

used_percent_msg:
    db "used:        "
used_percent_msg_end:

mib_suffix:
    db " MiB", 10
mib_suffix_end:

percent_suffix:
    db "%", 10
percent_suffix_end:

newline_msg:
    db 10
newline_msg_end:

short_verbose_arg:
    db "-v", 0

long_verbose_arg:
    db "--verbose", 0

usage_msg:
    db "usage: mem [-v]", 10
usage_msg_end:

fail_msg:
    db "mem: failed to read AOS memory info", 10
fail_msg_end:
