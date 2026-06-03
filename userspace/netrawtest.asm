; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_NETDEV_INFO 526
%define AOS_SYS_NETDEV_SEND 527
%define AOS_SYS_NETDEV_RECV 528

%define NET_MAC_OFF 2
%define NET_NAME_OFF 16
%define NET_DRIVER_OFF 32
%define NET_STRUCT_SIZE 208
%define FRAME_SIZE 60

_start:
    xor r15, r15
    mov rax, [rsp]
    cmp rax, 2
    jb .have_index
    mov rsi, [rsp + 16]
    mov al, [rsi]
    cmp al, '0'
    jb bad_index
    cmp al, '7'
    ja bad_index
    cmp byte [rsi + 1], 0
    jne bad_index
    sub al, '0'
    movzx r15, al

.have_index:
    mov rax, AOS_SYS_NETDEV_INFO
    mov rdi, r15
    lea rsi, [rel net_buf]
    syscall
    test rax, rax
    js no_net

    call build_test_frame

    mov rax, AOS_SYS_NETDEV_SEND
    mov rdi, r15
    lea rsi, [rel frame_buf]
    mov rdx, FRAME_SIZE
    syscall
    test rax, rax
    js send_fail

    lea rsi, [rel sent_msg]
    mov rdx, sent_msg_end - sent_msg
    call write_stdout
    lea rsi, [rel net_buf + NET_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel via_msg]
    mov rdx, via_msg_end - via_msg
    call write_stdout
    lea rsi, [rel net_buf + NET_DRIVER_OFF]
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    mov rax, AOS_SYS_NETDEV_RECV
    mov rdi, r15
    lea rsi, [rel rx_buf]
    mov rdx, 1518
    syscall
    test rax, rax
    js recv_fail
    jz no_rx

    push rax
    lea rsi, [rel rx_msg]
    mov rdx, rx_msg_end - rx_msg
    call write_stdout
    pop rdi
    call write_u64
    lea rsi, [rel bytes_msg]
    mov rdx, bytes_msg_end - bytes_msg
    call write_stdout
    jmp done_ok

no_rx:
    lea rsi, [rel no_rx_msg]
    mov rdx, no_rx_msg_end - no_rx_msg
    call write_stdout
    jmp done_ok

done_ok:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

no_net:
    lea rsi, [rel no_net_msg]
    mov rdx, no_net_msg_end - no_net_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

bad_index:
    lea rsi, [rel bad_index_msg]
    mov rdx, bad_index_msg_end - bad_index_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

send_fail:
    lea rsi, [rel send_fail_msg]
    mov rdx, send_fail_msg_end - send_fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

recv_fail:
    lea rsi, [rel recv_fail_msg]
    mov rdx, recv_fail_msg_end - recv_fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

build_test_frame:
    lea rbx, [rel frame_buf]
    mov rcx, 0
.dst_loop:
    mov byte [rbx + rcx], 0xff
    inc rcx
    cmp rcx, 6
    jb .dst_loop

    lea rsi, [rel net_buf + NET_MAC_OFF]
    mov rcx, 0
.src_loop:
    mov al, [rsi + rcx]
    mov [rbx + 6 + rcx], al
    inc rcx
    cmp rcx, 6
    jb .src_loop

    mov byte [rbx + 12], 0x88
    mov byte [rbx + 13], 0xb5

    lea rsi, [rel payload_msg]
    mov rcx, 0
.payload_loop:
    mov al, [rsi + rcx]
    test al, al
    jz .pad
    mov [rbx + 14 + rcx], al
    inc rcx
    cmp rcx, 32
    jb .payload_loop
.pad:
    mov rdx, 14
    add rdx, rcx
.pad_loop:
    cmp rdx, FRAME_SIZE
    jae .done
    mov byte [rbx + rdx], 0
    inc rdx
    jmp .pad_loop
.done:
    ret

write_u64:
    lea rsi, [rel num_buf + 20]
    mov byte [rsi], 0
    mov rax, rdi
    test rax, rax
    jnz .loop
    dec rsi
    mov byte [rsi], '0'
    jmp write_cstring_stdout
.loop:
    xor rdx, rdx
    mov rbx, 10
    div rbx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test rax, rax
    jnz .loop
    jmp write_cstring_stdout

write_cstring_stdout:
    xor rdx, rdx
.count:
    cmp byte [rsi + rdx], 0
    je .emit
    inc rdx
    jmp .count
.emit:
    jmp write_stdout

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
net_buf:
    resb NET_STRUCT_SIZE
frame_buf:
    resb FRAME_SIZE
rx_buf:
    resb 1518
num_buf:
    resb 21

section .rodata
sent_msg:
    db "netrawtest: sent broadcast test frame on "
sent_msg_end:

via_msg:
    db " via "
via_msg_end:

rx_msg:
    db "netrawtest: received "
rx_msg_end:

bytes_msg:
    db " bytes", 10
bytes_msg_end:

no_rx_msg:
    db "netrawtest: no frame waiting yet", 10
no_rx_msg_end:

no_net_msg:
    db "netrawtest: no network interface", 10
no_net_msg_end:

bad_index_msg:
    db "netrawtest: usage: netrawtest [0-7]", 10
bad_index_msg_end:

send_fail_msg:
    db "netrawtest: send failed", 10
send_fail_msg_end:

recv_fail_msg:
    db "netrawtest: recv failed", 10
recv_fail_msg_end:

payload_msg:
    db "AOS raw ethernet test", 0

newline_msg:
    db 10
newline_msg_end:
