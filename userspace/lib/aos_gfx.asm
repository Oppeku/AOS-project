; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

global aos_gfx_info
global aos_gfx_clear
global aos_gfx_pixel
global aos_gfx_rect
global aos_gfx_present
global aos_gfx_blit_rgb565
global aos_gfx_blend_round_rect
global aos_gfx_cursor
global aos_gfx_aa_line
global aos_gfx_aa_circle
global aos_gfx_blit_rgb565_rect
global aos_gfx_blit_alpha_mask

%define AOS_SYS_GFX_INFO 520
%define AOS_SYS_GFX_CLEAR 521
%define AOS_SYS_GFX_PIXEL 522
%define AOS_SYS_GFX_RECT 523
%define AOS_SYS_GFX_PRESENT 524
%define AOS_SYS_GFX_BLIT_RGB565 553
%define AOS_SYS_GFX_BLEND_ROUND_RECT 554
%define AOS_SYS_GFX_CURSOR 555
%define AOS_SYS_GFX_AA_LINE 556
%define AOS_SYS_GFX_AA_CIRCLE 557
%define AOS_SYS_GFX_BLIT_RGB565_RECT 558
%define AOS_SYS_GFX_BLIT_ALPHA_MASK 559

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

aos_gfx_blit_rgb565:
    mov rax, AOS_SYS_GFX_BLIT_RGB565
    syscall
    ret

aos_gfx_blend_round_rect:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_BLEND_ROUND_RECT
    syscall
    ret

aos_gfx_cursor:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_CURSOR
    syscall
    ret

aos_gfx_aa_line:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_AA_LINE
    syscall
    ret

aos_gfx_aa_circle:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_AA_CIRCLE
    syscall
    ret

aos_gfx_blit_rgb565_rect:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_BLIT_RGB565_RECT
    syscall
    ret

aos_gfx_blit_alpha_mask:
    mov r10, rcx
    mov rax, AOS_SYS_GFX_BLIT_ALPHA_MASK
    syscall
    ret
