; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

bits 64
default rel
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_DISPLAY_INFO 511
%define AOS_SYS_DISPLAY_SET 512

%define MENU_COUNT 8
%define DISPLAY_COUNT 4
%define KEY_UP 0x11
%define KEY_DOWN 0x12
%define KEY_LEFT 0x13
%define KEY_RIGHT 0x14

_start:
    mov qword [rel selected_index], 0
    mov byte [rel selected_action], 0
    mov byte [rel status_code], 0
    mov byte [rel page_mode], 0
    mov byte [rel pending_display], 0

draw_screen:
    call read_display_info

    lea rsi, [rel blue_color_msg]
    mov rdx, blue_color_msg_end - blue_color_msg
    call write_stdout
    lea rsi, [rel clear_msg]
    mov rdx, clear_msg_end - clear_msg
    call write_stdout

    lea rsi, [rel panel_color_msg]
    mov rdx, panel_color_msg_end - panel_color_msg
    call write_stdout
    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout
    call newline

    call draw_display_line
    call newline

    cmp byte [rel page_mode], 1
    je draw_display_page

draw_main_page:
    lea rsi, [rel main_label_msg]
    mov rdx, main_label_msg_end - main_label_msg
    call write_stdout

    lea rsi, [rel box_top_msg]
    mov rdx, box_top_msg_end - box_top_msg
    call write_stdout

    xor rdi, rdi
    lea rsi, [rel item_system]
    mov rdx, item_system_end - item_system
    call draw_item

    mov rdi, 1
    lea rsi, [rel item_display]
    mov rdx, item_display_end - item_display
    call draw_item

    mov rdi, 2
    lea rsi, [rel item_memory]
    mov rdx, item_memory_end - item_memory
    call draw_item

    mov rdi, 3
    lea rsi, [rel item_boot]
    mov rdx, item_boot_end - item_boot
    call draw_item

    mov rdi, 4
    lea rsi, [rel item_storage]
    mov rdx, item_storage_end - item_storage
    call draw_item

    mov rdi, 5
    lea rsi, [rel item_user]
    mov rdx, item_user_end - item_user
    call draw_item

    mov rdi, 6
    lea rsi, [rel item_mui]
    mov rdx, item_mui_end - item_mui
    call draw_item

    mov rdi, 7
    lea rsi, [rel item_about]
    mov rdx, item_about_end - item_about
    call draw_item

    lea rsi, [rel box_bottom_msg]
    mov rdx, box_bottom_msg_end - box_bottom_msg
    call write_stdout

    call draw_status
    call draw_actions

    lea rsi, [rel help_msg]
    mov rdx, help_msg_end - help_msg
    call write_stdout
    jmp read_key

draw_display_page:
    lea rsi, [rel display_label_msg]
    mov rdx, display_label_msg_end - display_label_msg
    call write_stdout

    lea rsi, [rel box_top_msg]
    mov rdx, box_top_msg_end - box_top_msg
    call write_stdout

    xor rdi, rdi
    lea rsi, [rel option_display_auto]
    mov rdx, option_display_auto_end - option_display_auto
    call draw_item

    mov rdi, 1
    lea rsi, [rel option_display_full]
    mov rdx, option_display_full_end - option_display_full
    call draw_item

    mov rdi, 2
    lea rsi, [rel option_display_compact]
    mov rdx, option_display_compact_end - option_display_compact
    call draw_item

    mov rdi, 3
    lea rsi, [rel option_display_tiny]
    mov rdx, option_display_tiny_end - option_display_tiny
    call draw_item

    lea rsi, [rel box_bottom_msg]
    mov rdx, box_bottom_msg_end - box_bottom_msg
    call write_stdout

    call draw_status
    call draw_actions

    lea rsi, [rel help_msg]
    mov rdx, help_msg_end - help_msg
    call write_stdout

read_key:
    mov rax, SYS_READ
    xor rdi, rdi
    lea rsi, [rel key_buf]
    mov rdx, 1
    syscall
    test rax, rax
    jle read_key

    mov al, [rel key_buf]
    cmp al, KEY_UP
    je select_up
    cmp al, KEY_DOWN
    je select_down
    cmp al, KEY_LEFT
    je action_left
    cmp al, KEY_RIGHT
    je action_right
    cmp al, 27
    je read_escape_key
    cmp al, 'q'
    je done
    cmp al, 'Q'
    je done
    cmp al, 10
    je activate_current
    cmp al, 13
    je activate_current
    cmp al, 'h'
    je action_left
    cmp al, 'H'
    je action_left
    cmp al, 'l'
    je action_right
    cmp al, 'L'
    je action_right
    cmp al, 'j'
    je select_down
    cmp al, 'J'
    je select_down
    cmp al, 'k'
    je select_up
    cmp al, 'K'
    je select_up
    cmp al, 'A'
    je select_up
    cmp al, 'B'
    je select_down
    cmp al, 'C'
    je action_right
    cmp al, 'D'
    je action_left
    cmp al, '['
    je read_key
    mov byte [rel status_code], 9
    call redraw_status_only
    jmp read_key

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
    cmp al, 'C'
    je action_right
    cmp al, 'D'
    je action_left
    jmp read_key

select_down:
    mov byte [rel selected_action], 0
    mov rax, [rel selected_index]
    mov [rel prev_index], rax
    inc rax
    mov rcx, MENU_COUNT
    cmp byte [rel page_mode], 1
    jne .have_count
    mov rcx, DISPLAY_COUNT
.have_count:
    cmp rax, rcx
    jb .store
    xor rax, rax
.store:
    mov [rel selected_index], rax
    mov byte [rel status_code], 0
    call redraw_selection
    jmp read_key

select_up:
    mov byte [rel selected_action], 0
    mov rax, [rel selected_index]
    mov [rel prev_index], rax
    test rax, rax
    jnz .dec
    mov rax, MENU_COUNT
    cmp byte [rel page_mode], 1
    jne .dec
    mov rax, DISPLAY_COUNT
.dec:
    dec rax
    mov [rel selected_index], rax
    mov byte [rel status_code], 0
    call redraw_selection
    jmp read_key

action_left:
    mov al, [rel selected_action]
    cmp al, 0
    jne .has_action
    mov byte [rel selected_action], 1
    jmp .done
.has_action:
    cmp al, 1
    jne .dec
    mov byte [rel selected_action], 3
    jmp .done
.dec:
    dec al
    mov [rel selected_action], al
.done:
    mov byte [rel status_code], 0
    call redraw_action_focus
    jmp read_key

action_right:
    mov al, [rel selected_action]
    cmp al, 0
    jne .has_action
    mov byte [rel selected_action], 1
    jmp .done
.has_action:
    cmp al, 3
    jne .inc
    mov byte [rel selected_action], 1
    jmp .done
.inc:
    inc al
    mov [rel selected_action], al
.done:
    mov byte [rel status_code], 0
    call redraw_action_focus
    jmp read_key

activate_current:
    cmp byte [rel selected_action], 1
    je cancel_action
    cmp byte [rel selected_action], 2
    je save_settings
    cmp byte [rel selected_action], 3
    je done

    cmp byte [rel page_mode], 1
    je activate_display_option

    mov rax, [rel selected_index]
    cmp rax, 0
    je system_settings
    cmp rax, 1
    je open_display_page
    cmp rax, 7
    je show_about
    mov byte [rel status_code], 5
    jmp draw_screen

open_display_page:
    mov byte [rel page_mode], 1
    mov qword [rel selected_index], 0
    mov byte [rel selected_action], 0
    mov byte [rel status_code], 0
    jmp draw_screen

activate_display_option:
    mov rax, [rel selected_index]
    cmp rax, 0
    je choose_display_auto
    cmp rax, 1
    je choose_display_full
    cmp rax, 2
    je choose_display_compact
    cmp rax, 3
    je choose_display_tiny
    mov byte [rel status_code], 6
    jmp draw_screen

