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

%define MAX_U64_DIGITS 20
%define PART_SIZE 64
%define PART_SIZE_OFF 0
%define PART_OFFSET_OFF 8
%define PART_INDEX_OFF 24
%define PART_FLAGS_OFF 27
%define PART_NAME_OFF 28
%define PART_FS_NAME_OFF 44

_start:
    mov qword [rel selected_index], 0
    mov byte [rel status_code], 0

draw_screen:
    call clamp_selection

    lea rsi, [rel clear_seq]
    mov rdx, clear_seq_end - clear_seq
    call write_stdout

    lea rsi, [rel white_seq]
    mov rdx, white_seq_end - white_seq
    call write_stdout

    lea rsi, [rel screen_top_msg]
    mov rdx, screen_top_msg_end - screen_top_msg
    call write_stdout

    lea rsi, [rel normal_seq]
    mov rdx, normal_seq_end - normal_seq
    call write_stdout

    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

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
    call draw_free_space

    lea rsi, [rel lower_box_msg]
    mov rdx, lower_box_msg_end - lower_box_msg
    call write_stdout

    lea rsi, [rel invert_seq]
    mov rdx, invert_seq_end - invert_seq
    call write_stdout

    lea rsi, [rel create_button_msg]
    mov rdx, create_button_msg_end - create_button_msg
    call write_stdout

    lea rsi, [rel normal_seq]
    mov rdx, normal_seq_end - normal_seq
    call write_stdout

    lea rsi, [rel menu_tail_msg]
    mov rdx, menu_tail_msg_end - menu_tail_msg
    call write_stdout

    call draw_status

read_key:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel key_buf]
    mov rdx, 1
    syscall
    test rax, rax
    jle read_key

    mov al, [rel key_buf]
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
    mov rdi, 2
    mov rsi, 8388608
    syscall
    test rax, rax
    js .failed
    mov [rel selected_index], rax
    mov byte [rel status_code], 1
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
    mov byte [rel status_code], 5
    jmp draw_screen
.failed:
    mov byte [rel status_code], 4
    jmp draw_screen

write_partition_table:
    mov byte [rel status_code], 7
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
    lea rsi, [rel part_buf + PART_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel row_gap_1_msg]
    mov rdx, row_gap_1_msg_end - row_gap_1_msg
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
    shr rdi, 9
    call write_u64_decimal
    lea rsi, [rel row_gap_2_msg]
    mov rdx, row_gap_2_msg_end - row_gap_2_msg
    call write_stdout

    mov rdi, [rel part_buf + PART_SIZE_OFF]
    call write_size_label
    lea rsi, [rel row_gap_2_msg]
    mov rdx, row_gap_2_msg_end - row_gap_2_msg
    call write_stdout

    lea rsi, [rel part_buf + PART_FS_NAME_OFF]
    call write_cstring_stdout

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
    lea rsi, [rel status_write_blocked_msg]
    mov rdx, status_write_blocked_msg_end - status_write_blocked_msg
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

section .bss
part_buf:
    resb PART_SIZE
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

screen_top_msg:
    db "                         wait here - PartiotionMANAGAER", 10, 10
screen_top_msg_end:

title_msg:
    db "                           Disk: AOS RAM disk image", 10
    db "                  Size: 64 MiB, 67108864 bytes, 131072 sectors", 10, 10
title_msg_end:

disk_msg:
    db "    Device        Start      End        Sectors    Size      Type", 10
disk_msg_end:

table_header_msg:
    db "    ---------------------------------------------------------------", 10
table_header_msg_end:

selected_prefix_msg:
    db "  > "
selected_prefix_msg_end:

row_prefix_msg:
    db "    "
row_prefix_msg_end:

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
    db "    +------------------------------------------------------------------+", 10
    db "    | j/k=select   c=create 8M ext4   d=delete planned partition      |", 10
    db "    | t=change selected type          w=write table later   q=quit    |", 10
    db "    +------------------------------------------------------------------+", 10, 10
lower_box_msg_end:

create_button_msg:
    db "      [ Create ]"
create_button_msg_end:

menu_tail_msg:
    db "    [ Delete ]    [ Type ]    [ Write ]    [ Quit ]", 10
menu_tail_msg_end:

status_default_msg:
    db "      Select with j/k. Only planned partitions can be changed before blkdev.", 10
status_default_msg_end:

status_created_msg:
    db "      Created planned 8M ext4 partition.", 10
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

status_bad_key_msg:
    db "      Unknown key. Use j k c d t w q.", 10
status_bad_key_msg_end:

status_write_blocked_msg:
    db "      Write is blocked until block-device support exists.", 10
status_write_blocked_msg_end:
