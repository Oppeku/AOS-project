; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

; =============================================================================
; AOS - x86_64 Multiboot2 Entry
; =============================================================================

section .boot
align 8
multiboot_header_start:
    dd 0xe85250d6                ; Magic
    dd 0                         ; Architecture 0 (i386)
    dd multiboot_header_end - multiboot_header_start
    dd 0x100000000 - (0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))
    ; End tag
    dw 0, 0
    dd 8
multiboot_header_end:

[BITS 32]
section .text
global _start
global stack_top
global load_tss
extern kernel_main
extern set_up_page_tables
extern enable_paging

_start:
    cli
    mov esp, stack_top

    ; Save Multiboot2 info
    mov edi, eax    ; Magic
    mov esi, ebx    ; Info Pointer

    call set_up_page_tables
    call enable_paging

    lgdt [gdt64.pointer]
    jmp 0x08:long_mode_init

[BITS 64]
long_mode_init:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Enable x87/SSE for userspace libc binaries.
    mov rax, cr0
    and eax, ~(1 << 2)              ; Clear EM
    or eax, (1 << 1)                ; Set MP
    mov cr0, rax
    clts                            ; Clear TS

    mov eax, 1
    cpuid

    mov rax, cr4
    or eax, (1 << 9) | (1 << 10)    ; OSFXSR | OSXMMEXCPT
    bt ecx, 26                       ; XSAVE available?
    jnc .no_xsave
    or eax, (1 << 18)                ; OSXSAVE
    mov cr4, rax
    xor ecx, ecx
    mov eax, 0x3                     ; Enable x87 + SSE state in XCR0
    xor edx, edx
    xsetbv
    jmp .fpu_ready

.no_xsave:
    mov cr4, rax
.fpu_ready:
    fninit

    mov rdi, rdi
    mov rsi, rsi

    call kernel_main

    cli
.hlt:
    hlt
    jmp .hlt

load_tss:
    ltr di
    ret

section .rodata
gdt64:
    dq 0 ; null
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; Kernel Code
.data: equ $ - gdt64
    dq (1<<44) | (1<<47) | (1<<41)           ; Kernel Data
.user_data: equ $ - gdt64
    dq (1<<44) | (1<<47) | (1<<41) | (3<<45) ; User Data (DPL 3)
.user_code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) | (3<<45) ; User Code (DPL 3)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64 ; <--- FIXED: Must be dq (8 bytes) for 64-bit

section .bss
align 16
stack_bottom:
    resb 32768
stack_top:
