; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_PCI_INFO 518
%define AOS_SYS_FIRMWARE_INFO 529
%define AOS_SYS_WIFI_SCAN_INFO 530
%define AOS_SYS_WIFI_STATE_INFO 536
%define AOS_SYS_WIFI_CONTROL 537

%define PCI_VENDOR_OFF 0
%define PCI_DEVICE_OFF 2
%define PCI_BUS_OFF 4
%define PCI_SLOT_OFF 5
%define PCI_FUNC_OFF 6
%define PCI_CLASS_OFF 7
%define PCI_SUBCLASS_OFF 8
%define PCI_PROGIF_OFF 9
%define PCI_IRQ_OFF 12

%define FW_NAME_OFF 0

%define SCAN_SSID_OFF 0
%define SCAN_BSSID_OFF 32
%define SCAN_CHANNEL_OFF 38
%define SCAN_RSSI_OFF 39
%define SCAN_SECURITY_OFF 40

%define STATE_STATE_OFF 0
%define STATE_SCAN_COUNT_OFF 1
%define STATE_SELECTED_OFF 2
%define STATE_SECURITY_OFF 3
%define STATE_SSID_OFF 4
%define STATE_BSSID_OFF 36
%define STATE_CHANNEL_OFF 42
%define STATE_RSSI_OFF 43
%define STATE_AUTH_ATTEMPTS_OFF 48
%define STATE_ASSOC_ATTEMPTS_OFF 52
%define STATE_RX_MGMT_OFF 56
%define STATE_RX_BEACON_OFF 60
%define STATE_RX_PROBE_RESP_OFF 64
%define STATE_RX_DATA_OFF 68
%define STATE_RX_OTHER_OFF 72
%define STATE_TX_MGMT_OFF 76
%define STATE_TX_PROBE_REQ_OFF 80
%define STATE_LAST_TX_LEN_OFF 84
%define STATE_DRIVER_TX_CALLS_OFF 88
%define STATE_DRIVER_TX_HW_OFF 92
%define STATE_DRIVER_TX_SIM_OFF 96
%define STATE_DRIVER_TX_ERRORS_OFF 100
%define STATE_DRIVER_RX_CALLS_OFF 104
%define STATE_DRIVER_RX_ACCEPTED_OFF 108
%define STATE_DRIVER_RX_ERRORS_OFF 112
%define STATE_DRIVER_RX_LAST_LEN_OFF 116
%define STATE_DRIVER_RX_LAST_RSSI_OFF 120

_start:
    mov rax, [rsp]
    cmp rax, 2
    jb .show_status
    mov rsi, [rsp + 16]
    lea rdi, [rel scan_arg]
    call cstr_equals
    test eax, eax
    jnz scan_command
    mov rsi, [rsp + 16]
    lea rdi, [rel connect_arg]
    call cstr_equals
    test eax, eax
    jnz connect_command
    mov rsi, [rsp + 16]
    lea rdi, [rel probe_arg]
    call cstr_equals
    test eax, eax
    jnz probe_command
    mov rsi, [rsp + 16]
    lea rdi, [rel rxprobe_arg]
    call cstr_equals
    test eax, eax
    jnz rxprobe_command

.show_status:
    lea rsi, [rel title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    xor r12, r12
    xor r13, r13

.loop:
    mov rax, AOS_SYS_PCI_INFO
    mov rdi, r12
    lea rsi, [rel pci_buf]
    syscall
    test rax, rax
    js .done

    cmp byte [rel pci_buf + PCI_CLASS_OFF], 0x02
    jne .next
    cmp byte [rel pci_buf + PCI_SUBCLASS_OFF], 0x80
    jne .next

    inc r13
    lea rsi, [rel device_msg]
    mov rdx, device_msg_end - device_msg
    call write_stdout

    movzx edi, byte [rel pci_buf + PCI_BUS_OFF]
    call write_hex8
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rel pci_buf + PCI_SLOT_OFF]
    call write_hex8
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rel pci_buf + PCI_FUNC_OFF]
    call write_hex_nibble

    lea rsi, [rel vendor_msg]
    mov rdx, vendor_msg_end - vendor_msg
    call write_stdout
    movzx edi, word [rel pci_buf + PCI_VENDOR_OFF]
    call write_hex16
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, word [rel pci_buf + PCI_DEVICE_OFF]
    call write_hex16

    lea rsi, [rel class_msg]
    mov rdx, class_msg_end - class_msg
    call write_stdout
    movzx edi, byte [rel pci_buf + PCI_CLASS_OFF]
    call write_hex8
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rel pci_buf + PCI_SUBCLASS_OFF]
    call write_hex8
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rel pci_buf + PCI_PROGIF_OFF]
    call write_hex8

    lea rsi, [rel status_msg]
    mov rdx, status_msg_end - status_msg
    call write_stdout

    movzx edi, word [rel pci_buf + PCI_VENDOR_OFF]
    call write_vendor_driver

