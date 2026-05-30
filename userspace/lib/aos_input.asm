; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

global aos_input_poll

%define AOS_SYS_INPUT_POLL 525

aos_input_poll:
    mov rax, AOS_SYS_INPUT_POLL
    syscall
    ret
