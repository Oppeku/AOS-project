; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_PARTITION_INFO 500
%define AOS_SYS_PARTITION_CREATE 501
%define AOS_SYS_PARTITION_DELETE 502
%define AOS_SYS_PARTITION_TYPE 503
%define AOS_SYS_BLKDEV_INFO 504
%define AOS_SYS_PARTITION_WRITE 505
%define AOS_SYS_MOUNT_INFO 506
%define AOS_SYS_PARTITION_ROLE 507
%define AOS_SYS_PARTITION_LAYOUT 508
%define PARTITION_FS_FAT32 1
%define PARTITION_FS_EXT4 2
%define PARTITION_FS_SWAP 4
%define PARTITION_ROLE_MAIN 2
%define PARTITION_ROLE_SWAP 6
%define PARTITION_ROLE_TRASH 7
%define SIZE_USE_REST 0xffffffffffffffff

%define MAX_U64_DIGITS 20
%define PART_SIZE 72
%define PART_SIZE_OFF 0
%define PART_OFFSET_OFF 8
%define PART_INDEX_OFF 24
%define PART_FS_TYPE_OFF 26
%define PART_ROLE_OFF 27
%define PART_FLAGS_OFF 28
%define PART_BLKDEV_OFF 32
%define PART_NAME_OFF 36
%define PART_FS_NAME_OFF 52
%define BLKDEV_SIZE 40
%define BLKDEV_ID_OFF 0
%define BLKDEV_BLOCK_SIZE_OFF 4
%define BLKDEV_SIZE_OFF 8
%define BLKDEV_READ_ONLY_OFF 16
%define BLKDEV_HAS_OPS_OFF 17
%define BLKDEV_NAME_OFF 24

_start:
    mov qword [rel selected_index], 0
    mov byte [rel status_code], 0
    mov byte [rel dirty_flag], 0

draw_screen:
    call clamp_selection

    lea rsi, [rel clear_seq]
    mov rdx, clear_seq_end - clear_seq
    call write_stdout

    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    call draw_dirty_status

    lea rsi, [rel disk_msg]
    mov rdx, disk_msg_end - disk_msg
    call write_stdout

    lea rsi, [rel table_header_msg]
    mov rdx, table_header_msg_end - table_header_msg
    call write_stdout

    xor r12, r12

partition_loop:
    mov rax, AOS_SYS_PARTITION_INFO
    mov rdi, r12
    lea rsi, [rel part_buf]
    syscall
    test rax, rax
    js after_table

    mov rdi, r12
    call draw_partition_row

    inc r12
    jmp partition_loop

after_table:
    mov [rel part_count], r12

    lea rsi, [rel lower_box_msg]
    mov rdx, lower_box_msg_end - lower_box_msg
    call write_stdout

    call draw_status
    call draw_selected_info

read_key:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel key_buf]
    mov rdx, 1
    syscall
    test rax, rax
    jle read_key

    mov al, [rel key_buf]
    cmp al, 27
    je read_escape_key
    cmp al, ' '
    je read_key
    cmp al, 9
    je read_key
    cmp al, 'q'
    je done
    cmp al, 'Q'
    je done
    cmp al, 10
    je done
    cmp al, 13
    je done

    cmp al, 'c'
    je create_partition
    cmp al, 'C'
    je create_partition
    cmp al, 'd'
    je delete_partition
    cmp al, 'D'
    je delete_partition
    cmp al, 't'
    je type_partition
    cmp al, 'T'
    je type_partition
    cmp al, 'r'
    je role_partition
    cmp al, 'R'
    je role_partition
    cmp al, 'l'
    je default_layout
    cmp al, 'L'
    je default_layout
    cmp al, 'm'
    je create_main_partition
    cmp al, 'M'
    je create_main_partition
    cmp al, 's'
    je create_swap_partition
    cmp al, 'S'
    je create_swap_partition
    cmp al, 'x'
    je create_trash_partition
    cmp al, 'X'
    je create_trash_partition
    cmp al, 'w'
    je write_partition_table
    cmp al, 'W'
    je write_partition_table
    cmp al, 'j'
    je select_down
    cmp al, 'J'
    je select_down
    cmp al, 'k'
    je select_up
    cmp al, 'K'
    je select_up

    mov byte [rel status_code], 6
    jmp draw_screen