.next:
    inc r12
    jmp .loop

.done:
    test r13, r13
    jnz .print_fw_status
    lea rsi, [rel no_wifi_msg]
    mov rdx, no_wifi_msg_end - no_wifi_msg
    call write_stdout
    lea rsi, [rel next_msg]
    mov rdx, next_msg_end - next_msg
    call write_stdout

    lea rsi, [rel roadmap_msg]
    mov rdx, roadmap_msg_end - roadmap_msg
    call write_stdout
    lea rsi, [rel mac80211_msg]
    mov rdx, mac80211_msg_end - mac80211_msg
    call write_stdout

.print_fw_status:
    call print_firmware_status
    call print_wifi_state

.exit:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

scan_command:
    lea rsi, [rel scan_header_msg]
    mov rdx, scan_header_msg_end - scan_header_msg
    call write_stdout

    xor r12, r12
.scan_loop:
    mov rax, AOS_SYS_WIFI_SCAN_INFO
    mov rdi, r12
    lea rsi, [rel scan_buf]
    syscall
    test rax, rax
    js .scan_done

    lea rsi, [rel scan_buf + SCAN_SSID_OFF]
    mov rdx, 18
    call write_padded_cstr
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    lea rbx, [rel scan_buf + SCAN_BSSID_OFF]
    call write_bssid_from_rbx
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    movzx edi, byte [rel scan_buf + SCAN_CHANNEL_OFF]
    call write_u64
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    movsx edi, byte [rel scan_buf + SCAN_RSSI_OFF]
    call write_i64
    lea rsi, [rel dbm_msg]
    mov rdx, dbm_msg_end - dbm_msg
    call write_stdout
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    movzx edi, byte [rel scan_buf + SCAN_SECURITY_OFF]
    call write_security
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    inc r12
    jmp .scan_loop

.scan_done:
    test r12, r12
    jnz .scan_exit
    lea rsi, [rel scan_empty_msg]
    mov rdx, scan_empty_msg_end - scan_empty_msg
    call write_stdout
.scan_exit:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

connect_command:
    lea rsi, [rel connect_start_msg]
    mov rdx, connect_start_msg_end - connect_start_msg
    call write_stdout

    xor r12, r12
    mov rax, [rsp]
    cmp rax, 3
    jb .have_target

    mov rsi, [rsp + 24]
    call find_scan_target
    test eax, eax
    js .not_found
    mov r12d, eax

.have_target:
    mov rax, AOS_SYS_WIFI_CONTROL
    mov rdi, 4
    mov rsi, r12
    syscall
    test rax, rax
    js .failed

    lea rsi, [rel connect_ok_msg]
    mov rdx, connect_ok_msg_end - connect_ok_msg
    call write_stdout
    call print_wifi_state
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.not_found:
    lea rsi, [rel connect_not_found_msg]
    mov rdx, connect_not_found_msg_end - connect_not_found_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

.failed:
    lea rsi, [rel connect_failed_msg]
    mov rdx, connect_failed_msg_end - connect_failed_msg
    call write_stdout
    call print_wifi_state
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

probe_command:
    lea rsi, [rel probe_start_msg]
    mov rdx, probe_start_msg_end - probe_start_msg
    call write_stdout

    mov rax, AOS_SYS_WIFI_CONTROL
    mov rdi, 5
    xor rsi, rsi
    syscall
    test rax, rax
    js .probe_failed

    lea rsi, [rel probe_ok_msg]
    mov rdx, probe_ok_msg_end - probe_ok_msg
    call write_stdout
    call print_wifi_state
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.probe_failed:
    lea rsi, [rel probe_failed_msg]
    mov rdx, probe_failed_msg_end - probe_failed_msg
    call write_stdout
    call print_wifi_state
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