save_settings:
    call apply_pending_display
    jmp draw_screen

cancel_action:
    mov byte [rel pending_display], 0
    cmp byte [rel page_mode], 1
    jne .main_cancel
    mov byte [rel page_mode], 0
    mov qword [rel selected_index], 0
    mov byte [rel selected_action], 0
    mov byte [rel status_code], 12
    jmp draw_screen
.main_cancel:
    mov byte [rel selected_action], 0
    mov byte [rel status_code], 12
    jmp draw_screen

system_settings:
    mov byte [rel status_code], 11
    call redraw_status_only
    jmp read_key

choose_display_auto:
    mov byte [rel pending_display], 1
    mov byte [rel status_code], 1
    call redraw_status_only
    jmp read_key

choose_display_full:
    mov byte [rel pending_display], 2
    mov byte [rel status_code], 2
    call redraw_status_only
    jmp read_key

choose_display_compact:
    mov byte [rel pending_display], 3
    mov byte [rel status_code], 3
    call redraw_status_only
    jmp read_key

choose_display_tiny:
    mov byte [rel pending_display], 4
    mov byte [rel status_code], 4
    call redraw_status_only
    jmp read_key

apply_pending_display:
    mov al, [rel pending_display]
    test al, al
    jz .saved
    cmp al, 1
    je set_display_auto
    cmp al, 2
    je set_display_full
    cmp al, 3
    je set_display_compact
    cmp al, 4
    je set_display_tiny
.saved:
    mov byte [rel status_code], 10
    ret

set_display_auto:
    xor rdi, rdi
    xor rsi, rsi
    mov byte [rel status_code], 10
    jmp set_display

set_display_full:
    mov rdi, 80
    mov rsi, 25
    mov byte [rel status_code], 10
    jmp set_display

set_display_compact:
    mov rdi, 60
    mov rsi, 20
    mov byte [rel status_code], 10
    jmp set_display

set_display_tiny:
    mov rdi, 40
    mov rsi, 15
    mov byte [rel status_code], 10

set_display:
    mov rax, AOS_SYS_DISPLAY_SET
    syscall
    test rax, rax
    js .failed
    mov byte [rel pending_display], 0
    ret
.failed:
    mov byte [rel status_code], 6
    ret

show_about:
    mov byte [rel status_code], 8
    call redraw_status_only
    jmp read_key

done:
    lea rsi, [rel normal_color_msg]
    mov rdx, normal_color_msg_end - normal_color_msg
    call write_stdout
    lea rsi, [rel clear_msg]
    mov rdx, clear_msg_end - clear_msg
    call write_stdout
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

draw_item:
    push rdi
    push rsi
    push rdx

    cmp byte [rel selected_action], 0
    jne .plain
    mov rax, [rel selected_index]
    cmp rax, rdi
    jne .plain

    lea rsi, [rel highlight_color_msg]
    mov rdx, highlight_color_msg_end - highlight_color_msg
    call write_stdout
    lea rsi, [rel selected_prefix]
    mov rdx, selected_prefix_end - selected_prefix
    call write_stdout
    pop rdx
    pop rsi
    push rsi
    push rdx
    call write_stdout
    lea rsi, [rel panel_color_msg]
    mov rdx, panel_color_msg_end - panel_color_msg
    call write_stdout
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call newline
    pop rdx
    pop rsi
    pop rdi
    ret

.plain:
    lea rsi, [rel plain_prefix]
    mov rdx, plain_prefix_end - plain_prefix
    call write_stdout
    pop rdx
    pop rsi
    push rsi
    push rdx
    call write_stdout
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call newline
    pop rdx
    pop rsi
    pop rdi
    ret

redraw_selection:
    mov rdi, [rel prev_index]
    call draw_indexed_item
    mov rdi, [rel selected_index]
    call draw_indexed_item
    call redraw_status_only
    call redraw_actions_only
    ret