read_escape_key:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel key_buf]
    mov rdx, 1
    syscall
    cmp byte [rel key_buf], '['
    jne read_key
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel key_buf]
    mov rdx, 1
    syscall
    mov al, [rel key_buf]
    cmp al, 'A'
    je select_up
    cmp al, 'B'
    je select_down
    jmp read_key

select_down:
    mov rax, [rel selected_index]
    inc rax
    cmp rax, [rel part_count]
    jae .keep
    mov [rel selected_index], rax
.keep:
    mov byte [rel status_code], 0
    jmp draw_screen

select_up:
    mov rax, [rel selected_index]
    test rax, rax
    jz .keep
    dec rax
    mov [rel selected_index], rax
.keep:
    mov byte [rel status_code], 0
    jmp draw_screen

create_partition:
    mov rax, AOS_SYS_PARTITION_CREATE
    mov rdi, PARTITION_FS_EXT4
    mov rsi, 8388608
    xor rdx, rdx
    syscall
    test rax, rax
    js .failed
    mov [rel selected_index], rax
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 1
    jmp draw_screen
.failed:
    mov byte [rel status_code], 2
    jmp draw_screen

create_main_partition:
    mov rax, AOS_SYS_PARTITION_CREATE
    mov rdi, PARTITION_FS_EXT4
    mov rsi, SIZE_USE_REST
    mov rdx, PARTITION_ROLE_MAIN
    syscall
    test rax, rax
    js .failed
    mov [rel selected_index], rax
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 12
    jmp draw_screen
.failed:
    mov byte [rel status_code], 2
    jmp draw_screen

create_swap_partition:
    mov rax, AOS_SYS_PARTITION_CREATE
    mov rdi, PARTITION_FS_SWAP
    mov rsi, 1048576
    mov rdx, PARTITION_ROLE_SWAP
    syscall
    test rax, rax
    js .failed
    mov [rel selected_index], rax
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 13
    jmp draw_screen
.failed:
    mov byte [rel status_code], 2
    jmp draw_screen

create_trash_partition:
    mov rax, AOS_SYS_PARTITION_CREATE
    mov rdi, PARTITION_FS_FAT32
    mov rsi, 2097152
    mov rdx, PARTITION_ROLE_TRASH
    syscall
    test rax, rax
    js .failed
    mov [rel selected_index], rax
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 14
    jmp draw_screen
.failed:
    mov byte [rel status_code], 2
    jmp draw_screen

delete_partition:
    mov rax, AOS_SYS_PARTITION_DELETE
    mov rdi, [rel selected_index]
    syscall
    test rax, rax
    js .failed
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 3
    jmp draw_screen
.failed:
    mov byte [rel status_code], 4
    jmp draw_screen

type_partition:
    mov rax, AOS_SYS_PARTITION_TYPE
    mov rdi, [rel selected_index]
    syscall
    test rax, rax
    js .failed
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 5
    jmp draw_screen
.failed:
    mov byte [rel status_code], 4
    jmp draw_screen

role_partition:
    mov rax, AOS_SYS_PARTITION_ROLE
    mov rdi, [rel selected_index]
    syscall
    test rax, rax
    js .failed
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 9
    jmp draw_screen
.failed:
    mov byte [rel status_code], 4
    jmp draw_screen

write_partition_table:
    mov rax, AOS_SYS_PARTITION_WRITE
    xor rdi, rdi
    syscall
    test rax, rax
    js .failed
    mov byte [rel dirty_flag], 0
    mov byte [rel status_code], 7
    jmp draw_screen
.failed:
    mov byte [rel status_code], 8
    jmp draw_screen

default_layout:
    mov rax, AOS_SYS_PARTITION_LAYOUT
    xor rdi, rdi
    syscall
    test rax, rax
    js .failed
    mov qword [rel selected_index], 0
    mov byte [rel dirty_flag], 1
    mov byte [rel status_code], 10
    jmp draw_screen
.failed:
    mov byte [rel status_code], 11
    jmp draw_screen

done:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

draw_partition_row:
    push r12

    mov rax, [rel selected_index]
    cmp rax, rdi
    jne .not_selected
    lea rsi, [rel selected_prefix_msg]
    mov rdx, selected_prefix_msg_end - selected_prefix_msg
    call write_stdout
    jmp .after_prefix
.not_selected:
    lea rsi, [rel row_prefix_msg]
    mov rdx, row_prefix_msg_end - row_prefix_msg
    call write_stdout
.after_prefix:
    movzx edi, word [rel part_buf + PART_INDEX_OFF]
    inc rdi
    call write_u64_decimal
    lea rsi, [rel row_gap_no_msg]
    mov rdx, row_gap_no_msg_end - row_gap_no_msg
    call write_stdout

    lea rsi, [rel part_buf + PART_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel row_gap_1_msg]
    mov rdx, row_gap_1_msg_end - row_gap_1_msg
    call write_stdout

    lea rsi, [rel part_buf + PART_FS_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel row_gap_2_msg]
    mov rdx, row_gap_2_msg_end - row_gap_2_msg
    call write_stdout

    mov rdi, [rel part_buf + PART_OFFSET_OFF]
    shr rdi, 9
    call write_u64_decimal
    lea rsi, [rel row_gap_2_msg]
    mov rdx, row_gap_2_msg_end - row_gap_2_msg
    call write_stdout

    mov rdi, [rel part_buf + PART_OFFSET_OFF]
    mov rax, [rel part_buf + PART_SIZE_OFF]
    add rdi, rax
    shr rdi, 9
    test rdi, rdi
    jz .end_zero
    dec rdi
.end_zero:
    call write_u64_decimal
    lea rsi, [rel row_gap_2_msg]
    mov rdx, row_gap_2_msg_end - row_gap_2_msg
    call write_stdout

    mov rdi, [rel part_buf + PART_SIZE_OFF]
    call write_size_label

    cmp byte [rel part_buf + PART_FLAGS_OFF], 0
    je .newline
    lea rsi, [rel planned_msg]
    mov rdx, planned_msg_end - planned_msg
    call write_stdout
.newline:
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    pop r12
    ret

draw_blkdev_table:
    lea rsi, [rel blkdev_header_msg]
    mov rdx, blkdev_header_msg_end - blkdev_header_msg
    call write_stdout

    xor r12, r12
.loop:
    mov rax, AOS_SYS_BLKDEV_INFO
    mov rdi, r12
    lea rsi, [rel blkdev_buf]
    syscall
    test rax, rax
    js .done
    call draw_blkdev_row
    inc r12
    jmp .loop
.done:
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    ret

draw_blkdev_row:
    lea rsi, [rel blkdev_row_prefix_msg]
    mov rdx, blkdev_row_prefix_msg_end - blkdev_row_prefix_msg
    call write_stdout

    lea rsi, [rel blkdev_buf + BLKDEV_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel blkdev_gap_1_msg]
    mov rdx, blkdev_gap_1_msg_end - blkdev_gap_1_msg
    call write_stdout

    mov edi, [rel blkdev_buf + BLKDEV_ID_OFF]
    call write_u64_decimal
    lea rsi, [rel blkdev_gap_2_msg]
    mov rdx, blkdev_gap_2_msg_end - blkdev_gap_2_msg
    call write_stdout

    mov edi, [rel blkdev_buf + BLKDEV_BLOCK_SIZE_OFF]
    call write_u64_decimal
    lea rsi, [rel blkdev_gap_2_msg]
    mov rdx, blkdev_gap_2_msg_end - blkdev_gap_2_msg
    call write_stdout

    mov rdi, [rel blkdev_buf + BLKDEV_SIZE_OFF]
    call write_size_label
    lea rsi, [rel blkdev_gap_2_msg]
    mov rdx, blkdev_gap_2_msg_end - blkdev_gap_2_msg
    call write_stdout

    cmp byte [rel blkdev_buf + BLKDEV_READ_ONLY_OFF], 0
    je .rw
    lea rsi, [rel blkdev_ro_msg]
    mov rdx, blkdev_ro_msg_end - blkdev_ro_msg
    jmp .mode
.rw:
    lea rsi, [rel blkdev_rw_msg]
    mov rdx, blkdev_rw_msg_end - blkdev_rw_msg
.mode:
    call write_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    jmp write_stdout

draw_dirty_status:
    cmp byte [rel dirty_flag], 0
    je .clean
    lea rsi, [rel dirty_status_msg]
    mov rdx, dirty_status_msg_end - dirty_status_msg
    jmp write_stdout
.clean:
    lea rsi, [rel clean_status_msg]
    mov rdx, clean_status_msg_end - clean_status_msg
    jmp write_stdout

