; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]
default rel
global _start
extern aos_gfx_info
extern aos_gfx_clear
extern aos_gfx_rect
extern aos_gfx_present
extern aos_input_poll

%define SYS_WRITE 1
%define SYS_EXIT 60

%define GFX_INFO_WIDTH 0
%define GFX_INFO_HEIGHT 4
%define GFX_INFO_BPP 8
%define GFX_INFO_READY 12
%define KEY_UP 0x11
%define KEY_DOWN 0x12
%define KEY_LEFT 0x13
%define KEY_RIGHT 0x14
%define INPUT_EVENT_KEY 0

_start:
    lea rsi, [title_msg]
    mov rdx, title_msg_end - title_msg
    call write_stdout

    lea rdi, [gfx_info]
    call aos_gfx_info
    test rax, rax
    js fail

    cmp dword [gfx_info + GFX_INFO_READY], 1
    jne no_gfx

    lea rsi, [mode_msg]
    mov rdx, mode_msg_end - mode_msg
    call write_stdout
    mov edi, [gfx_info + GFX_INFO_WIDTH]
    call print_u64
    lea rsi, [x_msg]
    mov rdx, x_msg_end - x_msg
    call write_stdout
    mov edi, [gfx_info + GFX_INFO_HEIGHT]
    call print_u64
    lea rsi, [bpp_msg]
    mov rdx, bpp_msg_end - bpp_msg
    call write_stdout
    mov edi, [gfx_info + GFX_INFO_BPP]
    call print_u64
    call newline

    lea rsi, [control_msg]
    mov rdx, control_msg_end - control_msg
    call write_stdout

    mov r14, 464
    mov r15, 320
.frame:
    lea rdi, [input_event]
    call aos_input_poll
    cmp rax, 0
    jle .draw

    mov eax, [input_event + INPUT_EVENT_KEY]
    cmp eax, 'q'
    je .done
    cmp eax, 'Q'
    je .done
    cmp eax, KEY_LEFT
    je .move_left
    cmp eax, 'a'
    je .move_left
    cmp eax, 'A'
    je .move_left
    cmp eax, KEY_RIGHT
    je .move_right
    cmp eax, 'd'
    je .move_right
    cmp eax, 'D'
    je .move_right
    cmp eax, KEY_UP
    je .move_up
    cmp eax, 'w'
    je .move_up
    cmp eax, 'W'
    je .move_up
    cmp eax, KEY_DOWN
    je .move_down
    cmp eax, 's'
    je .move_down
    cmp eax, 'S'
    je .move_down
    jmp .draw

.move_left:
    cmp r14, 24
    jb .draw
    sub r14, 24
    jmp .draw
.move_right:
    cmp r14, 904
    ja .draw
    add r14, 24
    jmp .draw
.move_up:
    cmp r15, 144
    jb .draw
    sub r15, 24
    jmp .draw
.move_down:
    cmp r15, 616
    ja .draw
    add r15, 24

.draw:
    mov rdi, 0x05070a
    call aos_gfx_clear

    mov rdi, 64
    mov rsi, 64
    mov rdx, 896
    mov rcx, 48
    mov r8, 0x2563eb
    call aos_gfx_rect

    mov rdi, r14
    mov rsi, r15
    mov rdx, 96
    mov rcx, 96
    mov r8, 0x22c55e
    call aos_gfx_rect

    mov rdi, 96
    mov rsi, 660
    mov rdx, 640
    mov rcx, 28
    mov r8, 0xfacc15
    call aos_gfx_rect

    mov rdi, 780
    mov rsi, 230
    mov rdx, 112
    mov rcx, 112
    mov r8, 0xef4444
    call aos_gfx_rect

    call aos_gfx_present

    call delay
    jmp .frame

.done:
    mov rdi, 0x05070a
    call aos_gfx_clear
    call aos_gfx_present

    lea rsi, [reset_screen_msg]
    mov rdx, reset_screen_msg_end - reset_screen_msg
    call write_stdout
    lea rsi, [done_msg]
    mov rdx, done_msg_end - done_msg
    call write_stdout
    xor rdi, rdi
    jmp exit

no_gfx:
    lea rsi, [no_gfx_msg]
    mov rdx, no_gfx_msg_end - no_gfx_msg
    call write_stdout
    mov rdi, 1
    jmp exit

fail:
    lea rsi, [fail_msg]
    mov rdx, fail_msg_end - fail_msg
    call write_stdout
    mov rdi, 1
    jmp exit

delay:
    mov rcx, 2500000
.loop:
    pause
    loop .loop
    ret

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

print_u64:
    mov rax, rdi
    lea rsi, [num_buf_end]
    mov byte [rsi - 1], 0
    lea rbx, [rsi - 1]
    mov rcx, 10
    test rax, rax
    jnz .digits
    dec rbx
    mov byte [rbx], '0'
    jmp .emit
.digits:
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rbx
    mov [rbx], dl
    test rax, rax
    jnz .digits
.emit:
    mov rsi, rbx
    lea rdx, [num_buf_end - 1]
    sub rdx, rbx
    jmp write_stdout

newline:
    lea rsi, [newline_msg]
    mov rdx, 1
    jmp write_stdout

exit:
    mov rax, SYS_EXIT
    syscall

section .bss
gfx_info:
    resb 16
input_event:
    resb 16
num_buf:
    resb 32
num_buf_end:

section .rodata
title_msg:
    db "gfxdemo: userspace framebuffer demo", 10
title_msg_end:
mode_msg:
    db "gfxdemo: mode "
mode_msg_end:
control_msg:
    db "gfxdemo: arrows/WASD move, q exits", 10
control_msg_end:
x_msg:
    db "x"
x_msg_end:
bpp_msg:
    db "x"
bpp_msg_end:
reset_screen_msg:
    db 27, "[2J", 27, "[H"
reset_screen_msg_end:
done_msg:
    db "gfxdemo: done", 10
done_msg_end:
no_gfx_msg:
    db "gfxdemo: framebuffer is not ready", 10
no_gfx_msg_end:
fail_msg:
    db "gfxdemo: gfx syscall failed", 10
fail_msg_end:
newline_msg:
    db 10