rxprobe_command:
    lea rsi, [rel rxprobe_start_msg]
    mov rdx, rxprobe_start_msg_end - rxprobe_start_msg
    call write_stdout

    mov rax, AOS_SYS_WIFI_CONTROL
    mov rdi, 6
    xor rsi, rsi
    syscall
    test rax, rax
    js .rxprobe_failed

    lea rsi, [rel rxprobe_ok_msg]
    mov rdx, rxprobe_ok_msg_end - rxprobe_ok_msg
    call write_stdout
    call print_wifi_state
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

.rxprobe_failed:
    lea rsi, [rel rxprobe_failed_msg]
    mov rdx, rxprobe_failed_msg_end - rxprobe_failed_msg
    call write_stdout
    call print_wifi_state
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

print_firmware_status:
    lea rsi, [rel fw_status_msg]
    mov rdx, fw_status_msg_end - fw_status_msg
    call write_stdout

    lea rsi, [rel fw_intel_label]
    mov rdx, fw_intel_label_end - fw_intel_label
    lea rdi, [rel fw_intel_path]
    mov r8, fw_intel_path_end - fw_intel_path
    call print_fw_target

    lea rsi, [rel fw_realtek_label]
    mov rdx, fw_realtek_label_end - fw_realtek_label
    lea rdi, [rel fw_realtek_path]
    mov r8, fw_realtek_path_end - fw_realtek_path
    call print_fw_target

    lea rsi, [rel fw_broadcom_label]
    mov rdx, fw_broadcom_label_end - fw_broadcom_label
    lea rdi, [rel fw_broadcom_path]
    mov r8, fw_broadcom_path_end - fw_broadcom_path
    call print_fw_target

    lea rsi, [rel fw_atheros_label]
    mov rdx, fw_atheros_label_end - fw_atheros_label
    lea rdi, [rel fw_atheros_path]
    mov r8, fw_atheros_path_end - fw_atheros_path
    call print_fw_target

    lea rsi, [rel fw_ralink_label]
    mov rdx, fw_ralink_label_end - fw_ralink_label
    lea rdi, [rel fw_ralink_path]
    mov r8, fw_ralink_path_end - fw_ralink_path
    call print_fw_target

    lea rsi, [rel fw_mediatek_label]
    mov rdx, fw_mediatek_label_end - fw_mediatek_label
    lea rdi, [rel fw_mediatek_path]
    mov r8, fw_mediatek_path_end - fw_mediatek_path
    call print_fw_target
    ret

print_wifi_state:
    mov rax, AOS_SYS_WIFI_STATE_INFO
    xor rdi, rdi
    lea rsi, [rel state_buf]
    syscall
    test rax, rax
    js .state_failed

    lea rsi, [rel state_title_msg]
    mov rdx, state_title_msg_end - state_title_msg
    call write_stdout

    lea rsi, [rel state_state_msg]
    mov rdx, state_state_msg_end - state_state_msg
    call write_stdout
    movzx edi, byte [rel state_buf + STATE_STATE_OFF]
    call write_wifi_state
    lea rsi, [rel state_scan_msg]
    mov rdx, state_scan_msg_end - state_scan_msg
    call write_stdout
    movzx edi, byte [rel state_buf + STATE_SCAN_COUNT_OFF]
    call write_u64
    lea rsi, [rel state_selected_msg]
    mov rdx, state_selected_msg_end - state_selected_msg
    call write_stdout
    movzx edi, byte [rel state_buf + STATE_SELECTED_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel state_network_msg]
    mov rdx, state_network_msg_end - state_network_msg
    call write_stdout
    lea rsi, [rel state_buf + STATE_SSID_OFF]
    mov rdx, 32
    call write_cstr_limited
    lea rsi, [rel state_bssid_msg]
    mov rdx, state_bssid_msg_end - state_bssid_msg
    call write_stdout
    lea rbx, [rel state_buf + STATE_BSSID_OFF]
    call write_bssid_from_rbx
    lea rsi, [rel state_channel_msg]
    mov rdx, state_channel_msg_end - state_channel_msg
    call write_stdout
    movzx edi, byte [rel state_buf + STATE_CHANNEL_OFF]
    call write_u64
    lea rsi, [rel state_rssi_msg]
    mov rdx, state_rssi_msg_end - state_rssi_msg
    call write_stdout
    movsx edi, byte [rel state_buf + STATE_RSSI_OFF]
    call write_i64
    lea rsi, [rel dbm_msg]
    mov rdx, dbm_msg_end - dbm_msg
    call write_stdout
    lea rsi, [rel state_security_msg]
    mov rdx, state_security_msg_end - state_security_msg
    call write_stdout
    movzx edi, byte [rel state_buf + STATE_SECURITY_OFF]
    call write_security
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel state_rx_msg]
    mov rdx, state_rx_msg_end - state_rx_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_RX_MGMT_OFF]
    call write_u64
    lea rsi, [rel state_beacon_msg]
    mov rdx, state_beacon_msg_end - state_beacon_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_RX_BEACON_OFF]
    call write_u64
    lea rsi, [rel state_probe_msg]
    mov rdx, state_probe_msg_end - state_probe_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_RX_PROBE_RESP_OFF]
    call write_u64
    lea rsi, [rel state_data_msg]
    mov rdx, state_data_msg_end - state_data_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_RX_DATA_OFF]
    call write_u64
    lea rsi, [rel state_other_msg]
    mov rdx, state_other_msg_end - state_other_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_RX_OTHER_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel state_tx_msg]
    mov rdx, state_tx_msg_end - state_tx_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_TX_MGMT_OFF]
    call write_u64
    lea rsi, [rel state_tx_probe_msg]
    mov rdx, state_tx_probe_msg_end - state_tx_probe_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_TX_PROBE_REQ_OFF]
    call write_u64
    lea rsi, [rel state_last_tx_msg]
    mov rdx, state_last_tx_msg_end - state_last_tx_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_LAST_TX_LEN_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel state_driver_tx_msg]
    mov rdx, state_driver_tx_msg_end - state_driver_tx_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_TX_CALLS_OFF]
    call write_u64
    lea rsi, [rel state_driver_hw_msg]
    mov rdx, state_driver_hw_msg_end - state_driver_hw_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_TX_HW_OFF]
    call write_u64
    lea rsi, [rel state_driver_sim_msg]
    mov rdx, state_driver_sim_msg_end - state_driver_sim_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_TX_SIM_OFF]
    call write_u64
    lea rsi, [rel state_driver_err_msg]
    mov rdx, state_driver_err_msg_end - state_driver_err_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_TX_ERRORS_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel state_driver_rx_msg]
    mov rdx, state_driver_rx_msg_end - state_driver_rx_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_RX_CALLS_OFF]
    call write_u64
    lea rsi, [rel state_driver_rx_accept_msg]
    mov rdx, state_driver_rx_accept_msg_end - state_driver_rx_accept_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_RX_ACCEPTED_OFF]
    call write_u64
    lea rsi, [rel state_driver_rx_err_msg]
    mov rdx, state_driver_rx_err_msg_end - state_driver_rx_err_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_RX_ERRORS_OFF]
    call write_u64
    lea rsi, [rel state_driver_rx_len_msg]
    mov rdx, state_driver_rx_len_msg_end - state_driver_rx_len_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_DRIVER_RX_LAST_LEN_OFF]
    call write_u64
    lea rsi, [rel state_driver_rx_rssi_msg]
    mov rdx, state_driver_rx_rssi_msg_end - state_driver_rx_rssi_msg
    call write_stdout
    movsx edi, byte [rel state_buf + STATE_DRIVER_RX_LAST_RSSI_OFF]
    call write_i64
    lea rsi, [rel dbm_msg]
    mov rdx, dbm_msg_end - dbm_msg
    call write_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel state_auth_msg]
    mov rdx, state_auth_msg_end - state_auth_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_AUTH_ATTEMPTS_OFF]
    call write_u64
    lea rsi, [rel state_assoc_msg]
    mov rdx, state_assoc_msg_end - state_assoc_msg
    call write_stdout
    mov edi, [rel state_buf + STATE_ASSOC_ATTEMPTS_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    ret

.state_failed:
    lea rsi, [rel state_failed_msg]
    mov rdx, state_failed_msg_end - state_failed_msg
    jmp write_stdout

print_fw_target:
    mov r12, rsi
    mov r13, rdx
    mov r14, rdi
    mov r15, r8

    mov rsi, r12
    mov rdx, r13
    call write_stdout
    mov rsi, r14
    mov rdx, r15
    call write_stdout
    lea rsi, [rel fw_arrow_msg]
    mov rdx, fw_arrow_msg_end - fw_arrow_msg
    call write_stdout

    mov rdi, r14
    mov rsi, r15
    call firmware_exists
    test eax, eax
    jz .missing
    lea rsi, [rel fw_found_msg]
    mov rdx, fw_found_msg_end - fw_found_msg
    jmp write_stdout
.missing:
    lea rsi, [rel fw_missing_msg]
    mov rdx, fw_missing_msg_end - fw_missing_msg
    jmp write_stdout

firmware_exists:
    mov [rel target_ptr], rdi
    mov [rel target_len], rsi
    xor r12, r12
.fw_loop:
    mov rax, AOS_SYS_FIRMWARE_INFO
    mov rdi, r12
    lea rsi, [rel fw_buf]
    syscall
    test rax, rax
    js .fw_not_found

    lea rdi, [rel fw_buf + FW_NAME_OFF]
    mov rsi, [rel target_ptr]
    mov rdx, [rel target_len]
    call name_equals_len
    test eax, eax
    jnz .fw_found

    inc r12
    jmp .fw_loop
.fw_found:
    mov eax, 1
    ret
.fw_not_found:
    xor eax, eax
    ret

name_equals_len:
    xor rcx, rcx
.cmp_loop:
    cmp rcx, rdx
    jae .check_end
    mov al, [rdi + rcx]
    cmp al, [rsi + rcx]
    jne .not_equal
    inc rcx
    jmp .cmp_loop
.check_end:
    cmp byte [rdi + rcx], 0
    jne .not_equal
    mov eax, 1
    ret
.not_equal:
    xor eax, eax
    ret

find_scan_target:
    mov r15, rsi
    mov al, [r15]
    cmp al, '0'
    jb .by_name
    cmp al, '9'
    ja .by_name
    cmp byte [r15 + 1], 0
    jne .by_name
    sub al, '0'
    movzx eax, al
    ret

.by_name:
    xor r12, r12
.find_loop:
    mov rax, AOS_SYS_WIFI_SCAN_INFO
    mov rdi, r12
    lea rsi, [rel scan_buf]
    syscall
    test rax, rax
    js .find_missing

    lea rdi, [rel scan_buf + SCAN_SSID_OFF]
    mov rsi, r15
    call cstr_iequals
    test eax, eax
    jnz .find_hit

    inc r12
    jmp .find_loop

.find_hit:
    mov eax, r12d
    ret

.find_missing:
    mov eax, -1
    ret

cstr_equals:
    mov al, [rdi]
    cmp al, [rsi]
    jne .cstr_no
    test al, al
    jz .cstr_yes
    inc rdi
    inc rsi
    jmp cstr_equals
.cstr_yes:
    mov eax, 1
    ret
.cstr_no:
    xor eax, eax
    ret

cstr_iequals:
    mov al, [rdi]
    mov dl, [rsi]
    call ascii_lower_pair
    cmp al, dl
    jne .no
    test al, al
    jz .yes
    inc rdi
    inc rsi
    jmp cstr_iequals
.yes:
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

ascii_lower_pair:
    cmp al, 'A'
    jb .check_dl
    cmp al, 'Z'
    ja .check_dl
    add al, 32
.check_dl:
    cmp dl, 'A'
    jb .done
    cmp dl, 'Z'
    ja .done
    add dl, 32
.done:
    ret

write_vendor_driver:
    cmp di, 0x8086
    je .intel
    cmp di, 0x10EC
    je .realtek
    cmp di, 0x14E4
    je .broadcom
    cmp di, 0x168C
    je .atheros
    cmp di, 0x1814
    je .ralink
    cmp di, 0x14C3
    je .mediatek
    lea rsi, [rel driver_unknown_msg]
    mov rdx, driver_unknown_msg_end - driver_unknown_msg
    jmp write_stdout
.intel:
    lea rsi, [rel driver_intel_msg]
    mov rdx, driver_intel_msg_end - driver_intel_msg
    jmp write_stdout
.realtek:
    lea rsi, [rel driver_realtek_msg]
    mov rdx, driver_realtek_msg_end - driver_realtek_msg
    jmp write_stdout
.broadcom:
    lea rsi, [rel driver_broadcom_msg]
    mov rdx, driver_broadcom_msg_end - driver_broadcom_msg
    jmp write_stdout
.atheros:
    lea rsi, [rel driver_atheros_msg]
    mov rdx, driver_atheros_msg_end - driver_atheros_msg
    jmp write_stdout
.ralink:
    lea rsi, [rel driver_ralink_msg]
    mov rdx, driver_ralink_msg_end - driver_ralink_msg
    jmp write_stdout
.mediatek:
    lea rsi, [rel driver_mediatek_msg]
    mov rdx, driver_mediatek_msg_end - driver_mediatek_msg
    jmp write_stdout

write_hex_nibble:
    and edi, 0xF
    lea rsi, [rel hex_digits]
    mov al, [rsi + rdi]
    mov [rel one_char], al
    lea rsi, [rel one_char]
    mov rdx, 1
    jmp write_stdout

write_hex8:
    push rdi
    shr edi, 4
    call write_hex_nibble
    pop rdi
    call write_hex_nibble
    ret

write_hex16:
    push rdi
    shr edi, 8
    call write_hex8
    pop rdi
    call write_hex8
    ret

write_bssid_from_rbx:
    movzx edi, byte [rbx]
    call write_hex8
    mov r14, 1
.bssid_loop:
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rbx + r14]
    push r14
    call write_hex8
    pop r14
    inc r14
    cmp r14, 6
    jb .bssid_loop
    ret

