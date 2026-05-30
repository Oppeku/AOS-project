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
%define PCI_BAR_OFF 16

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

    cmp byte [rel pci_buf + PCI_CLASS_OFF], 0x0c
    jne .next
    cmp byte [rel pci_buf + PCI_SUBCLASS_OFF], 0x03
    jne .next

    inc r13
    lea rsi, [rel controller_msg]
    mov rdx, controller_msg_end - controller_msg
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

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    call write_usb_kind

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

    lea rsi, [rel irq_msg]
    mov rdx, irq_msg_end - irq_msg
    call write_stdout
    movzx edi, byte [rel pci_buf + PCI_IRQ_OFF]
    call write_hex8

    lea rsi, [rel bar0_msg]
    mov rdx, bar0_msg_end - bar0_msg
    call write_stdout
    mov edi, [rel pci_buf + PCI_BAR_OFF]
    call write_hex32

    lea rsi, [rel status_msg]
    mov rdx, status_msg_end - status_msg
    call write_stdout

.next:
    inc r12
    jmp .loop

.done:
    test r13, r13
    jnz .exit
    lea rsi, [rel no_usb_msg]
    mov rdx, no_usb_msg_end - no_usb_msg
    call write_stdout

.exit:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

write_usb_kind:
    mov al, [rel pci_buf + PCI_PROGIF_OFF]
    cmp al, 0x00
    je .uhci
    cmp al, 0x10
    je .ohci
    cmp al, 0x20
    je .ehci
    cmp al, 0x30
    je .xhci
    lea rsi, [rel unknown_msg]
    mov rdx, unknown_msg_end - unknown_msg
    jmp write_stdout
.uhci:
    lea rsi, [rel uhci_msg]
    mov rdx, uhci_msg_end - uhci_msg
    jmp write_stdout
.ohci:
    lea rsi, [rel ohci_msg]
    mov rdx, ohci_msg_end - ohci_msg
    jmp write_stdout
.ehci:
    lea rsi, [rel ehci_msg]
    mov rdx, ehci_msg_end - ehci_msg
    jmp write_stdout
.xhci:
    lea rsi, [rel xhci_msg]
    mov rdx, xhci_msg_end - xhci_msg
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

write_hex32:
    push rdi
    shr edi, 16
    call write_hex16
    pop rdi
    call write_hex16
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
    db "AOS USB", 10
    db "-------", 10
title_msg_end:

controller_msg:
    db "usb: controller at "
controller_msg_end:

gap_msg:
    db "  "
gap_msg_end:

uhci_msg:
    db "UHCI"
uhci_msg_end:

ohci_msg:
    db "OHCI"
ohci_msg_end:

ehci_msg:
    db "EHCI"
ehci_msg_end:

xhci_msg:
    db "xHCI"
xhci_msg_end:

unknown_msg:
    db "unknown-HCI"
unknown_msg_end:

vendor_msg:
    db "  vendor:device="
vendor_msg_end:

irq_msg:
    db "  irq="
irq_msg_end:

bar0_msg:
    db "  bar0="
bar0_msg_end:

status_msg:
    db "  status=xHCI keyboard input ready", 10
status_msg_end:

no_usb_msg:
    db "usb: no PCI USB controller detected", 10
no_usb_msg_end:

colon_msg:
    db ":"
colon_msg_end:

dot_msg:
    db "."
dot_msg_end:

hex_digits:
    db "0123456789abcdef"
