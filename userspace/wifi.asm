; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_PCI_INFO 518

%define PCI_VENDOR_OFF 0
%define PCI_DEVICE_OFF 2
%define PCI_BUS_OFF 4
%define PCI_SLOT_OFF 5
%define PCI_FUNC_OFF 6
%define PCI_CLASS_OFF 7
%define PCI_SUBCLASS_OFF 8
%define PCI_PROGIF_OFF 9
%define PCI_IRQ_OFF 12

_start:
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
    jnz .exit
    lea rsi, [rel no_wifi_msg]
    mov rdx, no_wifi_msg_end - no_wifi_msg
    call write_stdout
    lea rsi, [rel next_msg]
    mov rdx, next_msg_end - next_msg
    call write_stdout

    lea rsi, [rel roadmap_msg]
    mov rdx, roadmap_msg_end - roadmap_msg
    call write_stdout

.exit:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

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

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
pci_buf:
    resb 64
one_char:
    resb 1

section .rodata
title_msg:
    db "AOS WiFi", 10
    db "--------", 10
title_msg_end:

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
    db "next: firmware loader + 802.11/mac layer", 10
roadmap_msg_end:

colon_msg:
    db ":"
colon_msg_end:

dot_msg:
    db "."
dot_msg_end:

hex_digits:
    db "0123456789abcdef"
