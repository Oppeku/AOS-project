; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

extern generic_handler
extern invalid_opcode_handler
extern gpf_handler
extern page_fault_handler
extern timer_handler
extern keyboard_handler_main

global load_idt
global generic_stub
global invalid_opcode_stub
global gpf_stub
global page_fault_stub
global keyboard_stub
global timer_stub

load_idt:
    lidt [rdi]
    ret

%macro push_all 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro pop_all 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro

generic_stub:
    push_all
    call generic_handler
    pop_all
    iretq

timer_stub:
    push_all
    call timer_handler
    ; Send EOI
    mov al, 0x20
    out 0x20, al
    pop_all
    iretq

keyboard_stub:
    push_all
    call keyboard_handler_main
    ; Send EOI
    mov al, 0x20
    out 0x20, al
    pop_all
    iretq

invalid_opcode_stub:
    swapgs
    push_all
    call invalid_opcode_handler
    pop_all
    swapgs
    iretq

gpf_stub:
    ; Stack: [ErrorCode] <- RSP
    xchg [rsp], rax
    push_all
    call gpf_handler
    pop_all
    add rsp, 8    
    iretq

page_fault_stub:
    xchg [rsp], rax
    push_all
    call page_fault_handler
    pop_all
    add rsp, 8    
    iretq

section .note.GNU-stack noalloc noexec nowrite progbits
