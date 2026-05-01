; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 32]
global start
extern kernel_main      ; This is our C function!
extern setup_paging    ; We'll use your paging.asm

section .text
start:
    mov esp, stack_top

    call setup_paging
    
    ; [Insert the Long Mode Switch we wrote earlier here]
    ; After the switch...

[BITS 64]
long_mode_start:
    ; Load Null data segments
    mov ax, 0
    mov ss, ax
    mov ds, ax
    
    call kernel_main   ; Jump into the C Brain
    hlt

section .bss
stack_bottom:
    resb 4096 * 4
stack_top:
