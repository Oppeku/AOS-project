; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]
global gdt_flush
global tss_flush

gdt_flush:
    lgdt [rdi]
    mov ax, 0x10      ; Kernel Data Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rdi
    mov rax, 0x08     ; Kernel Code Selector
    push rax
    push rdi
    retfq

tss_flush:
    mov ax, 0x2B      ; (5 * 8) | 3
    ltr ax
    ret
section .note.GNU-stack noalloc noexec nowrite progbits