draw_selected_info:
    mov rax, AOS_SYS_PARTITION_INFO
    mov rdi, [rel selected_index]
    lea rsi, [rel part_buf]
    syscall
    test rax, rax
    js .done

    lea rsi, [rel selected_box_top_msg]
    mov rdx, selected_box_top_msg_end - selected_box_top_msg
    call write_stdout

    lea rsi, [rel selected_info_prefix_msg]
    mov rdx, selected_info_prefix_msg_end - selected_info_prefix_msg
    call write_stdout

    lea rsi, [rel part_buf + PART_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel selected_type_msg]
    mov rdx, selected_type_msg_end - selected_type_msg
    call write_stdout
    lea rsi, [rel part_buf + PART_FS_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel selected_size_msg]
    mov rdx, selected_size_msg_end - selected_size_msg
    call write_stdout
    mov rdi, [rel part_buf + PART_SIZE_OFF]
    call write_size_label
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel selected_box_bottom_msg]
    mov rdx, selected_box_bottom_msg_end - selected_box_bottom_msg
    call write_stdout
.done:
    ret

draw_free_space:
    lea rsi, [rel free_space_prefix_msg]
    mov rdx, free_space_prefix_msg_end - free_space_prefix_msg
    call write_stdout

    mov rdi, 67108864
    mov rax, [rel part_count]
    test rax, rax
    jz .emit_free
    dec rax
    mov rdi, rax
    mov rax, AOS_SYS_PARTITION_INFO
    lea rsi, [rel part_buf]
    syscall
    test rax, rax
    js .emit_free
    mov rdi, [rel part_buf + PART_OFFSET_OFF]
    mov rax, [rel part_buf + PART_SIZE_OFF]
    add rdi, rax
    mov rax, 67108864
    sub rax, rdi
    mov rdi, rax
.emit_free:
    call write_size_label
    lea rsi, [rel newline3_msg]
    mov rdx, newline3_msg_end - newline3_msg
    call write_stdout
    ret

draw_status:
    mov al, [rel status_code]
    cmp al, 1
    je .created
    cmp al, 2
    je .no_space
    cmp al, 3
    je .deleted
    cmp al, 4
    je .blocked
    cmp al, 5
    je .typed
    cmp al, 6
    je .bad_key
    cmp al, 7
    je .write_blocked
    cmp al, 8
    je .write_failed
    cmp al, 9
    je .role
    cmp al, 10
    je .layout
    cmp al, 11
    je .layout_failed
    cmp al, 12
    je .main_created
    cmp al, 13
    je .swap_created
    cmp al, 14
    je .trash_created
    lea rsi, [rel status_default_msg]
    mov rdx, status_default_msg_end - status_default_msg
    jmp write_stdout
.created:
    lea rsi, [rel status_created_msg]
    mov rdx, status_created_msg_end - status_created_msg
    jmp write_stdout
.no_space:
    lea rsi, [rel status_no_space_msg]
    mov rdx, status_no_space_msg_end - status_no_space_msg
    jmp write_stdout
.deleted:
    lea rsi, [rel status_deleted_msg]
    mov rdx, status_deleted_msg_end - status_deleted_msg
    jmp write_stdout
.blocked:
    lea rsi, [rel status_blocked_msg]
    mov rdx, status_blocked_msg_end - status_blocked_msg
    jmp write_stdout
.typed:
    lea rsi, [rel status_typed_msg]
    mov rdx, status_typed_msg_end - status_typed_msg
    jmp write_stdout
.bad_key:
    lea rsi, [rel status_bad_key_msg]
    mov rdx, status_bad_key_msg_end - status_bad_key_msg
    jmp write_stdout
.write_blocked:
    lea rsi, [rel status_write_done_msg]
    mov rdx, status_write_done_msg_end - status_write_done_msg
    jmp write_stdout
.write_failed:
    lea rsi, [rel status_write_failed_msg]
    mov rdx, status_write_failed_msg_end - status_write_failed_msg
    jmp write_stdout
.role:
    lea rsi, [rel status_role_msg]
    mov rdx, status_role_msg_end - status_role_msg
    jmp write_stdout
.layout:
    lea rsi, [rel status_layout_msg]
    mov rdx, status_layout_msg_end - status_layout_msg
    jmp write_stdout
.layout_failed:
    lea rsi, [rel status_layout_failed_msg]
    mov rdx, status_layout_failed_msg_end - status_layout_failed_msg
    jmp write_stdout
.main_created:
    lea rsi, [rel status_main_created_msg]
    mov rdx, status_main_created_msg_end - status_main_created_msg
    jmp write_stdout
.swap_created:
    lea rsi, [rel status_swap_created_msg]
    mov rdx, status_swap_created_msg_end - status_swap_created_msg
    jmp write_stdout