draw_indexed_item:
    push rdi
    mov rax, rdi
    add rax, 6
    mov rdi, rax
    call write_cursor_row
    pop rdi

    cmp byte [rel page_mode], 1
    je .display

    cmp rdi, 0
    je .main_system
    cmp rdi, 1
    je .main_display
    cmp rdi, 2
    je .main_memory
    cmp rdi, 3
    je .main_boot
    cmp rdi, 4
    je .main_storage
    cmp rdi, 5
    je .main_user
    cmp rdi, 6
    je .main_mui
    lea rsi, [rel item_about]
    mov rdx, item_about_end - item_about
    jmp draw_item
.main_system:
    lea rsi, [rel item_system]
    mov rdx, item_system_end - item_system
    jmp draw_item
.main_display:
    lea rsi, [rel item_display]
    mov rdx, item_display_end - item_display
    jmp draw_item
.main_memory:
    lea rsi, [rel item_memory]
    mov rdx, item_memory_end - item_memory
    jmp draw_item
.main_boot:
    lea rsi, [rel item_boot]
    mov rdx, item_boot_end - item_boot
    jmp draw_item
.main_storage:
    lea rsi, [rel item_storage]
    mov rdx, item_storage_end - item_storage
    jmp draw_item
.main_user:
    lea rsi, [rel item_user]
    mov rdx, item_user_end - item_user
    jmp draw_item
.main_mui:
    lea rsi, [rel item_mui]
    mov rdx, item_mui_end - item_mui
    jmp draw_item

.display:
    cmp rdi, 0
    je .display_auto
    cmp rdi, 1
    je .display_full
    cmp rdi, 2
    je .display_compact
    lea rsi, [rel option_display_tiny]
    mov rdx, option_display_tiny_end - option_display_tiny
    jmp draw_item
.display_auto:
    lea rsi, [rel option_display_auto]
    mov rdx, option_display_auto_end - option_display_auto
    jmp draw_item
.display_full:
    lea rsi, [rel option_display_full]
    mov rdx, option_display_full_end - option_display_full
    jmp draw_item
.display_compact:
    lea rsi, [rel option_display_compact]
    mov rdx, option_display_compact_end - option_display_compact
    jmp draw_item

redraw_status_only:
    cmp byte [rel page_mode], 1
    je .display
    mov rdi, 15
    jmp .draw
.display:
    mov rdi, 11
.draw:
    call write_cursor_row
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call draw_status
    ret

redraw_actions_only:
    cmp byte [rel page_mode], 1
    je .display
    mov rdi, 16
    jmp .draw
.display:
    mov rdi, 12
.draw:
    call write_cursor_row
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call draw_actions
    ret

redraw_action_focus:
    mov rdi, [rel selected_index]
    call draw_indexed_item
    call redraw_actions_only
    ret

draw_display_line:
    lea rsi, [rel display_prefix]
    mov rdx, display_prefix_end - display_prefix
    call write_stdout
    mov edi, [rel display_info]
    call print_u64
    lea rsi, [rel x_msg]
    mov rdx, x_msg_end - x_msg
    call write_stdout
    mov edi, [rel display_info + 4]
    call print_u64
    lea rsi, [rel display_detected_msg]
    mov rdx, display_detected_msg_end - display_detected_msg
    call write_stdout
    mov edi, [rel display_info + 8]
    call print_u64
    lea rsi, [rel x_msg]
    mov rdx, x_msg_end - x_msg
    call write_stdout
    mov edi, [rel display_info + 12]
    call print_u64
    call newline
    ret

draw_status:
    lea rsi, [rel status_prefix]
    mov rdx, status_prefix_end - status_prefix
    call write_stdout
    mov al, [rel status_code]
    cmp al, 1
    je .auto
    cmp al, 2
    je .full
    cmp al, 3
    je .compact
    cmp al, 4
    je .tiny
    cmp al, 5
    je .soon
    cmp al, 6
    je .failed
    cmp al, 8
    je .about
    cmp al, 9
    je .unknown
    cmp al, 10
    je .saved
    cmp al, 11
    je .system
    cmp al, 12
    je .cancel
    lea rsi, [rel status_ready]
    mov rdx, status_ready_end - status_ready
    jmp .write