write_padded_cstr:
    push rdx
    push rsi
    call cstrnlen
    mov r13, rax
    pop rsi
    push rsi
    mov rdx, r13
    call write_stdout
    pop rsi
    pop rdx
    sub rdx, r13
    jbe .pad_done
.pad_loop:
    push rdx
    lea rsi, [rel space_msg]
    mov rdx, 1
    call write_stdout
    pop rdx
    dec rdx
    jnz .pad_loop
.pad_done:
    ret

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

write_cstr_limited:
    push rsi
    call cstrnlen
    mov rdx, rax
    pop rsi
    jmp write_stdout

write_i64:
    test edi, edi
    jns write_u64
    push rdi
    lea rsi, [rel minus_msg]
    mov rdx, minus_msg_end - minus_msg
    call write_stdout
    pop rdi
    neg edi
    jmp write_u64

write_u64:
    lea rsi, [rel num_buf + 20]
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
    lea rdx, [rel num_buf + 20]
    sub rdx, rsi
    jmp write_stdout

write_security:
    cmp dil, 0
    je .open
    cmp dil, 1
    je .wpa2
    cmp dil, 2
    je .wpa3
    lea rsi, [rel sec_unknown_msg]
    mov rdx, sec_unknown_msg_end - sec_unknown_msg
    jmp write_stdout