.trash_created:
    lea rsi, [rel status_trash_created_msg]
    mov rdx, status_trash_created_msg_end - status_trash_created_msg
    jmp write_stdout

clamp_selection:
    xor r12, r12
.count_loop:
    mov rax, AOS_SYS_PARTITION_INFO
    mov rdi, r12
    lea rsi, [rel part_buf]
    syscall
    test rax, rax
    js .count_done
    inc r12
    jmp .count_loop
.count_done:
    mov [rel part_count], r12
    test r12, r12
    jz .zero
    mov rax, [rel selected_index]
    cmp rax, r12
    jb .done
    dec r12
    mov [rel selected_index], r12
    ret
.zero:
    mov qword [rel selected_index], 0
.done:
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
    lea rbx, [rel decimal_buffer + MAX_U64_DIGITS]
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

write_size_label:
    mov rax, rdi
    mov rcx, 1048576
    xor rdx, rdx
    div rcx
    test rax, rax
    jz .bytes
    mov rdi, rax
    call write_u64_decimal
    lea rsi, [rel mib_msg]
    mov rdx, mib_msg_end - mib_msg
    jmp write_stdout
.bytes:
    call write_u64_decimal
    lea rsi, [rel bytes_msg]
    mov rdx, bytes_msg_end - bytes_msg
    jmp write_stdout

write_role_name:
    cmp edi, 1
    je .root
    cmp edi, 2
    je .main
    cmp edi, 3
    je .etc
    cmp edi, 4
    je .commands
    cmp edi, 5
    je .tmp
    cmp edi, 6
    je .swap
    cmp edi, 7
    je .trash
    lea rsi, [rel unknown_role_msg]
    mov rdx, unknown_role_msg_end - unknown_role_msg
    jmp write_stdout
.root:
    lea rsi, [rel root_role_msg]
    mov rdx, root_role_msg_end - root_role_msg
    jmp write_stdout
.main:
    lea rsi, [rel main_role_msg]
    mov rdx, main_role_msg_end - main_role_msg
    jmp write_stdout
.etc:
    lea rsi, [rel etc_role_msg]
    mov rdx, etc_role_msg_end - etc_role_msg
    jmp write_stdout
.commands:
    lea rsi, [rel commands_role_msg]
    mov rdx, commands_role_msg_end - commands_role_msg
    jmp write_stdout
.tmp:
    lea rsi, [rel tmp_role_msg]
    mov rdx, tmp_role_msg_end - tmp_role_msg
    jmp write_stdout
.swap:
    lea rsi, [rel swap_role_msg]
    mov rdx, swap_role_msg_end - swap_role_msg
    jmp write_stdout
.trash:
    lea rsi, [rel trash_role_msg]
    mov rdx, trash_role_msg_end - trash_role_msg
    jmp write_stdout

section .bss
part_buf:
    resb PART_SIZE
blkdev_buf:
    resb BLKDEV_SIZE
decimal_buffer:
    resb MAX_U64_DIGITS + 1
key_buf:
    resb 1
selected_index:
    resq 1
part_count:
    resq 1
status_code:
    resb 1
dirty_flag:
    resb 1

section .rodata
clear_seq:
    db 27, 'L'
clear_seq_end:

white_seq:
    db 27, 'C', 0x0F
white_seq_end:

normal_seq:
    db 27, 'C', 0x07
normal_seq_end:

invert_seq:
    db 27, 'C', 0x70
invert_seq_end:

title_msg:
    db 10
    db "                              AOS Partition Manager", 10
    db "               Disk: ata0  Image: build/aosfs.img  Table: AOSPT1", 10
title_msg_end:

clean_status_msg:
    db "                              Status: Clean", 10, 10
clean_status_msg_end:

dirty_status_msg:
    db "                         Status: Unsaved changes", 10, 10
dirty_status_msg_end:

blkdev_header_msg:
    db "    Raw block devices", 10
    db "    Name          Id    Block     Size      Mode", 10
    db "    ------------------------------------------------", 10
blkdev_header_msg_end:

blkdev_row_prefix_msg:
    db "    "
blkdev_row_prefix_msg_end:

blkdev_gap_1_msg:
    db "          "
blkdev_gap_1_msg_end:

blkdev_gap_2_msg:
    db "      "
blkdev_gap_2_msg_end:

blkdev_ro_msg:
    db "ro"
blkdev_ro_msg_end:

blkdev_rw_msg:
    db "rw"
blkdev_rw_msg_end:

