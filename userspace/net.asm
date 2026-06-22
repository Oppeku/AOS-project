; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_WRITE 1
%define SYS_EXIT 60
%define AOS_SYS_NETDEV_INFO 526
%define AOS_SYS_NETDEV_STATS 535

%define NET_TYPE_OFF 0
%define NET_LINK_OFF 1
%define NET_MAC_OFF 2
%define NET_BUS_OFF 8
%define NET_SLOT_OFF 9
%define NET_FUNC_OFF 10
%define NET_NAME_OFF 16
%define NET_DRIVER_OFF 32
%define NET_STATUS_OFF 64
%define NET_IPV4_OFF 128
%define NET_GATEWAY_OFF 132
%define NET_DNS_OFF 136
%define NET_PREFIX_OFF 140
%define NET_IPV4_CONFIGURED_OFF 141
%define NET_IPV6_CONFIGURED_OFF 142
%define NET_IPV6_PREFIX_OFF 143
%define NET_IPV6_OFF 146
%define NET_IPV6_GATEWAY_OFF 162
%define NET_IPV6_DNS_OFF 178
%define NET_STRUCT_SIZE 208

%define ST_TX_PACKETS_OFF 0
%define ST_RX_PACKETS_OFF 8
%define ST_TX_BYTES_OFF 16
%define ST_RX_BYTES_OFF 24
%define ST_TX_ERRORS_OFF 32
%define ST_RX_ERRORS_OFF 40
%define ST_TX_DROPPED_OFF 48
%define ST_RX_DROPPED_OFF 56
%define ST_STRUCT_SIZE 160

_start:
    lea rsi, [rel header_msg]
    mov rdx, header_msg_end - header_msg
    call write_stdout

    xor r12, r12

.loop:
    mov rax, AOS_SYS_NETDEV_INFO
    mov rdi, r12
    lea rsi, [rel net_buf]
    syscall
    test rax, rax
    js .done

    lea rsi, [rel net_buf + NET_NAME_OFF]
    mov rdx, 8
    call write_padded_cstr
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    lea rsi, [rel net_buf + NET_DRIVER_OFF]
    mov rdx, 12
    call write_padded_cstr
    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout

    lea rbx, [rel net_buf + NET_MAC_OFF]
    movzx edi, byte [rbx]
    call write_hex8
    mov r14, 1
.mac_loop:
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rbx + r14]
    push r14
    call write_hex8
    pop r14
    inc r14
    cmp r14, 6
    jb .mac_loop

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    cmp byte [rel net_buf + NET_LINK_OFF], 0
    je .link_down
    lea rsi, [rel up_msg]
    mov rdx, up_msg_end - up_msg
    jmp .print_link
.link_down:
    lea rsi, [rel down_msg]
    mov rdx, down_msg_end - down_msg
.print_link:
    call write_stdout

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    movzx edi, byte [rel net_buf + NET_BUS_OFF]
    call write_hex8
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rel net_buf + NET_SLOT_OFF]
    call write_hex8
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rel net_buf + NET_FUNC_OFF]
    call write_hex_nibble

    lea rsi, [rel gap_msg]
    mov rdx, gap_msg_end - gap_msg
    call write_stdout
    lea rsi, [rel net_buf + NET_STATUS_OFF]
    mov rdx, 64
    call write_cstr_max
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    cmp byte [rel net_buf + NET_IPV4_CONFIGURED_OFF], 0
    je .no_ipv4
    lea rsi, [rel inet_indent_msg]
    mov rdx, inet_indent_msg_end - inet_indent_msg
    call write_stdout
    lea rbx, [rel net_buf + NET_IPV4_OFF]
    call write_ipv4_from_rbx
    lea rsi, [rel slash_msg]
    mov rdx, slash_msg_end - slash_msg
    call write_stdout
    movzx edi, byte [rel net_buf + NET_PREFIX_OFF]
    call write_u64
    lea rsi, [rel gateway_msg]
    mov rdx, gateway_msg_end - gateway_msg
    call write_stdout
    lea rbx, [rel net_buf + NET_GATEWAY_OFF]
    call write_ipv4_from_rbx
    lea rsi, [rel dns_msg]
    mov rdx, dns_msg_end - dns_msg
    call write_stdout
    lea rbx, [rel net_buf + NET_DNS_OFF]
    call write_ipv4_from_rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
.no_ipv4:
    cmp byte [rel net_buf + NET_IPV6_CONFIGURED_OFF], 0
    je .no_ipv6
    lea rsi, [rel inet6_indent_msg]
    mov rdx, inet6_indent_msg_end - inet6_indent_msg
    call write_stdout
    lea rbx, [rel net_buf + NET_IPV6_OFF]
    call write_ipv6_from_rbx
    lea rsi, [rel slash_msg]
    mov rdx, slash_msg_end - slash_msg
    call write_stdout
    movzx edi, byte [rel net_buf + NET_IPV6_PREFIX_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    lea rbx, [rel net_buf + NET_IPV6_GATEWAY_OFF]
    call ipv6_is_zero
    test eax, eax
    jnz .no_ipv6
    lea rsi, [rel gateway6_indent_msg]
    mov rdx, gateway6_indent_msg_end - gateway6_indent_msg
    call write_stdout
    lea rbx, [rel net_buf + NET_IPV6_GATEWAY_OFF]
    call write_ipv6_from_rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
.no_ipv6:
    mov rax, AOS_SYS_NETDEV_STATS
    mov rdi, r12
    lea rsi, [rel stats_buf]
    syscall
    test rax, rax
    js .skip_stats

    lea rsi, [rel stats_indent_msg]
    mov rdx, stats_indent_msg_end - stats_indent_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_RX_PACKETS_OFF]
    call write_u64
    lea rsi, [rel packets_bytes_sep_msg]
    mov rdx, packets_bytes_sep_msg_end - packets_bytes_sep_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_RX_BYTES_OFF]
    call write_u64
    lea rsi, [rel tx_stats_msg]
    mov rdx, tx_stats_msg_end - tx_stats_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_TX_PACKETS_OFF]
    call write_u64
    lea rsi, [rel packets_bytes_sep_msg]
    mov rdx, packets_bytes_sep_msg_end - packets_bytes_sep_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_TX_BYTES_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    lea rsi, [rel stats_err_indent_msg]
    mov rdx, stats_err_indent_msg_end - stats_err_indent_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_RX_ERRORS_OFF]
    call write_u64
    lea rsi, [rel dropped_msg]
    mov rdx, dropped_msg_end - dropped_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_RX_DROPPED_OFF]
    call write_u64
    lea rsi, [rel tx_errors_msg]
    mov rdx, tx_errors_msg_end - tx_errors_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_TX_ERRORS_OFF]
    call write_u64
    lea rsi, [rel dropped_msg]
    mov rdx, dropped_msg_end - dropped_msg
    call write_stdout
    mov rdi, [rel stats_buf + ST_TX_DROPPED_OFF]
    call write_u64
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
.skip_stats:

    inc r12
    jmp .loop

.done:
    test r12, r12
    jnz .exit
    lea rsi, [rel no_net_msg]
    mov rdx, no_net_msg_end - no_net_msg
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

write_ipv4_from_rbx:
    movzx edi, byte [rbx]
    call write_u64
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rbx + 1]
    call write_u64
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rbx + 2]
    call write_u64
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rbx + 3]
    jmp write_u64

write_ipv6_from_rbx:
    xor r14, r14
.ipv6_loop:
    movzx edi, byte [rbx + r14 * 2]
    shl edi, 8
    movzx eax, byte [rbx + r14 * 2 + 1]
    or edi, eax
    call write_hex16
    inc r14
    cmp r14, 8
    jae .ipv6_done
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    jmp .ipv6_loop
.ipv6_done:
    ret

ipv6_is_zero:
    xor eax, eax
    xor ecx, ecx
.zero_loop:
    cmp byte [rbx + rcx], 0
    jne .not_zero
    inc ecx
    cmp ecx, 16
    jb .zero_loop
    mov eax, 1
.not_zero:
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
    mov r10, 10
    div r10
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
net_buf:
    resb NET_STRUCT_SIZE
stats_buf:
    resb ST_STRUCT_SIZE
one_char:
    resb 1
num_buf:
    resb 21

section .rodata
header_msg:
    db "IFACE     DRIVER        MAC                LINK  PCI       STATUS", 10
    db "------------------------------------------------------------------", 10
header_msg_end:

no_net_msg:
    db "net: no network interfaces registered", 10
no_net_msg_end:

gap_msg:
    db "  "
gap_msg_end:

colon_msg:
    db ":"
colon_msg_end:

dot_msg:
    db "."
dot_msg_end:

slash_msg:
    db "/"
slash_msg_end:

inet_indent_msg:
    db "          inet "
inet_indent_msg_end:

inet6_indent_msg:
    db "          inet6 "
inet6_indent_msg_end:

gateway6_indent_msg:
    db "          gateway6 "
gateway6_indent_msg_end:

gateway_msg:
    db "  gateway "
gateway_msg_end:

dns_msg:
    db "  dns "
dns_msg_end:

stats_indent_msg:
    db "          RX packets "
stats_indent_msg_end:

packets_bytes_sep_msg:
    db " bytes "
packets_bytes_sep_msg_end:

tx_stats_msg:
    db "  TX packets "
tx_stats_msg_end:

stats_err_indent_msg:
    db "          RX errors "
stats_err_indent_msg_end:

dropped_msg:
    db " dropped "
dropped_msg_end:

tx_errors_msg:
    db "  TX errors "
tx_errors_msg_end:

up_msg:
    db "up  "
up_msg_end:

down_msg:
    db "down"
down_msg_end:

space_msg:
    db " "

newline_msg:
    db 10
newline_msg_end:

hex_digits:
    db "0123456789abcdef"
