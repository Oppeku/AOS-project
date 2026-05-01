; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]
extern syscall_handler
global init_syscall
global syscall_entry

; MSR Addresses
EFER_MSR        equ 0xC0000080
STAR_MSR        equ 0xC0000081
LSTAR_MSR       equ 0xC0000082
SFMASK_MSR      equ 0xC0000084

section .text

init_syscall:
    ; 1. Enable System Call Extensions (SCE)
    mov ecx, EFER_MSR
    rdmsr
    or eax, 1           ; Set SCE bit
    wrmsr

    ; 2. Set the kernel's syscall entry point
    mov rax, syscall_entry
    mov rdx, rax
    shr rdx, 32
    mov ecx, LSTAR_MSR
    wrmsr

    ; 3. Set STAR: Kernel CS 0x08, User CS base 0x10
    ; SYSRET will use 0x10+8=0x18 for SS and 0x10+16=0x20 for CS
    xor rax, rax
    mov edx, 0x00100008 
    mov ecx, STAR_MSR
    wrmsr

    ; 4. Set SFMASK: Mask the Interrupt Flag (Disable IRQs during syscall)
    mov rax, 0x200
    xor rdx, rdx
    mov ecx, SFMASK_MSR
    wrmsr
    ret

syscall_entry:
    swapgs                  ; GS now points to kernel's cpu_context
    mov [gs:0x08], rsp      ; Save user stack into cpu0.user_stack
    mov rsp, [gs:0x00]      ; Load kernel stack from cpu0.kernel_stack

    ; Create syscall_regs on stack
    ; Order (top down):
    ; rax (0), rdx (8), rsi (16), rdi (24), r10 (32), r8 (40), r9 (48), 
    ; r15 (56), r14 (64), r13 (72), r12 (80), rbx (88), rbp (96), 
    ; rcx (104), r11 (112), rsp (120)

    push qword [gs:0x08] ; rsp
    push r11 ; rflags
    push rcx ; rip
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push r9
    push r8
    push r10
    push rdi
    push rsi
    push rdx
    push rax

    ; Set kernel data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rdi, rsp ; Pass syscall_regs*
    call syscall_handler

    ; Restore registers (skipping rsp)
    pop rax
    pop rdx
    pop rsi
    pop rdi
    pop r10
    pop r8
    pop r9
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx ; rip
    pop r11 ; rflags
    add rsp, 8 ; skip rsp

    ; Prepare for sysretq
    mov rsp, [gs:0x08]      ; Restore user stack
    
    ; Restore user segments
    push rax
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    pop rax
    ; SS is handled by sysretq (STAR MSR)
    
    swapgs
    o64 sysret              ; Return to 64-bit userspace without truncating RIP