disk_msg:
    db "    +------------------------------------------------------------------------+", 10
    db "    | No   Name             Type           Start        End          Size     |", 10
disk_msg_end:

table_header_msg:
    db "    |------------------------------------------------------------------------|", 10
table_header_msg_end:

selected_prefix_msg:
    db "    > "
selected_prefix_msg_end:

row_prefix_msg:
    db "    | "
row_prefix_msg_end:

row_gap_no_msg:
    db "    "
row_gap_no_msg_end:

row_gap_1_msg:
    db "        "
row_gap_1_msg_end:

row_gap_2_msg:
    db "      "
row_gap_2_msg_end:

planned_msg:
    db "  planned"
planned_msg_end:

bytes_msg:
    db " bytes"
bytes_msg_end:

mib_msg:
    db "M"
mib_msg_end:

root_role_msg:
    db "root"
root_role_msg_end:

main_role_msg:
    db "main"
main_role_msg_end:

etc_role_msg:
    db "etc"
etc_role_msg_end:

commands_role_msg:
    db "commands"
commands_role_msg_end:

tmp_role_msg:
    db "tmp"
tmp_role_msg_end:

swap_role_msg:
    db "swap"
swap_role_msg_end:

trash_role_msg:
    db "trash"
trash_role_msg_end:

unknown_role_msg:
    db "unknown"
unknown_role_msg_end:

newline_msg:
    db 10
newline_msg_end:

newline3_msg:
    db 10, 10, 10
newline3_msg_end:

free_space_prefix_msg:
    db "    Free space remaining: "
free_space_prefix_msg_end:

lower_box_msg:
    db "    +------------------------------------------------------------------------+", 10
    db "                   Use j/k to select partition", 10, 10
    db "        [ n New ]  [ m Main ]  [ s Swap ]  [ x Trash ]  [ d Delete ]", 10
    db "        [ l Root ] [ t Type ]  [ r Role ]  [ w Write ]  [ q Quit ]", 10
lower_box_msg_end:

create_button_msg:
    db "      [ Create ]"
create_button_msg_end:

menu_tail_msg:
    db "    [ New ]    [ Main ]    [ Swap ]    [ Trash ]    [ Write ]    [ Quit ]", 10
menu_tail_msg_end:

status_default_msg:
    db "      Arch-style: l makes root, then m/s/x add user partitions, w writes.", 10
status_default_msg_end:

status_created_msg:
    db "      Created planned 8M ext4 partition on ata0.", 10
status_created_msg_end:

status_no_space_msg:
    db "      No space left, or partition table is full.", 10
status_no_space_msg_end:

status_deleted_msg:
    db "      Deleted selected planned partition.", 10
status_deleted_msg_end:

status_blocked_msg:
    db "      That entry is real/read-only right now. Create a planned partition first.", 10
status_blocked_msg_end:

status_typed_msg:
    db "      Changed selected planned partition type.", 10
status_typed_msg_end:

status_role_msg:
    db "      Changed selected planned partition role.", 10
status_role_msg_end:

status_bad_key_msg:
    db "      Unknown key. Use j k l m s x c d t r w q.", 10
status_bad_key_msg_end:

status_write_done_msg:
    db "      Wrote AOS partition table with roles to ata0 block 0.", 10
status_write_done_msg_end:

status_write_failed_msg:
    db "      Could not write AOS partition table to ata0.", 10
status_write_failed_msg_end:

status_layout_msg:
    db "      Created automatic root AOSFS. User creates /main/apps later.", 10
status_layout_msg_end:

status_layout_failed_msg:
    db "      Could not create default AOS layout on ata0.", 10
status_layout_failed_msg_end:

status_main_created_msg:
    db "      Created /main ext4 using remaining free ata0 space.", 10
status_main_created_msg_end:

status_swap_created_msg:
    db "      Created 1M swap partition on ata0.", 10
status_swap_created_msg_end:

status_trash_created_msg:
    db "      Created 2M /trash FAT32 partition on ata0.", 10
status_trash_created_msg_end:

selected_box_top_msg:
    db "    +------------------------------------------------------------------------+", 10
selected_box_top_msg_end:

selected_info_prefix_msg:
    db "    | Selected: "
selected_info_prefix_msg_end:

selected_type_msg:
    db " | Type: "
selected_type_msg_end:

selected_size_msg:
    db " | Size: "
selected_size_msg_end:

selected_box_bottom_msg:
    db "    +------------------------------------------------------------------------+", 10
selected_box_bottom_msg_end:
