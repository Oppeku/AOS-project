; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_EXIT 60
%define SYS_GETCWD 79
%define SYS_CHDIR 80
%define SYS_OPENAT 257
%define AT_FDCWD -100

%macro WRITE_LITERAL 2
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel %1]
    mov rdx, %2 - %1
    syscall
%endmacro

%macro CHECK_OPEN 2
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel %1]
    xor rdx, rdx
    xor r10, r10
    syscall
    test rax, rax
    js %%failed

    mov rdi, rax
    mov rax, SYS_CLOSE
    syscall

    WRITE_LITERAL %2, %2 %+ _end
    jmp %%done

%%failed:
    WRITE_LITERAL %2, %2 %+ _end
    WRITE_LITERAL fail_suffix, fail_suffix_end
    jmp fail_exit

%%done:
%endmacro

%macro CHECK_CHDIR 2
    mov rax, SYS_CHDIR
    lea rdi, [rel %1]
    syscall
    test rax, rax
    js %%failed

    WRITE_LITERAL %2, %2 %+ _end
    jmp %%done

%%failed:
    WRITE_LITERAL %2, %2 %+ _end
    WRITE_LITERAL fail_suffix, fail_suffix_end
    jmp fail_exit

%%done:
%endmacro

%macro CHECK_GETCWD 2
    mov rax, SYS_GETCWD
    lea rdi, [rel cwd_buf]
    mov rsi, cwd_buf_end - cwd_buf
    syscall
    test rax, rax
    js %%failed

    lea rdi, [rel cwd_buf]
    lea rsi, [rel %1]
    call strcmp
    test eax, eax
    jne %%failed

    WRITE_LITERAL %2, %2 %+ _end
    jmp %%done

%%failed:
    WRITE_LITERAL %2, %2 %+ _end
    WRITE_LITERAL fail_suffix, fail_suffix_end
    jmp fail_exit

%%done:
%endmacro

_start:
    WRITE_LITERAL start_msg, start_msg_end

    CHECK_OPEN abs_hello_path, abs_hello_msg
    CHECK_OPEN rel_hello_path, rel_hello_msg
    CHECK_CHDIR tmp_path, chdir_tmp_msg
    CHECK_GETCWD tmp_expected_path, getcwd_tmp_msg
    CHECK_OPEN parent_hello_path, parent_hello_msg
    CHECK_CHDIR root_path, chdir_root_msg
    CHECK_OPEN fat32_direct_path, fat32_direct_msg
    CHECK_OPEN fat32_mnt_path, fat32_mnt_msg
    CHECK_OPEN ext4_direct_path, ext4_direct_msg
    CHECK_OPEN ext4_mnt_path, ext4_mnt_msg

    WRITE_LITERAL done_msg, done_msg_end
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

fail_exit:
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

strcmp:
    xor eax, eax
.strcmp_loop:
    mov dl, [rdi]
    mov cl, [rsi]
    cmp dl, cl
    jne .strcmp_diff
    test dl, dl
    jz .strcmp_equal
    inc rdi
    inc rsi
    jmp .strcmp_loop
.strcmp_diff:
    mov eax, 1
.strcmp_equal:
    ret

section .bss
cwd_buf:
    resb 256
cwd_buf_end:

section .rodata
abs_hello_path:
    db "/hello.txt", 0
rel_hello_path:
    db "hello.txt", 0
tmp_path:
    db "/tmp", 0
tmp_expected_path:
    db "/tmp", 0
parent_hello_path:
    db "../hello.txt", 0
root_path:
    db "/", 0
fat32_direct_path:
    db "/fat32/HELLO.TXT", 0
fat32_mnt_path:
    db "/mnt/fat32/HELLO.TXT", 0
ext4_direct_path:
    db "/ext4/readme.txt", 0
ext4_mnt_path:
    db "/mnt/ext4/readme.txt", 0

start_msg:
    db "pathtest: start", 10
start_msg_end:
abs_hello_msg:
    db "pathtest: /hello.txt ok", 10
abs_hello_msg_end:
rel_hello_msg:
    db "pathtest: hello.txt ok", 10
rel_hello_msg_end:
chdir_tmp_msg:
    db "pathtest: chdir /tmp ok", 10
chdir_tmp_msg_end:
getcwd_tmp_msg:
    db "pathtest: getcwd /tmp ok", 10
getcwd_tmp_msg_end:
parent_hello_msg:
    db "pathtest: ../hello.txt ok", 10
parent_hello_msg_end:
chdir_root_msg:
    db "pathtest: chdir / ok", 10
chdir_root_msg_end:
fat32_direct_msg:
    db "pathtest: /fat32/HELLO.TXT ok", 10
fat32_direct_msg_end:
fat32_mnt_msg:
    db "pathtest: /mnt/fat32/HELLO.TXT ok", 10
fat32_mnt_msg_end:
ext4_direct_msg:
    db "pathtest: /ext4/readme.txt ok", 10
ext4_direct_msg_end:
ext4_mnt_msg:
    db "pathtest: /mnt/ext4/readme.txt ok", 10
ext4_mnt_msg_end:
done_msg:
    db "pathtest: all ok", 10
done_msg_end:
fail_suffix:
    db "pathtest: failed", 10
fail_suffix_end:
