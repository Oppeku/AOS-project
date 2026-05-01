; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]
global jump_to_user
global switch_to_process

section .text

; process_t offsets:
; pid: 0
; parent_pid: 4
; status: 8
; exit_status: 12
; p4_table: 16
; regs: 24
;   rax (0), rdx (8), rsi (16), rdi (24), r10 (32), r8 (40), r9 (48), 
;   r15 (56), r14 (64), r13 (72), r12 (80), rbx (88), rbp (96), 
;   rcx (104), r11 (112), rsp (120)
; kernel_stack: 152

switch_to_process:
    ; RDI = process_t* next
    
    ; 1. Load Page Table
    mov rax, [rdi + 16]
    mov cr3, rax

    ; 2. Restore all registers from the process struct
    ; We'll use RDI as base, then restore it last.
    
    mov rax, [rdi + 24 + 0]
    mov rdx, [rdi + 24 + 8]
    mov rsi, [rdi + 24 + 16]
    mov r10, [rdi + 24 + 32]
    mov r8,  [rdi + 24 + 40]
    mov r9,  [rdi + 24 + 48]
    mov r15, [rdi + 24 + 56]
    mov r14, [rdi + 24 + 64]
    mov r13, [rdi + 24 + 72]
    mov r12, [rdi + 24 + 80]
    mov rbx, [rdi + 24 + 88]
    mov rbp, [rdi + 24 + 96]
    mov rcx, [rdi + 24 + 104] ; User RIP
    mov r11, [rdi + 24 + 112] ; User RFLAGS
    
    ; Save User RSP into GS:0x08 so it can be restored or used later
    mov rbx, [rdi + 24 + 120]
    swapgs
    mov [gs:0x08], rbx
    
    ; Now we need to actually set RSP to the user stack pointer
    mov rsp, rbx

    ; Restore original RBX from struct
    mov rbx, [rdi + 24 + 88]

    ; Restore RDI last
    mov rdi, [rdi + 24 + 24]

    ; SYSRET enters user mode using:
    ; RIP = RCX
    ; RFLAGS = R11
    ; CS/SS from STAR MSR
    o64 sysret

jump_to_user:
    ; RDI: RIP, RSI: RSP
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x1B ; SS
    push rsi  ; RSP
    push 0x202; RFLAGS
    push 0x23 ; CS
    push rdi  ; RIP
    iretq
