; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_MOUNT_INFO 506

%define MOUNT_INFO_SIZE 136
%define MOUNT_BACKEND_OFF 0
%define MOUNT_PATH_OFF 8
%define MOUNT_ROOT_OFF 72

_start:
    lea rsi, [rel header_msg]
    mov rdx, header_msg_end - header_msg
    call write_stdout

    xor r12, r12

mount_loop:
    mov rax, AOS_SYS_MOUNT_INFO
    mov rdi, r12
    lea rsi, [rel mount_buf]
    syscall
    test rax, rax
    js done

    lea rsi, [rel mount_buf + MOUNT_PATH_OFF]
    call write_path_or_root
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    movzx edi, byte [rel mount_buf + MOUNT_BACKEND_OFF]
    call write_backend_name
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    lea rsi, [rel mount_buf + MOUNT_ROOT_OFF]
    call write_path_or_root
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    inc r12
    jmp mount_loop

done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

write_backend_name:
    cmp edi, 1
    je .synthetic
    cmp edi, 2
    je .initrd
    cmp edi, 3
    je .fat32
    cmp edi, 4
    je .tmpfs
    cmp edi, 5
    je .ext4
    cmp edi, 6
    je .aosfs
    lea rsi, [rel unknown_msg]
    mov rdx, unknown_msg_end - unknown_msg
    jmp write_stdout
.synthetic:
    lea rsi, [rel synthetic_msg]
    mov rdx, synthetic_msg_end - synthetic_msg
    jmp write_stdout
.initrd:
    lea rsi, [rel initrd_msg]
    mov rdx, initrd_msg_end - initrd_msg
    jmp write_stdout
.fat32:
    lea rsi, [rel fat32_msg]
    mov rdx, fat32_msg_end - fat32_msg
    jmp write_stdout
.tmpfs:
    lea rsi, [rel tmpfs_msg]
    mov rdx, tmpfs_msg_end - tmpfs_msg
    jmp write_stdout
.ext4:
    lea rsi, [rel ext4_msg]
    mov rdx, ext4_msg_end - ext4_msg
    jmp write_stdout
.aosfs:
    lea rsi, [rel aosfs_msg]
    mov rdx, aosfs_msg_end - aosfs_msg
    jmp write_stdout

write_path_or_root:
    cmp byte [rsi], 0
    jne write_cstring_stdout
    lea rsi, [rel slash_msg]
    mov rdx, slash_msg_end - slash_msg
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
mount_buf:
    resb MOUNT_INFO_SIZE

section .rodata
header_msg:
    db "TARGET        BACKEND    SOURCE", 10
    db "-------------------------------", 10
header_msg_end:

gap_msg:
    db "        "
gap_msg_end:

newline_msg:
    db 10
newline_msg_end:

slash_msg:
    db "/"
slash_msg_end:

synthetic_msg:
    db "synthetic"
synthetic_msg_end:

initrd_msg:
    db "initrd"
initrd_msg_end:

fat32_msg:
    db "fat32"
fat32_msg_end:

tmpfs_msg:
    db "tmpfs"
tmpfs_msg_end:

ext4_msg:
    db "ext4"
ext4_msg_end:

aosfs_msg:
    db "aosfs"
aosfs_msg_end:

unknown_msg:
    db "unknown"
unknown_msg_end:
