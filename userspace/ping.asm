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
%define AOS_SYS_DNS_LOOKUP 534

%define NET_LINK_OFF 1
%define NET_MAC_OFF 2
%define NET_NAME_OFF 16
%define NET_DRIVER_OFF 32
%define NET_IPV4_OFF 128
%define NET_GATEWAY_OFF 132
%define NET_DNS_OFF 136
%define NET_IPV4_CONFIGURED_OFF 141
%define NET_STRUCT_SIZE 208
%define TX_SIZE 1518
%define RX_SIZE 1518

_start:
    mov r12, [rsp]
    lea r13, [rsp + 8]
    cmp r12, 2
    jb usage

    mov qword [rel iface_index], 0
    mov rsi, [r13]
    call detect_dns_mode
    mov r14, [r13 + 8]
    cmp r12, 4
    jb .load_netdev
    mov rsi, [r13 + 8]
    call is_dash_i
    test eax, eax
    jz .load_netdev
    mov rsi, [r13 + 16]
    call parse_iface_index
    test eax, eax
    js usage
    mov [rel iface_index], rax
    mov r14, [r13 + 24]
    test r14, r14
    jz usage

.load_netdev:
    mov rax, AOS_SYS_NETDEV_INFO
    mov rdi, [rel iface_index]
    lea rsi, [rel net_buf]
    syscall
    test rax, rax
    js no_netdev
    cmp byte [rel net_buf + NET_LINK_OFF], 0
    je link_down
    cmp byte [rel net_buf + NET_IPV4_CONFIGURED_OFF], 0
    je no_ipv4

    mov rsi, r14
    call copy_target_text
    mov rsi, r14
    call parse_ipv4
    test eax, eax
    jz .have_target

    mov rsi, r14
    call copy_domain_text
    call resolve_dns_name
    test eax, eax
    jnz dns_fail
    cmp byte [rel dns_mode], 0
    jne exit_ok

.have_target:
    cmp byte [rel dns_mode], 0
    jne dns_numeric_ok
    call print_ping_header
    call choose_ping_arp_ip
    call resolve_arp
    test eax, eax
    jnz arp_fail

    call build_icmp_echo
    mov rax, AOS_SYS_NETDEV_SEND
    mov rdi, [rel iface_index]
    lea rsi, [rel tx_frame]
    mov rdx, 60
    syscall
    test rax, rax
    js icmp_send_fail

    mov ecx, 500000
.icmp_recv_loop:
    push rcx
    mov rax, AOS_SYS_NETDEV_RECV
    mov rdi, [rel iface_index]
    lea rsi, [rel rx_frame]
    mov rdx, RX_SIZE
    syscall
    pop rcx
    test rax, rax
    js recv_fail
    cmp rax, 42
    jb .next_icmp
    call is_icmp_echo_reply
    test eax, eax
    jz got_icmp
.next_icmp:
    loop .icmp_recv_loop

    lea rsi, [rel icmp_timeout_msg]
    mov rdx, icmp_timeout_msg_end - icmp_timeout_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

got_icmp:
    lea rsi, [rel icmp_reply_msg]
    mov rdx, icmp_reply_msg_end - icmp_reply_msg
    call write_stdout
    lea rbx, [rel target_ip]
    call write_ipv4_from_rbx
    lea rsi, [rel icmp_tail_msg]
    mov rdx, icmp_tail_msg_end - icmp_tail_msg
    call write_stdout
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

exit_ok:
    mov rax, SYS_EXIT
    xor rdi, rdi
    syscall

dns_numeric_ok:
    lea rsi, [rel dns_reply_msg]
    mov rdx, dns_reply_msg_end - dns_reply_msg
    call write_stdout
    lea rsi, [rel target_text]
    call write_cstring_stdout
    lea rsi, [rel arrow_msg]
    mov rdx, arrow_msg_end - arrow_msg
    call write_stdout
    lea rbx, [rel target_ip]
    call write_ipv4_from_rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    jmp exit_ok

usage:
    lea rsi, [rel usage_msg]
    mov rdx, usage_msg_end - usage_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

no_netdev:
    lea rsi, [rel no_netdev_msg]
    mov rdx, no_netdev_msg_end - no_netdev_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