.open:
    lea rsi, [rel sec_open_msg]
    mov rdx, sec_open_msg_end - sec_open_msg
    jmp write_stdout
.wpa2:
    lea rsi, [rel sec_wpa2_msg]
    mov rdx, sec_wpa2_msg_end - sec_wpa2_msg
    jmp write_stdout
.wpa3:
    lea rsi, [rel sec_wpa3_msg]
    mov rdx, sec_wpa3_msg_end - sec_wpa3_msg
    jmp write_stdout

write_wifi_state:
    cmp dil, 0
    je .down
    cmp dil, 1
    je .scanning
    cmp dil, 2
    je .scanned
    cmp dil, 3
    je .authing
    cmp dil, 4
    je .authed
    cmp dil, 5
    je .associng
    cmp dil, 6
    je .associated
    lea rsi, [rel state_unknown_msg]
    mov rdx, state_unknown_msg_end - state_unknown_msg
    jmp write_stdout
.down:
    lea rsi, [rel state_down_msg]
    mov rdx, state_down_msg_end - state_down_msg
    jmp write_stdout
.scanning:
    lea rsi, [rel state_scanning_msg]
    mov rdx, state_scanning_msg_end - state_scanning_msg
    jmp write_stdout
.scanned:
    lea rsi, [rel state_scanned_msg]
    mov rdx, state_scanned_msg_end - state_scanned_msg
    jmp write_stdout
.authing:
    lea rsi, [rel state_authing_msg]
    mov rdx, state_authing_msg_end - state_authing_msg
    jmp write_stdout
.authed:
    lea rsi, [rel state_authed_msg]
    mov rdx, state_authed_msg_end - state_authed_msg
    jmp write_stdout
.associng:
    lea rsi, [rel state_associng_msg]
    mov rdx, state_associng_msg_end - state_associng_msg
    jmp write_stdout
.associated:
    lea rsi, [rel state_associated_msg]
    mov rdx, state_associated_msg_end - state_associated_msg
    jmp write_stdout

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
pci_buf:
    resb 64
fw_buf:
    resb 104
scan_buf:
    resb 48
state_buf:
    resb 124
target_ptr:
    resq 1
target_len:
    resq 1
num_buf:
    resb 21
one_char:
    resb 1

section .rodata
scan_arg:
    db "scan", 0

connect_arg:
    db "connect", 0

probe_arg:
    db "probe", 0

rxprobe_arg:
    db "rxprobe", 0

title_msg:
    db "AOS WiFi", 10
    db "--------", 10
title_msg_end:

scan_header_msg:
    db "AOS WiFi scan cache", 10
    db "SSID                BSSID              CH  RSSI  SECURITY", 10
    db "---------------------------------------------------------", 10
scan_header_msg_end:

scan_empty_msg:
    db "wifi scan: no networks cached", 10
scan_empty_msg_end:

connect_start_msg:
    db "wifi: sending auth and association frames through mac80211", 10
connect_start_msg_end:

connect_ok_msg:
    db "wifi: auth and association completed", 10
connect_ok_msg_end:

connect_not_found_msg:
    db "wifi connect: network not found in scan cache", 10
connect_not_found_msg_end:

connect_failed_msg:
    db "wifi connect: failed", 10
connect_failed_msg_end:

probe_start_msg:
    db "wifi: sending active probe request through mac80211", 10
probe_start_msg_end:

probe_ok_msg:
    db "wifi: probe request built, probe response cached", 10
probe_ok_msg_end:

probe_failed_msg:
    db "wifi probe: failed", 10
probe_failed_msg_end:

rxprobe_start_msg:
    db "wifi: injecting probe response through driver RX hook", 10
rxprobe_start_msg_end:

rxprobe_ok_msg:
    db "wifi: RX hook accepted probe response", 10
rxprobe_ok_msg_end:

rxprobe_failed_msg:
    db "wifi rxprobe: failed", 10
rxprobe_failed_msg_end:

device_msg:
    db "wifi: PCI 802.11 controller at "
device_msg_end:

vendor_msg:
    db "  vendor:device="
vendor_msg_end:

class_msg:
    db "  class="
class_msg_end:

status_msg:
    db "  status=detected, firmware/mac80211 stack needed", 10
status_msg_end:

driver_intel_msg:
    db "  target-driver=aos-iwlwifi", 10
driver_intel_msg_end:

driver_realtek_msg:
    db "  target-driver=aos-rtlwifi", 10
driver_realtek_msg_end:

driver_broadcom_msg:
    db "  target-driver=aos-b43", 10
driver_broadcom_msg_end:

driver_atheros_msg:
    db "  target-driver=aos-athwifi", 10
driver_atheros_msg_end:

driver_ralink_msg:
    db "  target-driver=aos-rt2x00", 10
driver_ralink_msg_end:

driver_mediatek_msg:
    db "  target-driver=aos-mtwifi", 10
driver_mediatek_msg_end:

driver_unknown_msg:
    db "  target-driver=aos-wifi-pci", 10
driver_unknown_msg_end:

no_wifi_msg:
    db "wifi: no PCI 802.11 controller detected", 10
no_wifi_msg_end:

next_msg:
    db "wifi: current QEMU run provides e1000 Ethernet only", 10
next_msg_end:

roadmap_msg:
    db "wifi targets:", 10
    db "  Intel    -> aos-iwlwifi", 10
    db "  Realtek  -> aos-rtlwifi", 10
    db "  Broadcom -> aos-b43", 10
    db "  Atheros  -> aos-athwifi", 10
    db "  Ralink   -> aos-rt2x00", 10
    db "  MediaTek -> aos-mtwifi", 10
    db "next: chipset RX path + auth/assoc states", 10
roadmap_msg_end:

mac80211_msg:
    db "mac80211: scan/auth/assoc state machine registered", 10
mac80211_msg_end:

state_title_msg:
    db "mac80211 state:", 10
state_title_msg_end:

state_state_msg:
    db "  state="
state_state_msg_end:

state_scan_msg:
    db " scan-cache="
state_scan_msg_end:

state_selected_msg:
    db " selected="
state_selected_msg_end:

state_network_msg:
    db "  network="
state_network_msg_end:

state_bssid_msg:
    db " bssid="
state_bssid_msg_end:

state_channel_msg:
    db " ch="
state_channel_msg_end:

state_rssi_msg:
    db " rssi="
state_rssi_msg_end:

state_security_msg:
    db " security="
state_security_msg_end:

state_rx_msg:
    db "  rx: mgmt="
