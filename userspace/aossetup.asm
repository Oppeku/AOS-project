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
    dq path_boot
    dq path_boot_loader
    dq path_boot_configs
    dq path_boot_logs
    dq path_kernel
    dq path_kernel_modules
    dq path_kernel_memory
    dq path_kernel_syscalls
    dq path_drivers
    dq path_drivers_display
    dq path_drivers_storage
    dq path_drivers_input
    dq path_mui
    dq path_mui_core
    dq path_mui_shell
    dq path_mui_themes
    dq path_mui_apps
    dq path_sudo
    dq path_sudo_permissions
    dq path_sudo_policies
    dq path_configs
    dq path_configs_system
    dq path_configs_users
    dq path_configs_network
    dq path_configs_mui
    dq path_memory
    dq path_memory_optimization
    dq path_memory_cache
    dq path_memory_swapd
    dq path_squash
    dq path_squash_packages
    dq path_squash_fallback
    dq path_services
    dq path_services_networkd
    dq path_services_logind
    dq path_services_powerd
    dq path_runtime
    dq path_runtime_temp
    dq path_runtime_locks
    dq path_runtime_sessions
    dq path_logs
    dq path_logs_kernel
    dq path_logs_boot
    dq path_logs_system
    dq path_etc
    dq path_tmp
    dq path_main
    dq path_main_oppeko
    dq path_main_oppeko_apps
    dq path_main_oppeko_downloads
    dq path_main_oppeko_documents
    dq path_main_oppeko_projects
    dq path_main_oppeko_configs
    dq path_main_oppeko_cache
    dq path_main_oppeko_workspace
    dq path_trash
    dq 0

path_boot: db "/boot", 0
path_boot_loader: db "/boot/loader", 0
path_boot_configs: db "/boot/configs", 0
path_boot_emergency: db "/boot/emergency", 0
path_boot_logs: db "/boot/logs", 0
path_kernel: db "/kernel", 0
path_kernel_modules: db "/kernel/modules", 0
path_kernel_scheduler: db "/kernel/scheduler", 0
path_kernel_memory: db "/kernel/memory", 0
path_kernel_interrupts: db "/kernel/interrupts", 0
path_kernel_syscalls: db "/kernel/syscalls", 0
path_drivers: db "/drivers", 0
path_drivers_audio: db "/drivers/audio", 0
path_drivers_display: db "/drivers/display", 0
path_drivers_network: db "/drivers/network", 0
path_drivers_storage: db "/drivers/storage", 0
path_drivers_usb: db "/drivers/usb", 0
path_drivers_bluetooth: db "/drivers/bluetooth", 0
path_drivers_input: db "/drivers/input", 0
path_mui: db "/MUI", 0
path_mui_core: db "/MUI/core", 0
path_mui_shell: db "/MUI/shell", 0
path_mui_themes: db "/MUI/themes", 0
path_mui_fonts: db "/MUI/fonts", 0
path_mui_sounds: db "/MUI/sounds", 0
path_mui_login: db "/MUI/login", 0
path_mui_widgets: db "/MUI/widgets", 0
path_mui_settings: db "/MUI/settings", 0
path_mui_apps: db "/MUI/apps", 0
path_sudo: db "/sudo", 0
path_sudo_permissions: db "/sudo/permissions", 0
path_sudo_policies: db "/sudo/policies", 0
path_sudo_groups: db "/sudo/groups", 0
path_sudo_auth: db "/sudo/auth", 0
path_configs: db "/configs", 0
path_configs_system: db "/configs/system", 0
path_configs_users: db "/configs/users", 0
path_configs_network: db "/configs/network", 0
path_configs_mui: db "/configs/mui", 0
path_memory: db "/memory", 0
path_memory_optimization: db "/memory/optimization", 0
path_memory_cache: db "/memory/cache", 0
path_memory_swapd: db "/memory/swapd", 0
path_memory_cleaner: db "/memory/cleaner", 0
path_memory_compression: db "/memory/compression", 0
path_squash: db "/squash", 0
path_squash_base: db "/squash/base", 0
path_squash_readonly: db "/squash/readonly", 0
path_squash_packages: db "/squash/packages", 0
path_squash_fallback: db "/squash/fallback", 0
path_services: db "/services", 0
path_services_networkd: db "/services/networkd", 0
path_services_audiod: db "/services/audiod", 0
path_services_logind: db "/services/logind", 0
path_services_updated: db "/services/updated", 0
path_services_powerd: db "/services/powerd", 0
path_runtime: db "/runtime", 0
path_runtime_temp: db "/runtime/temp", 0
path_runtime_locks: db "/runtime/locks", 0
path_runtime_sessions: db "/runtime/sessions", 0
path_logs: db "/logs", 0
path_logs_kernel: db "/logs/kernel", 0
path_logs_boot: db "/logs/boot", 0
path_logs_crashes: db "/logs/crashes", 0
path_logs_system: db "/logs/system", 0
path_etc: db "/etc", 0
path_tmp: db "/tmp", 0
path_main: db "/main", 0
path_main_oppeko: db "/main/oppeko", 0
path_main_oppeko_apps: db "/main/oppeko/apps", 0
path_main_oppeko_downloads: db "/main/oppeko/downloads", 0
path_main_oppeko_documents: db "/main/oppeko/documents", 0
path_main_oppeko_projects: db "/main/oppeko/projects", 0
path_main_oppeko_configs: db "/main/oppeko/configs", 0
path_main_oppeko_cache: db "/main/oppeko/cache", 0
path_main_oppeko_workspace: db "/main/oppeko/workspace", 0
path_trash: db "/trash", 0
path_trash_oppeko: db "/trash/oppeko", 0
path_trash_oppeko_files: db "/trash/oppeko/files", 0
path_trash_oppeko_metadata: db "/trash/oppeko/metadata", 0
path_trash_oppeko_restore: db "/trash/oppeko/restore", 0
path_trash_oppeko_auto_clean: db "/trash/oppeko/auto_clean", 0
