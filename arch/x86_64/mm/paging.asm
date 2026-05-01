; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 32]

section .text
global set_up_page_tables
global enable_paging
global load_pagemap       ; Export for VMM
global p4_table           ; Export for VMM

set_up_page_tables:
    ; Point P4[0] to P3 (Must include User bit for ring 3)
    mov eax, p3_table
    or eax, 0b111         ; Present + Writable + User
    mov [p4_table], eax

    ; Point P3[0] to P2 (Must include User bit for ring 3)
    mov eax, p2_table
    or eax, 0b111         ; Present + Writable + User
    mov [p3_table], eax

    ; Identity map the first 1GB using 2MB huge pages.
    xor ecx, ecx
    xor ebx, ebx
.map_identity:
    mov eax, ebx
    or eax, 0b10000111    ; P + W + U + HugePage (Bit 7)
    mov [p2_table + ecx*8], eax
    mov dword [p2_table + ecx*8 + 4], 0
    add ebx, 0x00200000
    inc ecx
    cmp ecx, 512
    jne .map_identity

    ret

enable_paging:
    ; Pass P4 to CR3
    mov eax, p4_table
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable Long Mode in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable Paging in CR0 and set WP bit (bit 16)
    mov eax, cr0
    or eax, (1 << 31) | (1 << 16)
    mov cr0, eax
    ret

[BITS 64]
load_pagemap:
    mov cr3, rdi         ; RDI is the first argument (new P4 table)
    ret

section .bss
align 4096
p4_table: resb 4096      ; The actual table the linker was missing
p3_table: resb 4096
p2_table: resb 4096
section .note.GNU-stack noalloc noexec nowrite progbits