state_rx_msg_end:

state_beacon_msg:
    db " beacon="
state_beacon_msg_end:

state_probe_msg:
    db " probe="
state_probe_msg_end:

state_data_msg:
    db " data="
state_data_msg_end:

state_other_msg:
    db " other="
state_other_msg_end:

state_tx_msg:
    db "  tx: mgmt="
state_tx_msg_end:

state_tx_probe_msg:
    db " probe-req="
state_tx_probe_msg_end:

state_last_tx_msg:
    db " last-len="
state_last_tx_msg_end:

state_driver_tx_msg:
    db "  driver-tx: calls="
state_driver_tx_msg_end:

state_driver_hw_msg:
    db " hw="
state_driver_hw_msg_end:

state_driver_sim_msg:
    db " sim="
state_driver_sim_msg_end:

state_driver_err_msg:
    db " errors="
state_driver_err_msg_end:

state_driver_rx_msg:
    db "  driver-rx: calls="
state_driver_rx_msg_end:

state_driver_rx_accept_msg:
    db " accepted="
state_driver_rx_accept_msg_end:

state_driver_rx_err_msg:
    db " errors="
state_driver_rx_err_msg_end:

state_driver_rx_len_msg:
    db " last-len="
state_driver_rx_len_msg_end:

state_driver_rx_rssi_msg:
    db " rssi="
state_driver_rx_rssi_msg_end:

state_auth_msg:
    db "  auth attempts="
state_auth_msg_end:

state_assoc_msg:
    db " assoc attempts="
state_assoc_msg_end:

state_failed_msg:
    db "mac80211 state: unavailable", 10
state_failed_msg_end:

state_down_msg:
    db "down"
state_down_msg_end:

state_scanning_msg:
    db "scanning"
state_scanning_msg_end:

state_scanned_msg:
    db "scanned"
state_scanned_msg_end:

state_authing_msg:
    db "authenticating"
state_authing_msg_end:

state_authed_msg:
    db "authenticated"
state_authed_msg_end:

state_associng_msg:
    db "associating"
state_associng_msg_end:

state_associated_msg:
    db "associated"
state_associated_msg_end:

state_unknown_msg:
    db "unknown"
state_unknown_msg_end:

fw_status_msg:
    db "firmware status:", 10
fw_status_msg_end:

fw_arrow_msg:
    db " -> "
fw_arrow_msg_end:

fw_found_msg:
    db "found", 10
fw_found_msg_end:

fw_missing_msg:
    db "missing", 10
fw_missing_msg_end:

fw_intel_label:
    db "  Intel    "
fw_intel_label_end:
fw_intel_path:
    db "firmware/iwlwifi-test.fw"
fw_intel_path_end:

fw_realtek_label:
    db "  Realtek  "
fw_realtek_label_end:
fw_realtek_path:
    db "firmware/rtlwifi-test.fw"
fw_realtek_path_end:

fw_broadcom_label:
    db "  Broadcom "
fw_broadcom_label_end:
fw_broadcom_path:
    db "firmware/b43-test.fw"
fw_broadcom_path_end:

fw_atheros_label:
    db "  Atheros  "
fw_atheros_label_end:
fw_atheros_path:
    db "firmware/athwifi-test.fw"
fw_atheros_path_end:

fw_ralink_label:
    db "  Ralink   "
fw_ralink_label_end:
fw_ralink_path:
    db "firmware/rt2x00-test.fw"
fw_ralink_path_end:

fw_mediatek_label:
    db "  MediaTek "
fw_mediatek_label_end:
fw_mediatek_path:
    db "firmware/mtwifi-test.fw"
fw_mediatek_path_end:

gap_msg:
    db "  "
gap_msg_end:

space_msg:
    db " "

minus_msg:
    db "-"
minus_msg_end:

dbm_msg:
    db "dBm"
dbm_msg_end:

sec_open_msg:
    db "OPEN"
sec_open_msg_end:

sec_wpa2_msg:
    db "WPA2"
sec_wpa2_msg_end:

sec_wpa3_msg:
    db "WPA3"
sec_wpa3_msg_end:

sec_unknown_msg:
    db "UNKNOWN"
sec_unknown_msg_end:

colon_msg:
    db ":"
colon_msg_end:

dot_msg:
    db "."
dot_msg_end:

newline_msg:
    db 10
newline_msg_end:

hex_digits:
    db "0123456789abcdef"