link_down:
    lea rsi, [rel link_down_msg]
    mov rdx, link_down_msg_end - link_down_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

no_ipv4:
    lea rsi, [rel no_ipv4_prefix_msg]
    mov rdx, no_ipv4_prefix_msg_end - no_ipv4_prefix_msg
    call write_stdout
    lea rsi, [rel net_buf + NET_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel no_ipv4_suffix_msg]
    mov rdx, no_ipv4_suffix_msg_end - no_ipv4_suffix_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

dns_fail:
    lea rsi, [rel dns_fail_msg]
    mov rdx, dns_fail_msg_end - dns_fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

arp_fail:
    lea rsi, [rel arp_fail_msg]
    mov rdx, arp_fail_msg_end - arp_fail_msg
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

icmp_send_fail:
    lea rsi, [rel icmp_send_fail_msg]
    mov rdx, icmp_send_fail_msg_end - icmp_send_fail_msg
    call write_stdout
    mov rax, SYS_EXIT
    mov rdi, 1
    syscall

print_ping_header:
    lea rsi, [rel ping_msg]
    mov rdx, ping_msg_end - ping_msg
    call write_stdout
    lea rsi, [rel target_text]
    call write_cstring_stdout
    lea rsi, [rel via_msg]
    mov rdx, via_msg_end - via_msg
    call write_stdout
    lea rsi, [rel net_buf + NET_NAME_OFF]
    call write_cstring_stdout
    lea rsi, [rel driver_open_msg]
    mov rdx, driver_open_msg_end - driver_open_msg
    call write_stdout
    lea rsi, [rel net_buf + NET_DRIVER_OFF]
    call write_cstring_stdout
    lea rsi, [rel driver_close_msg]
    mov rdx, driver_close_msg_end - driver_close_msg
    jmp write_stdout

is_dash_i:
    xor eax, eax
    test rsi, rsi
    jz .done
    cmp byte [rsi], '-'
    jne .done
    cmp byte [rsi + 1], 'i'
    jne .done
    cmp byte [rsi + 2], 0
    jne .done
    mov eax, 1
.done:
    ret

parse_iface_index:
    test rsi, rsi
    jz .bad
    mov al, [rsi]
    cmp al, '0'
    jb .bad
    cmp al, '7'
    ja .bad
    cmp byte [rsi + 1], 0
    jne .bad
    sub al, '0'
    movzx eax, al
    ret
.bad:
    mov eax, -1
    ret

detect_dns_mode:
    mov byte [rel dns_mode], 0
    test rsi, rsi
    jz .done
    cmp byte [rsi], 'd'
    jne .check_nslookup
    cmp byte [rsi + 1], 'n'
    jne .done
    cmp byte [rsi + 2], 's'
    jne .done
    cmp byte [rsi + 3], 0
    jne .done
    mov byte [rel dns_mode], 1
    ret
.check_nslookup:
    cmp byte [rsi], 'n'
    jne .done
    cmp byte [rsi + 1], 's'
    jne .done
    cmp byte [rsi + 2], 'l'
    jne .done
    cmp byte [rsi + 3], 'o'
    jne .done
    cmp byte [rsi + 4], 'o'
    jne .done
    cmp byte [rsi + 5], 'k'
    jne .done
    cmp byte [rsi + 6], 'u'
    jne .done
    cmp byte [rsi + 7], 'p'
    jne .done
    cmp byte [rsi + 8], 0
    jne .done
    mov byte [rel dns_mode], 1
.done:
    ret

copy_target_text:
    lea rdi, [rel target_text]
    mov rcx, 127
    jmp copy_cstr_limited

copy_domain_text:
    lea rdi, [rel domain_text]
    mov rcx, 127
    jmp copy_cstr_limited