.auto:
    lea rsi, [rel status_auto]
    mov rdx, status_auto_end - status_auto
    jmp .write
.full:
    lea rsi, [rel status_full]
    mov rdx, status_full_end - status_full
    jmp .write
.compact:
    lea rsi, [rel status_compact]
    mov rdx, status_compact_end - status_compact
    jmp .write
.tiny:
    lea rsi, [rel status_tiny]
    mov rdx, status_tiny_end - status_tiny
    jmp .write
.soon:
    lea rsi, [rel status_soon]
    mov rdx, status_soon_end - status_soon
    jmp .write
.failed:
    lea rsi, [rel status_failed]
    mov rdx, status_failed_end - status_failed
    jmp .write
.about:
    lea rsi, [rel status_about]
    mov rdx, status_about_end - status_about
    jmp .write
.unknown:
    lea rsi, [rel status_unknown]
    mov rdx, status_unknown_end - status_unknown
    jmp .write
.saved:
    lea rsi, [rel status_saved]
    mov rdx, status_saved_end - status_saved
    jmp .write
.system:
    lea rsi, [rel status_system]
    mov rdx, status_system_end - status_system
    jmp .write
.cancel:
    lea rsi, [rel status_cancel]
    mov rdx, status_cancel_end - status_cancel
.write:
    call write_stdout
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call newline
    ret

draw_actions:
    lea rsi, [rel action_prefix]
    mov rdx, action_prefix_end - action_prefix
    call write_stdout

    cmp byte [rel selected_action], 1
    jne .cancel_plain
    lea rsi, [rel cancel_action_selected_msg]
    mov rdx, cancel_action_selected_msg_end - cancel_action_selected_msg
    call write_stdout
    jmp .after_cancel
.cancel_plain:
    lea rsi, [rel cancel_action_msg]
    mov rdx, cancel_action_msg_end - cancel_action_msg
    call write_stdout
.after_cancel:
    lea rsi, [rel action_gap]
    mov rdx, action_gap_end - action_gap
    call write_stdout

    cmp byte [rel selected_action], 2
    jne .save_plain
    lea rsi, [rel save_action_selected_msg]
    mov rdx, save_action_selected_msg_end - save_action_selected_msg
    call write_stdout
    jmp .after_save
.save_plain:
    lea rsi, [rel save_action]
    mov rdx, save_action_end - save_action
    call write_stdout
.after_save:
    lea rsi, [rel action_gap]
    mov rdx, action_gap_end - action_gap
    call write_stdout

    cmp byte [rel selected_action], 3
    jne .exit_plain
    lea rsi, [rel exit_action_selected_msg]
    mov rdx, exit_action_selected_msg_end - exit_action_selected_msg
    call write_stdout
    jmp .done
.exit_plain:
    lea rsi, [rel exit_action]
    mov rdx, exit_action_end - exit_action
    call write_stdout
.done:
    lea rsi, [rel clear_eol_msg]
    mov rdx, clear_eol_msg_end - clear_eol_msg
    call write_stdout
    call newline
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

read_display_info:
    mov rax, AOS_SYS_DISPLAY_INFO
    lea rdi, [rel display_info]
    syscall
    test rax, rax
    jns .done
    mov dword [rel display_info], 80
    mov dword [rel display_info + 4], 25
    mov dword [rel display_info + 8], 80
    mov dword [rel display_info + 12], 25
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

section .bss
selected_index:
    resq 1
prev_index:
    resq 1
selected_action:
    resb 1
page_mode:
    resb 1
pending_display:
    resb 1
display_info:
    resb 24
key_buf:
    resb 1
status_code:
    resb 1
num_buf:
    resb 32
