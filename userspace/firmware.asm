; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_FIRMWARE_INFO 529

%define FW_NAME_OFF 0
%define FW_SIZE_OFF 96

_start:
    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    xor r12, r12

.loop:
    mov rax, AOS_SYS_FIRMWARE_INFO
    mov rdi, r12
    lea rsi, [rel fw_buf]
    syscall
    test rax, rax
    js .done

    lea rsi, [rel fw_buf + FW_NAME_OFF]
    mov rdx, 96
    call write_cstr_max

    lea rsi, [rel size_msg]
    mov rdx, size_msg_end - size_msg
    call write_stdout
    mov edi, [rel fw_buf + FW_SIZE_OFF]
    call write_dec32
    lea rsi, [rel bytes_msg]
    mov rdx, bytes_msg_end - bytes_msg
    call write_stdout

    inc r12
    jmp .loop

.done:
    test r12, r12
    jnz .exit
    lea rsi, [rel empty_msg]
    mov rdx, empty_msg_end - empty_msg
    call write_stdout

.exit:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

write_dec32:
    lea rsi, [rel dec_buf + 15]
    mov byte [rsi], 0
    mov eax, edi
    mov ebx, 10
    test eax, eax
    jnz .digits
    dec rsi
    mov byte [rsi], '0'
    jmp .emit
.digits:
    xor edx, edx
    div ebx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test eax, eax
    jnz .digits
.emit:
    lea rdx, [rel dec_buf + 15]
    sub rdx, rsi
    jmp write_stdout

write_cstr_max:
    push rsi
    call cstrnlen
    mov rdx, rax
    pop rsi
    jmp write_stdout

cstrnlen:
    xor rax, rax
.len_loop:
    cmp rax, rdx
    jae .len_done
    cmp byte [rsi + rax], 0
    je .len_done
    inc rax
    jmp .len_loop
.len_done:
    ret

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
fw_buf:
    resb 104
dec_buf:
    resb 16

section .rodata
title_msg:
    db "AOS firmware", 10
    db "------------", 10
title_msg_end:

empty_msg:
    db "firmware: no blobs bundled in initrd", 10
empty_msg_end:

size_msg:
    db "  size="
size_msg_end:

bytes_msg:
    db " bytes", 10
bytes_msg_end:
