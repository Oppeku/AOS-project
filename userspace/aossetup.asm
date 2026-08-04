; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_MKDIR 83
%define SYS_EXIT 60

_start:
    lea rsi, [rel start_msg]
    mov rdx, start_msg_end - start_msg
    call write_stdout

    lea r12, [rel layout_paths]

.next_path:
    mov rdi, [r12]
    test rdi, rdi
    jz .done

    mov rax, SYS_MKDIR
    mov rsi, 0755o
    syscall

    add r12, 8
    jmp .next_path

.done:
    lea rsi, [rel done_msg]
    mov rdx, done_msg_end - done_msg
    call write_stdout

    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .rodata
start_msg:
    db "AOS setup: creating default filesystem layout", 10
start_msg_end:

done_msg:
    db "AOS setup: done", 10
done_msg_end:

layout_paths:
    dq path_bootloader
    dq path_bootloader_config
    dq path_commands
    dq path_kernel
    dq path_kernel_modules
    dq path_drivers
    dq path_drivers_video
    dq path_drivers_keyboard
    dq path_drivers_mouse
    dq path_drivers_controlers
    dq path_drivers_fingerprint
    dq path_networking_stack
    dq path_saved_wifi_config
    dq path_network_drivers
    dq path_netshell
    dq path_network_commands
    dq path_bluetooth
    dq path_etc
    dq path_tmp
    dq path_main
    dq path_main_desktop
    dq path_main_music
    dq path_main_photos
    dq path_main_video
    dq path_main_mui
    dq path_main_starred
    dq path_main_trash
    dq path_home
    dq path_legacy_home
    dq 0

path_bootloader: db "/bootloader", 0
path_bootloader_config: db "/bootloader/config", 0
path_commands: db "/commands", 0
path_kernel: db "/kernel", 0
path_kernel_modules: db "/kernel/modules", 0
path_drivers: db "/drivers", 0
path_drivers_video: db "/drivers/video", 0
path_drivers_keyboard: db "/drivers/Keyboard", 0
path_drivers_mouse: db "/drivers/mouse", 0
path_drivers_controlers: db "/drivers/Controlers", 0
path_drivers_fingerprint: db "/drivers/fingerprint-scanner", 0
path_networking_stack: db "/networking-stack", 0
path_saved_wifi_config: db "/networking-stack/Saved_Wifi_config", 0
path_network_drivers: db "/networking-stack/drivers", 0
path_netshell: db "/networking-stack/NetShell", 0
path_network_commands: db "/networking-stack/Commands", 0
path_bluetooth: db "/Bluetooth", 0
path_etc: db "/etc", 0
path_tmp: db "/tmp", 0
path_main: db "/main", 0
path_main_desktop: db "/main/Desktop", 0
path_main_music: db "/main/Music", 0
path_main_photos: db "/main/Photos", 0
path_main_video: db "/main/Video", 0
path_main_mui: db "/main/MUI", 0
path_main_starred: db "/main/Starred", 0
path_main_trash: db "/main/Trash", 0
path_home: db "/home", 0
path_legacy_home: db "/home/explorer", 0
