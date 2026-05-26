; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_DRIVER_INFO 519

%define DRV_TYPE_OFF 0
%define DRV_CLAIMED_OFF 1
%define DRV_BUS_OFF 2
%define DRV_SLOT_OFF 3
%define DRV_FUNC_OFF 4
%define DRV_IRQ_OFF 8
%define DRV_VENDOR_OFF 16
%define DRV_DEVICE_OFF 18
%define DRV_NAME_OFF 44
%define DRV_STATUS_OFF 76
%define DRV_STRUCT_SIZE 140

_start:
    lea rsi, [rel header_msg]
    mov rdx, header_msg_end - header_msg
    call write_stdout

    xor r12, r12

.loop:
    mov rax, AOS_SYS_DRIVER_INFO
    mov rdi, r12
    lea rsi, [rel driver_buf]
    syscall
    test rax, rax
    js .done

    lea rsi, [rel pci_msg]
    mov rdx, pci_msg_end - pci_msg
    call write_stdout
    movzx edi, byte [rel driver_buf + DRV_BUS_OFF]
    call write_hex8
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rel driver_buf + DRV_SLOT_OFF]
    call write_hex8
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rel driver_buf + DRV_FUNC_OFF]
    call write_hex_nibble

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    movzx edi, word [rel driver_buf + DRV_VENDOR_OFF]
    call write_hex16
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, word [rel driver_buf + DRV_DEVICE_OFF]
    call write_hex16

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    lea rsi, [rel driver_buf + DRV_NAME_OFF]
    mov rdx, 12
    call write_padded_cstr

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    lea rsi, [rel driver_buf + DRV_STATUS_OFF]
    mov rdx, 64
    call write_cstr_max

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

write_cstr_max:
    push rsi
    call cstrnlen
    mov rdx, rax
    pop rsi
    jmp write_stdout

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
driver_buf:
    resb DRV_STRUCT_SIZE
one_char:
    resb 1

section .rodata
header_msg:
    db "DEVICE    VENDOR:DEVICE  DRIVER       STATUS", 10
    db "----------------------------------------------", 10
header_msg_end:

no_devices_msg:
    db "drivers: no devices registered", 10
no_devices_msg_end:

pci_msg:
    db "pci "
pci_msg_end:

gap_msg:
    db "  "
gap_msg_end:

colon_msg:
    db ":"
colon_msg_end:

dot_msg:
    db "."
dot_msg_end:

space_msg:
    db " "

newline_msg:
    db 10
newline_msg_end:

hex_digits:
    db "0123456789abcdef"
