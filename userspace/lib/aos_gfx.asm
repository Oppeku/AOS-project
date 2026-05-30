; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

global aos_gfx_info
global aos_gfx_clear
global aos_gfx_pixel
global aos_gfx_rect
global aos_gfx_present

%define AOS_SYS_GFX_INFO 520
%define AOS_SYS_GFX_CLEAR 521
%define AOS_SYS_GFX_PIXEL 522
%define AOS_SYS_GFX_RECT 523
%define AOS_SYS_GFX_PRESENT 524

aos_gfx_info:
    mov rax, AOS_SYS_GFX_INFO
    syscall
    ret

aos_gfx_clear:
    mov rax, AOS_SYS_GFX_CLEAR
    syscall
    ret

aos_gfx_pixel:
    mov rax, AOS_SYS_GFX_PIXEL
    syscall
    ret

aos_gfx_rect:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_RECT
    syscall
    ret

aos_gfx_present:
    mov rax, AOS_SYS_GFX_PRESENT
    syscall
    ret