copy_cstr_limited:
.loop:
    cmp rcx, 0
    je .finish
    mov al, [rsi]
    test al, al
    jz .finish
    mov [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jmp .loop
.finish:
    mov byte [rdi], 0
    ret

parse_ipv4:
    xor r8d, r8d
    xor r9d, r9d
    xor ebx, ebx
.loop:
    mov al, [rsi]
    test al, al
    jz .end
    cmp al, '.'
    je .dot
    cmp al, '0'
    jb .bad
    cmp al, '9'
    ja .bad
    sub al, '0'
    movzx edx, al
    imul ebx, ebx, 10
    add ebx, edx
    cmp ebx, 255
    ja .bad
    inc r9d
    cmp r9d, 3
    ja .bad
    inc rsi
    jmp .loop
.dot:
    cmp r9d, 0
    je .bad
    cmp r8d, 3
    jae .bad
    lea rdx, [rel target_ip]
    mov [rdx + r8], bl
    inc r8d
    xor r9d, r9d
    xor ebx, ebx
    inc rsi
    jmp .loop
.end:
    cmp r9d, 0
    je .bad
    cmp r8d, 3
    jne .bad
    lea rdx, [rel target_ip]
    mov [rdx + r8], bl
    xor eax, eax
    ret
.bad:
    mov eax, 1
    ret

resolve_dns_name:
    mov rax, AOS_SYS_DNS_LOOKUP
    lea rdi, [rel domain_text]
    lea rsi, [rel target_ip]
    mov rdx, [rel iface_index]
    syscall
    test rax, rax
    js .fail
    lea rsi, [rel dns_reply_msg]
    mov rdx, dns_reply_msg_end - dns_reply_msg
    call write_stdout
    lea rsi, [rel domain_text]
    call write_cstring_stdout
    lea rsi, [rel arrow_msg]
    mov rdx, arrow_msg_end - arrow_msg
    call write_stdout
    lea rbx, [rel target_ip]
    call write_ipv4_from_rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    xor eax, eax
    ret
.fail:
    mov eax, 1
    ret

choose_ping_arp_ip:
    mov al, [rel target_ip]
    cmp al, [rel net_buf + NET_IPV4_OFF]
    jne .gateway
    mov al, [rel target_ip + 1]
    cmp al, [rel net_buf + NET_IPV4_OFF + 1]
    jne .gateway
    mov al, [rel target_ip + 2]
    cmp al, [rel net_buf + NET_IPV4_OFF + 2]
    jne .gateway
    lea rsi, [rel target_ip]
    jmp .copy
.gateway:
    lea rsi, [rel net_buf + NET_GATEWAY_OFF]
.copy:
    lea rdi, [rel arp_ip]
    jmp copy4

resolve_arp:
    call build_arp_request
    mov rax, AOS_SYS_NETDEV_SEND
    mov rdi, [rel iface_index]
    lea rsi, [rel tx_frame]
    mov rdx, 60
    syscall
    test rax, rax
    js .fail

    lea rsi, [rel arp_msg]
    mov rdx, arp_msg_end - arp_msg
    call write_stdout
    lea rbx, [rel arp_ip]
    call write_ipv4_from_rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    mov ecx, 250000
.recv_loop:
    push rcx
    mov rax, AOS_SYS_NETDEV_RECV
    mov rdi, [rel iface_index]
    lea rsi, [rel rx_frame]
    mov rdx, RX_SIZE
    syscall
    pop rcx
    test rax, rax
    js .fail
    cmp rax, 42
    jb .next
    call is_arp_reply
    test eax, eax
    jz .ok
.next:
    loop .recv_loop
.fail:
    mov eax, 1
    ret
.ok:
    call save_arp_mac
    lea rsi, [rel reply_msg]
    mov rdx, reply_msg_end - reply_msg
    call write_stdout
    lea rbx, [rel target_mac]
    call write_mac_from_rbx
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    xor eax, eax
    ret

build_arp_request:
    lea rbx, [rel tx_frame]
    xor ecx, ecx
.dst:
    mov byte [rbx + rcx], 0xff
    inc ecx
    cmp ecx, 6
    jb .dst
    lea rsi, [rel net_buf + NET_MAC_OFF]
    xor ecx, ecx
.src:
    mov al, [rsi + rcx]
    mov [rbx + 6 + rcx], al
    inc ecx
    cmp ecx, 6
    jb .src
    mov byte [rbx + 12], 0x08
    mov byte [rbx + 13], 0x06
    mov byte [rbx + 14], 0x00
    mov byte [rbx + 15], 0x01
    mov byte [rbx + 16], 0x08
    mov byte [rbx + 17], 0x00
    mov byte [rbx + 18], 0x06
    mov byte [rbx + 19], 0x04
    mov byte [rbx + 20], 0x00
    mov byte [rbx + 21], 0x01
    xor ecx, ecx
.sha:
    mov al, [rsi + rcx]
    mov [rbx + 22 + rcx], al
    inc ecx
    cmp ecx, 6
    jb .sha
    mov eax, [rel net_buf + NET_IPV4_OFF]
    mov [rbx + 28], eax
    xor ecx, ecx
.zero:
    mov byte [rbx + 32 + rcx], 0
    inc ecx
    cmp ecx, 6
    jb .zero
    mov eax, [rel arp_ip]
    mov [rbx + 38], eax
    mov ecx, 42
.pad:
    cmp ecx, 60
    jae .done
    mov byte [rbx + rcx], 0
    inc ecx
    jmp .pad
.done:
    ret

is_arp_reply:
    lea rbx, [rel rx_frame]
    cmp byte [rbx + 12], 0x08
    jne .no
    cmp byte [rbx + 13], 0x06
    jne .no
    cmp byte [rbx + 20], 0x00
    jne .no
    cmp byte [rbx + 21], 0x02
    jne .no
    mov eax, [rel arp_ip]
    cmp [rbx + 28], eax
    jne .no
    mov eax, [rel net_buf + NET_IPV4_OFF]
    cmp [rbx + 38], eax
    jne .no
    xor eax, eax
    ret
.no:
    mov eax, 1
    ret

save_arp_mac:
    lea rsi, [rel rx_frame + 22]
    lea rdi, [rel target_mac]
    mov ecx, 6
.loop:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    loop .loop
    ret

build_dns_query:
    lea rbx, [rel tx_frame]
    lea rsi, [rel target_mac]
    xor ecx, ecx
.dst:
    mov al, [rsi + rcx]
    mov [rbx + rcx], al
    inc ecx
    cmp ecx, 6
    jb .dst
    lea rsi, [rel net_buf + NET_MAC_OFF]
    xor ecx, ecx
.src:
    mov al, [rsi + rcx]
    mov [rbx + 6 + rcx], al
    inc ecx
    cmp ecx, 6
    jb .src
    mov byte [rbx + 12], 0x08
    mov byte [rbx + 13], 0x00

    mov byte [rbx + 14], 0x45
    mov byte [rbx + 15], 0x00
    mov byte [rbx + 18], 0xa0
    mov byte [rbx + 19], 0x52
    mov byte [rbx + 20], 0x00
    mov byte [rbx + 21], 0x00
    mov byte [rbx + 22], 64
    mov byte [rbx + 23], 17
    mov byte [rbx + 24], 0x00
    mov byte [rbx + 25], 0x00
    mov eax, [rel net_buf + NET_IPV4_OFF]
    mov [rbx + 26], eax
    mov eax, [rel net_buf + NET_DNS_OFF]
    mov [rbx + 30], eax

    mov byte [rbx + 34], 0xc0
    mov byte [rbx + 35], 0x00
    mov byte [rbx + 36], 0x00
    mov byte [rbx + 37], 0x35
    mov byte [rbx + 40], 0x00
    mov byte [rbx + 41], 0x00

    lea rdi, [rel tx_frame + 42]
    mov byte [rdi + 0], 0xa0
    mov byte [rdi + 1], 0x55
    mov byte [rdi + 2], 0x01
    mov byte [rdi + 3], 0x00
    mov byte [rdi + 4], 0x00
    mov byte [rdi + 5], 0x01
    mov byte [rdi + 6], 0x00
    mov byte [rdi + 7], 0x00
    mov byte [rdi + 8], 0x00
    mov byte [rdi + 9], 0x00
    mov byte [rdi + 10], 0x00
    mov byte [rdi + 11], 0x00
    lea rdi, [rel tx_frame + 54]
    lea rsi, [rel domain_text]
    call encode_qname
    mov r10, rax
    lea rdi, [rel tx_frame + 54]
    mov byte [rdi + r10], 0x00
    mov byte [rdi + r10 + 1], 0x01
    mov byte [rdi + r10 + 2], 0x00
    mov byte [rdi + r10 + 3], 0x01
    add r10, 4
    mov r11, r10
    add r11, 12
    mov r12, r11
    add r12, 8
    mov rax, r12
    shr rax, 8
    mov [rel tx_frame + 38], al
    mov [rel tx_frame + 39], r12b
    add r12, 20
    mov rax, r12
    shr rax, 8
    mov [rel tx_frame + 16], al
    mov [rel tx_frame + 17], r12b
    lea rsi, [rel tx_frame + 14]
    mov rcx, 20
    call checksum16
    mov [rel tx_frame + 24], ah
    mov [rel tx_frame + 25], al
    mov rax, r12
    add rax, 14
    cmp rax, 60
    jae .len_ok
    mov rax, 60
.len_ok:
    mov [rel tx_len], ax
    ret

encode_qname:
    xor r8, r8
    mov r9, rdi
.label_start:
    mov r10, rdi
    inc rdi
    xor ecx, ecx
.label_loop:
    mov al, [rsi]
    test al, al
    jz .finish_label
    cmp al, '.'
    je .finish_label_dot
    mov [rdi], al
    inc rdi
    inc rsi
    inc ecx
    inc r8
    cmp ecx, 63
    jb .label_loop
.finish_label:
    mov [r10], cl
    mov byte [rdi], 0
    inc rdi
    inc r8
    mov rax, rdi
    sub rax, r9
    ret
.finish_label_dot:
    mov [r10], cl
    inc rsi
    inc r8
    jmp .label_start

parse_dns_response:
    lea rbx, [rel rx_frame]
    cmp byte [rbx + 12], 0x08
    jne .bad
    cmp byte [rbx + 13], 0x00
    jne .bad
    cmp byte [rbx + 23], 17
    jne .bad
    mov eax, [rel net_buf + NET_DNS_OFF]
    cmp [rbx + 26], eax
    jne .bad
    mov eax, [rel net_buf + NET_IPV4_OFF]
    cmp [rbx + 30], eax
    jne .bad
    cmp byte [rbx + 34], 0x00
    jne .bad
    cmp byte [rbx + 35], 0x35
    jne .bad
    cmp byte [rbx + 36], 0xc0
    jne .bad
    cmp byte [rbx + 37], 0x00
    jne .bad
    cmp byte [rbx + 42], 0xa0
    jne .bad
    cmp byte [rbx + 43], 0x55
    jne .bad
    movzx r9d, byte [rbx + 49]
    test r9d, r9d
    jz .bad
    lea rsi, [rel rx_frame + 54]
.skip_q:
    mov al, [rsi]
    test al, al
    jz .q_done
    movzx edx, al
    inc rsi
    add rsi, rdx
    jmp .skip_q
.q_done:
    add rsi, 5
.answer_loop:
    cmp r9d, 0
    je .bad
    mov al, [rsi]
    test al, 0xc0
    jz .skip_name_full
    add rsi, 2
    jmp .name_done
.skip_name_full:
    mov al, [rsi]
    test al, al
    jz .name_full_done
    movzx edx, al
    inc rsi
    add rsi, rdx
    jmp .skip_name_full
.name_full_done:
    inc rsi
.name_done:
    cmp byte [rsi + 0], 0x00
    jne .skip_answer
    cmp byte [rsi + 1], 0x01
    jne .skip_answer
    cmp byte [rsi + 2], 0x00
    jne .skip_answer
    cmp byte [rsi + 3], 0x01
    jne .skip_answer
    cmp byte [rsi + 8], 0x00
    jne .skip_answer
    cmp byte [rsi + 9], 0x04
    jne .skip_answer
    mov eax, [rsi + 10]
    mov [rel target_ip], eax
    xor eax, eax
    ret
.skip_answer:
    cmp byte [rsi + 0], 0x00
    jne .skip_plain
    cmp byte [rsi + 1], 0x05
    jne .skip_plain
    cmp byte [rsi + 2], 0x00
    jne .skip_plain
    cmp byte [rsi + 3], 0x01
    jne .skip_plain
    lea rdi, [rel domain_text]
    lea rsi, [rsi + 10]
    call decode_dns_name
    mov eax, 2
    ret
.skip_plain:
    movzx edx, byte [rsi + 8]
    shl edx, 8
    movzx eax, byte [rsi + 9]
    or edx, eax
    add rsi, 10
    add rsi, rdx
    dec r9d
    jmp .answer_loop
.bad:
    mov eax, 1
    ret

decode_dns_name:
    xor r8d, r8d
    xor r11d, r11d
.part:
    cmp r8d, 126
    jae .finish
    cmp r11d, 24
    jae .finish
    inc r11d
    mov al, [rsi]
    test al, al
    jz .finish
    test al, 0xc0
    jz .label
    movzx edx, al
    and edx, 0x3f
    shl edx, 8
    movzx eax, byte [rsi + 1]
    or edx, eax
    lea rsi, [rel rx_frame + 42]
    add rsi, rdx
    jmp .part
.label:
    movzx ecx, al
    inc rsi
    cmp r8d, 0
    je .copy_label
    mov byte [rdi], '.'
    inc rdi
    inc r8d
.copy_label:
    cmp ecx, 0
    je .part
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    inc r8d
    dec ecx
    jmp .copy_label
.finish:
    mov byte [rdi], 0
    ret

build_icmp_echo:
    lea rbx, [rel tx_frame]
    lea rsi, [rel target_mac]
    xor ecx, ecx
.dst:
    mov al, [rsi + rcx]
    mov [rbx + rcx], al
    inc ecx
    cmp ecx, 6
    jb .dst
    lea rsi, [rel net_buf + NET_MAC_OFF]
    xor ecx, ecx
.src:
    mov al, [rsi + rcx]
    mov [rbx + 6 + rcx], al
    inc ecx
    cmp ecx, 6
    jb .src
    mov byte [rbx + 12], 0x08
    mov byte [rbx + 13], 0x00
    mov byte [rbx + 14], 0x45
    mov byte [rbx + 15], 0x00
    mov byte [rbx + 16], 0x00
    mov byte [rbx + 17], 0x24
    mov byte [rbx + 18], 0xa0
    mov byte [rbx + 19], 0x51
    mov byte [rbx + 20], 0x00
    mov byte [rbx + 21], 0x00
    mov byte [rbx + 22], 64
    mov byte [rbx + 23], 1
    mov byte [rbx + 24], 0x00
    mov byte [rbx + 25], 0x00
    mov eax, [rel net_buf + NET_IPV4_OFF]
    mov [rbx + 26], eax
    mov eax, [rel target_ip]
    mov [rbx + 30], eax
    lea rsi, [rel tx_frame + 14]
    mov rcx, 20
    call checksum16
    mov [rbx + 24], ah
    mov [rbx + 25], al
    mov byte [rbx + 34], 8
    mov byte [rbx + 35], 0
    mov byte [rbx + 36], 0x00
    mov byte [rbx + 37], 0x00
    mov byte [rbx + 38], 0xa0
    mov byte [rbx + 39], 0x05
    mov byte [rbx + 40], 0x00
    mov byte [rbx + 41], 0x01
    mov byte [rbx + 42], 'A'
    mov byte [rbx + 43], 'O'
    mov byte [rbx + 44], 'S'
    mov byte [rbx + 45], 'P'
    mov byte [rbx + 46], 'I'
    mov byte [rbx + 47], 'N'
    mov byte [rbx + 48], 'G'
    mov byte [rbx + 49], '!'
    lea rsi, [rel tx_frame + 34]
    mov rcx, 16
    call checksum16
    mov [rbx + 36], ah
    mov [rbx + 37], al
    mov ecx, 50
.pad:
    cmp ecx, 60
    jae .done
    mov byte [rbx + rcx], 0
    inc ecx
    jmp .pad
.done:
    ret

is_icmp_echo_reply:
    lea rbx, [rel rx_frame]
    cmp byte [rbx + 12], 0x08
    jne .no
    cmp byte [rbx + 13], 0x00
    jne .no
    cmp byte [rbx + 14], 0x45
    jne .no
    cmp byte [rbx + 23], 1
    jne .no
    mov eax, [rel target_ip]
    cmp [rbx + 26], eax
    jne .no
    mov eax, [rel net_buf + NET_IPV4_OFF]
    cmp [rbx + 30], eax
    jne .no
    cmp byte [rbx + 34], 0
    jne .no
    cmp byte [rbx + 35], 0
    jne .no
    cmp byte [rbx + 38], 0xa0
    jne .no
    cmp byte [rbx + 39], 0x05
    jne .no
    cmp byte [rbx + 40], 0x00
    jne .no
    cmp byte [rbx + 41], 0x01
    jne .no
    xor eax, eax
    ret
.no:
    mov eax, 1
    ret

checksum16:
    xor eax, eax
.sum:
    cmp rcx, 0
    je .fold
    movzx edx, byte [rsi]
    shl edx, 8
    movzx edi, byte [rsi + 1]
    or edx, edi
    add eax, edx
    add rsi, 2
    sub rcx, 2
    jmp .sum
.fold:
    mov edx, eax
    shr edx, 16
    and eax, 0xffff
    add eax, edx
    mov edx, eax
    shr edx, 16
    and eax, 0xffff
    add eax, edx
    not ax
    ret

copy4:
    mov eax, [rsi]
    mov [rdi], eax
    ret

write_ipv4_from_rbx:
    push rbx
    movzx edi, byte [rbx]
    call write_u64
    pop rbx
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    push rbx
    movzx edi, byte [rbx + 1]
    call write_u64
    pop rbx
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    push rbx
    movzx edi, byte [rbx + 2]
    call write_u64
    pop rbx
    lea rsi, [rel dot_msg]
    mov rdx, dot_msg_end - dot_msg
    call write_stdout
    movzx edi, byte [rbx + 3]
    jmp write_u64

write_mac_from_rbx:
    movzx edi, byte [rbx]
    call write_hex8
    mov r15, 1
.loop:
    lea rsi, [rel colon_msg]
    mov rdx, colon_msg_end - colon_msg
    call write_stdout
    movzx edi, byte [rbx + r15]
    push r15
    call write_hex8
    pop r15
    inc r15
    cmp r15, 6
    jb .loop
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

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

section .bss
net_buf:
    resb NET_STRUCT_SIZE
target_ip:
    resb 4
arp_ip:
    resb 4
target_mac:
    resb 6
tx_len:
    resw 1
iface_index:
    resq 1
dns_mode:
    resb 1
dns_depth:
    resb 1
dns_tries:
    resb 1
target_text:
    resb 128
domain_text:
    resb 128
tx_frame:
    resb TX_SIZE
rx_frame:
    resb RX_SIZE
one_char:
    resb 1
num_buf:
    resb 21

section .rodata
usage_msg:
    db "usage: ping [-i IFACE] HOST", 10
    db "examples: ping 10.0.2.2 | ping -i 1 10.0.2.2 | ping google.com", 10
usage_msg_end:

no_netdev_msg:
    db "ping: no network interface is registered", 10
no_netdev_msg_end:

link_down_msg:
    db "ping: network link is down", 10
link_down_msg_end:

no_ipv4_prefix_msg:
    db "ping: "
no_ipv4_prefix_msg_end:

no_ipv4_suffix_msg:
    db " has no IPv4 address", 10
no_ipv4_suffix_msg_end:

dns_fail_msg:
    db "ping: DNS lookup failed", 10
dns_fail_msg_end:

arp_fail_msg:
    db "ping: ARP failed", 10
arp_fail_msg_end:

recv_fail_msg:
    db "ping: network receive failed", 10
recv_fail_msg_end:

icmp_send_fail_msg:
    db "ping: ICMP send failed", 10
icmp_send_fail_msg_end:

icmp_timeout_msg:
    db "ping: timeout waiting for ICMP echo reply", 10
icmp_timeout_msg_end:

ping_msg:
    db "PING "
ping_msg_end:

via_msg:
    db " via "
via_msg_end:

driver_open_msg:
    db " ("
driver_open_msg_end:

driver_close_msg:
    db ")", 10
driver_close_msg_end:

arp_msg:
    db "arp: who-has "
arp_msg_end:

reply_msg:
    db "arp: reply at "
reply_msg_end:

dns_query_msg:
    db "dns: query "
dns_query_msg_end:

dns_reply_msg:
    db "dns: "
dns_reply_msg_end:

arrow_msg:
    db " -> "
arrow_msg_end:

icmp_reply_msg:
    db "64 bytes from "
icmp_reply_msg_end:

icmp_tail_msg:
    db ": icmp_seq=1 ttl=64", 10
icmp_tail_msg_end:

newline_msg:
    db 10
newline_msg_end:

dot_msg:
    db "."
dot_msg_end:

colon_msg:
    db ":"
colon_msg_end:

hex_digits:
    db "0123456789abcdef"
