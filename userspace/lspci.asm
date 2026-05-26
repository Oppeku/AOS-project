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
%define PCI_REVISION_OFF 10
%define PCI_HEADER_OFF 11
%define PCI_IRQ_OFF 12
%define PCI_BAR_OFF 16

_start:
    lea rsi, [rel header_msg]
    mov rdx, header_msg_end - header_msg
    call write_stdout

    xor r12, r12

.loop:
    mov rax, AOS_SYS_PCI_INFO
    mov rdi, r12
    lea rsi, [rel pci_buf]
    syscall
    test rax, rax
    js .done

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
    movzx edi, word [rel pci_buf + PCI_VENDOR_OFF]
    call write_hex16
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, word [rel pci_buf + PCI_DEVICE_OFF]
    call write_hex16

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
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

    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    inc r12
    jmp .loop

.done:
    test r12, r12
    jnz .exit
    lea rsi, [rel no_devices_msg]
    mov rdx, no_devices_msg_end - no_devices_msg
    call write_stdout

.exit:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

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
header_msg:
    db "PCI DEV  VENDOR:DEVICE  CLASS:SUB:IF  IRQ  BAR0", 10
    db "-------------------------------------------------", 10
header_msg_end:

no_devices_msg:
    db "lspci: no PCI devices found", 10
no_devices_msg_end:

gap_msg:
    db "  "
gap_msg_end:

colon_msg:
    db ":"
colon_msg_end:

dot_msg:
    db "."
dot_msg_end:

irq_msg:
    db "  irq="
irq_msg_end:

bar0_msg:
    db "  bar0="
bar0_msg_end:

newline_msg:
    db 10
newline_msg_end:

hex_digits:
    db "0123456789abcdef"