num_buf_end:

section .rodata
clear_msg:
    db 27, 'L'
clear_msg_end:
blue_color_msg:
    db 27, 'C', 0x1F
blue_color_msg_end:
panel_color_msg:
    db 27, 'C', 0x70
panel_color_msg_end:
highlight_color_msg:
    db 27, 'C', 0x4F
highlight_color_msg_end:
normal_color_msg:
    db 27, 'C', 0x0F
normal_color_msg_end:
inverse_msg:
    db 27, "[7m"
inverse_msg_end:
normal_msg:
    db 27, "[0m"
normal_msg_end:
clear_eol_msg:
    db 27, "[K"
clear_eol_msg_end:
cursor_row_msg:
    db 27, "[00;1H"
cursor_row_msg_end:
title_msg:
    db "AOS Software Configuration Tool (aos-config)"
title_msg_end:
display_prefix:
    db "Display "
display_prefix_end:
display_detected_msg:
    db " detected "
display_detected_msg_end:
x_msg:
    db "x"
x_msg_end:
box_top_msg:
    db "+----------------------------------------------------------------------------+", 10
box_top_msg_end:
box_bottom_msg:
    db "+----------------------------------------------------------------------------+", 10
box_bottom_msg_end:
selected_prefix:
    db "> "
selected_prefix_end:
plain_prefix:
    db "  "
plain_prefix_end:
item_display:
    db "2 Display Options        Configure display settings"
item_display_end:
item_system:
    db "1 System Options         Configure system settings"
item_system_end:
item_memory:
    db "3 Performance Options    Configure performance settings"
item_memory_end:
item_boot:
    db "4 Boot Options           Configure boot settings"
item_boot_end:
item_storage:
    db "5 Storage Options        Configure disks and mounts"
item_storage_end:
item_user:
    db "6 User Options           Configure users and sudo"
item_user_end:
item_mui:
    db "7 MUI Options            Configure interface settings"
item_mui_end:
item_about:
    db "8 About aos-config       Information about this tool"
item_about_end:
status_prefix:
    db " Status: "
status_prefix_end:
status_ready:
    db "ready"
status_ready_end:
status_auto:
    db "selected auto"
status_auto_end:
status_full:
    db "selected 80x25"
status_full_end:
status_compact:
    db "selected 60x20"
status_compact_end:
status_tiny:
    db "selected 40x15"
status_tiny_end:
status_soon:
    db "not available yet"
status_soon_end:
status_failed:
    db "display mode rejected"
status_failed_end:
status_about:
    db "AOS settings"
status_about_end:
status_unknown:
    db "unknown key"
status_unknown_end:
status_saved:
    db "saved"
status_saved_end:
status_system:
    db "not available yet"
status_system_end:
status_cancel:
    db "action canceled"
status_cancel_end:
action_prefix:
    db " Actions: "
action_prefix_end:
cancel_action_msg:
    db "[ Cancel ]"
cancel_action_msg_end:
cancel_action_selected_msg:
    db "> Cancel <"
cancel_action_selected_msg_end:
save_action:
    db "[ Save ]"
save_action_end:
save_action_selected_msg:
    db "> Save <"
save_action_selected_msg_end:
action_gap:
    db "   "
action_gap_end:
exit_action:
    db "[ Exit ]"
exit_action_end:
exit_action_selected_msg:
    db "> Exit <"
exit_action_selected_msg_end:
help_msg:
    db "Up/Down: select  Left/Right: actions  Enter: choose  q: quit", 10
help_msg_end:
main_label_msg:
    db "Settings", 10
main_label_msg_end:
display_label_msg:
    db "Display Options", 10
display_label_msg_end:
option_display_auto:
    db "Auto detect"
option_display_auto_end:
option_display_full:
    db "80x25"
option_display_full_end:
option_display_compact:
    db "60x20"
option_display_compact_end:
option_display_tiny:
    db "40x15"
option_display_tiny_end:
newline_msg:
    db 10
