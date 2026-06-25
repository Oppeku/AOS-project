; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) 2026 Oppeko

[BITS 64]

section .text
global _start

%define SYS_READ 0
%define SYS_WRITE 1
%define SYS_CLOSE 3
%define SYS_LSEEK 8
%define SYS_PIPE 22
%define SYS_DUP 32
%define SYS_DUP2 33
%define SYS_FORK 57
%define SYS_EXECVE 59
%define SYS_EXIT 60
%define SYS_WAIT4 61
%define SYS_GETCWD 79
%define SYS_CHDIR 80
%define SYS_MKDIRAT 258
%define SYS_UNLINKAT 263
%define SYS_GETDENTS64 217
%define SYS_OPENAT 257
%define AOS_SYS_PROCESS_INFO 544

%define AT_FDCWD -100
%define O_DIRECTORY 0x10000
%define O_WRONLY 1
%define O_CREAT 64
%define O_TRUNC 512
%define SEEK_END 2
%define CMD_BUF_SIZE 256
%define IO_BUF_SIZE 512
%define MAX_ARGS 8
%define MAX_PIPE_CMDS 8
%define HISTORY_SIZE 8
%define HOME_EXPANSION_SIZE (MAX_PIPE_CMDS * MAX_ARGS * CMD_BUF_SIZE)
%define KEY_HISTORY_PREV 0x11
%define KEY_HISTORY_NEXT 0x12
%define ARGV_STRIDE ((MAX_ARGS + 1) * 8)
%define VGA_COLOR_PROMPT 0x0A
%define VGA_COLOR_INPUT 0x0F
%define VGA_COLOR_OUTPUT 0x07
%define COMPLETION_MODE_COLLECT 0
%define COMPLETION_MODE_PRINT 1
%define BUILTIN_CMD_COUNT 15
%define MAX_PS_PROCESSES 64
%define PS_VALID_OFF 0
%define PS_STATUS_OFF 1
%define PS_PID_OFF 4
%define PS_PPID_OFF 8
%define PS_UID_OFF 12
%define PS_EUID_OFF 16
%define PS_USERNAME_OFF 40
%define PS_COMMAND_OFF 72

_start:
    xor r12, r12
    call clear_vga_console
    call set_output_color

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel welcome_msg]
    mov rdx, welcome_msg_end - welcome_msg
    syscall

shell_prompt:
    xor r12, r12
    mov byte [rel line_buffer], 0
    mov qword [rel history_browse], -1
    call write_prompt

shell_read_char:
    sub rsp, 8

    mov rax, SYS_READ
    mov rdi, 0
    mov rsi, rsp
    mov rdx, 1
    syscall

    test rax, rax
    jle shell_no_input

    mov al, [rsp]
    cmp al, 8
    je shell_handle_backspace
    cmp al, KEY_HISTORY_PREV
    je shell_handle_history_prev
    cmp al, KEY_HISTORY_NEXT
    je shell_handle_history_next
    cmp al, 9
    je shell_handle_tab
    cmp al, 10
    je shell_handle_newline
    cmp al, 13
    je shell_handle_newline

    cmp r12, CMD_BUF_SIZE - 1
    jae shell_discard_char

    lea rbx, [rel line_buffer]
    mov [rbx + r12], al
    inc r12

    mov rax, SYS_WRITE
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall

    add rsp, 8
    jmp shell_read_char

shell_handle_newline:
    call set_output_color
    mov rax, SYS_WRITE
    mov rdi, 1
    mov rsi, rsp
    mov rdx, 1
    syscall

    lea rbx, [rel line_buffer]
    mov byte [rbx + r12], 0
    call store_history_line
    add rsp, 8
    call execute_line
    jmp shell_prompt

shell_handle_backspace:
    cmp r12, 0
    je shell_discard_char

    dec r12
    lea rbx, [rel line_buffer]
    mov byte [rbx + r12], 0

    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel backspace_seq]
    mov rdx, backspace_seq_end - backspace_seq
    syscall

shell_discard_char:
    add rsp, 8
    jmp shell_read_char

shell_handle_history_prev:
    call history_prev
    add rsp, 8
    jmp shell_read_char

shell_handle_history_next:
    call history_next
    add rsp, 8
    jmp shell_read_char

shell_handle_tab:
    call tab_complete_current_line
    add rsp, 8
    jmp shell_read_char

shell_no_input:
    add rsp, 8
    jmp shell_read_char

execute_line:
    lea rax, [rel home_expansion_buffer]
    mov [rel home_expansion_next], rax
    lea rbx, [rel line_buffer]
    call split_pipeline_commands
    mov [rel command_count], r14
    test r14, r14
    jz execute_line_return

    xor r12, r12

execute_parse_loop:
    mov rax, r12
    imul rax, ARGV_STRIDE
    lea r15, [rel argv_table]
    add r15, rax
    lea r11, [rel command_starts]
    mov rbx, [r11 + r12 * 8]
    call parse_args
    call expand_argv_home
    mov rdi, r12
    call extract_redirections
    lea r11, [rel argc_table]
    mov [r11 + r12 * 8], r13
    test r13, r13
    jz execute_pipeline_usage
    inc r12
    cmp r12, r14
    jb execute_parse_loop

    cmp r14, 1
    jne execute_multi_pipeline

    lea r15, [rel argv_table]
    mov r13, [rel argc_table]
    call run_foreground_command
    jmp execute_line_return

execute_multi_pipeline:
    call run_pipeline
    jmp execute_line_return

execute_pipeline_usage:
    lea rsi, [rel pipeline_usage_msg]
    mov rdx, pipeline_usage_msg_end - pipeline_usage_msg
    call write_stdout

execute_line_return:
    ret

split_pipeline_commands:
    xor r14, r14
    xor edx, edx

split_skip_ws:
    mov al, [rbx]
    cmp al, ' '
    je split_advance_ws
    cmp al, 9
    je split_advance_ws
    jmp split_check_token

split_advance_ws:
    inc rbx
    jmp split_skip_ws

split_check_token:
    test al, al
    jz split_done
    cmp r14, MAX_PIPE_CMDS
    jae split_done

    lea r11, [rel command_starts]
    mov [r11 + r14 * 8], rbx
    inc r14

split_scan:
    mov al, [rbx]
    test al, al
    jz split_done
    cmp al, 39
    je split_single_quote
    cmp al, 34
    je split_double_quote
    test dl, dl
    jnz split_scan_advance
    cmp al, '|'
    je split_found_pipe

split_scan_advance:
    inc rbx
    jmp split_scan

split_single_quote:
    cmp dl, 2
    je split_scan_advance
    cmp dl, 1
    je split_leave_single
    mov dl, 1
    jmp split_scan_advance

split_leave_single:
    xor edx, edx
    jmp split_scan_advance

split_double_quote:
    cmp dl, 1
    je split_scan_advance
    cmp dl, 2
    je split_leave_double
    mov dl, 2
    jmp split_scan_advance

split_leave_double:
    xor edx, edx
    jmp split_scan_advance

split_found_pipe:
    mov byte [rbx], 0
    inc rbx
    xor edx, edx
    jmp split_skip_ws

split_done:
    ret

parse_args:
    xor r13, r13

parse_skip_ws:
    mov al, [rbx]
    cmp al, ' '
    je parse_advance_ws
    cmp al, 9
    je parse_advance_ws
    jmp parse_check_token

parse_advance_ws:
    inc rbx
    jmp parse_skip_ws

parse_check_token:
    test al, al
    jz parse_done
    cmp r13, MAX_ARGS
    jae parse_done
    cmp al, '<'
    je parse_special_redirect
    cmp al, '>'
    je parse_special_redirect
    cmp al, 39
    je parse_quoted_single
    cmp al, 34
    je parse_quoted_double

    mov [r15 + r13 * 8], rbx
    inc r13

parse_scan_token:
    mov al, [rbx]
    test al, al
    jz parse_done
    cmp al, ' '
    je parse_terminate_token
    cmp al, 9
    je parse_terminate_token
    cmp al, '<'
    je parse_terminate_token
    cmp al, '>'
    je parse_terminate_token
    inc rbx
    jmp parse_scan_token

parse_terminate_token:
    mov byte [rbx], 0
    inc rbx
    jmp parse_skip_ws

parse_quoted_single:
    inc rbx
    mov [r15 + r13 * 8], rbx
    inc r13

parse_scan_single:
    mov al, [rbx]
    test al, al
    jz parse_done
    cmp al, 39
    je parse_end_single
    inc rbx
    jmp parse_scan_single

parse_end_single:
    mov byte [rbx], 0
    inc rbx
    jmp parse_skip_ws

parse_quoted_double:
    inc rbx
    mov [r15 + r13 * 8], rbx
    inc r13

parse_scan_double:
    mov al, [rbx]
    test al, al
    jz parse_done
    cmp al, 34
    je parse_end_double
    inc rbx
    jmp parse_scan_double

parse_end_double:
    mov byte [rbx], 0
    inc rbx
    jmp parse_skip_ws

parse_special_redirect:
    cmp al, '<'
    jne parse_special_output
    lea rax, [rel less_token]
    jmp parse_special_store

parse_special_output:
    cmp byte [rbx + 1], '>'
    jne parse_special_single_output
    lea rax, [rel double_greater_token]
    add rbx, 2
    jmp parse_special_store_done

parse_special_single_output:
    lea rax, [rel greater_token]

parse_special_store:
    mov [r15 + r13 * 8], rax
    inc r13
    inc rbx
    jmp parse_skip_ws

parse_special_store_done:
    mov [r15 + r13 * 8], rax
    inc r13
    jmp parse_skip_ws

parse_done:
    mov qword [r15 + r13 * 8], 0
    ret

run_pipeline:
    mov qword [rel current_read_fd], -1
    xor r12, r12

pipeline_spawn_loop:
    mov rax, [rel command_count]
    cmp r12, rax
    jae pipeline_wait_all

    mov r11, [rel command_count]
    dec r11
    cmp r12, r11
    je pipeline_skip_pipe

    mov rax, SYS_PIPE
    lea rdi, [rel pipe_fds]
    syscall
    test rax, rax
    js pipeline_fail
    jmp pipeline_have_pipe_state

pipeline_skip_pipe:
    mov dword [rel pipe_fds], -1
    mov dword [rel pipe_fds + 4], -1

pipeline_have_pipe_state:
    mov rax, SYS_FORK
    syscall
    test rax, rax
    js pipeline_fail
    jz pipeline_child

    lea r10, [rel child_pids]
    mov [r10 + r12 * 8], rax

    mov eax, [rel current_read_fd]
    cmp eax, -1
    je pipeline_parent_no_prev_close
    mov rax, SYS_CLOSE
    mov edi, [rel current_read_fd]
    syscall

pipeline_parent_no_prev_close:
    mov r11, [rel command_count]
    dec r11
    cmp r12, r11
    je pipeline_parent_last

    mov rax, SYS_CLOSE
    mov edi, [rel pipe_fds + 4]
    syscall
    mov eax, [rel pipe_fds]
    mov [rel current_read_fd], eax
    jmp pipeline_parent_advance

pipeline_parent_last:
    mov qword [rel current_read_fd], -1

pipeline_parent_advance:
    inc r12
    jmp pipeline_spawn_loop

pipeline_child:
    mov eax, [rel current_read_fd]
    cmp eax, -1
    je pipeline_child_skip_input
    mov rax, SYS_DUP2
    mov edi, [rel current_read_fd]
    xor esi, esi
    syscall
    test rax, rax
    js pipeline_child_fail

pipeline_child_skip_input:
    mov r11, [rel command_count]
    dec r11
    cmp r12, r11
    je pipeline_child_skip_output
    mov rax, SYS_DUP2
    mov edi, [rel pipe_fds + 4]
    mov esi, 1
    syscall
    test rax, rax
    js pipeline_child_fail

pipeline_child_skip_output:
    mov eax, [rel current_read_fd]
    cmp eax, -1
    je pipeline_child_skip_prev_close
    mov rax, SYS_CLOSE
    mov edi, [rel current_read_fd]
    syscall

pipeline_child_skip_prev_close:
    mov eax, [rel pipe_fds]
    cmp eax, -1
    je pipeline_child_skip_pipe_read_close
    mov rax, SYS_CLOSE
    mov edi, [rel pipe_fds]
    syscall

pipeline_child_skip_pipe_read_close:
    mov eax, [rel pipe_fds + 4]
    cmp eax, -1
    je pipeline_child_run
    mov rax, SYS_CLOSE
    mov edi, [rel pipe_fds + 4]
    syscall

pipeline_child_run:
    mov rdi, r12
    call apply_command_redirections
    test eax, eax
    js pipeline_child_fail
    lea r10, [rel argc_table]
    mov r13, [r10 + r12 * 8]
    mov rax, r12
    imul rax, ARGV_STRIDE
    lea r15, [rel argv_table]
    add r15, rax
    call run_child_command
    mov edi, eax
    call exit_process

pipeline_wait_all:
    mov eax, [rel current_read_fd]
    cmp eax, -1
    je pipeline_wait_loop_start
    mov rax, SYS_CLOSE
    mov edi, [rel current_read_fd]
    syscall
    mov qword [rel current_read_fd], -1

pipeline_wait_loop_start:
    xor r12, r12

pipeline_wait_loop:
    mov rax, [rel command_count]
    cmp r12, rax
    jae pipeline_success
    lea r10, [rel child_pids]
    mov rdi, [r10 + r12 * 8]
    call wait_for_pid
    inc r12
    jmp pipeline_wait_loop

pipeline_success:
    xor eax, eax
    ret

pipeline_child_fail:
    mov edi, 1
    call exit_process

pipeline_fail:
    mov eax, [rel current_read_fd]
    cmp eax, -1
    je pipeline_fail_close_temp
    mov rax, SYS_CLOSE
    mov edi, [rel current_read_fd]
    syscall
    mov qword [rel current_read_fd], -1

pipeline_fail_close_temp:
    mov eax, [rel pipe_fds]
    cmp eax, -1
    je pipeline_fail_close_write
    mov rax, SYS_CLOSE
    mov edi, [rel pipe_fds]
    syscall

pipeline_fail_close_write:
    mov eax, [rel pipe_fds + 4]
    cmp eax, -1
    je pipeline_fail_wait
    mov rax, SYS_CLOSE
    mov edi, [rel pipe_fds + 4]
    syscall

pipeline_fail_wait:
    xor r11, r11

pipeline_fail_wait_loop:
    cmp r11, r12
    jae pipeline_fail_message
    lea r10, [rel child_pids]
    mov rdi, [r10 + r11 * 8]
    call wait_for_pid
    inc r11
    jmp pipeline_fail_wait_loop

pipeline_fail_message:
    lea rsi, [rel pipeline_pipe_failed_msg]
    mov rdx, pipeline_pipe_failed_msg_end - pipeline_pipe_failed_msg
    call write_stdout
    mov eax, 1
    ret

wait_for_pid:
    mov rax, SYS_WAIT4
    xor rsi, rsi
    xor rdx, rdx
    xor r10, r10
    syscall
    ret

clear_current_line:
    mov r8, r12

clear_current_line_loop:
    test r8, r8
    jz clear_current_line_done
    mov rax, SYS_WRITE
    mov rdi, 1
    lea rsi, [rel backspace_seq]
    mov rdx, backspace_seq_end - backspace_seq
    syscall
    dec r8
    jmp clear_current_line_loop

clear_current_line_done:
    ret

tab_complete_current_line:
    test r12, r12
    jz tab_complete_done

    lea rbx, [rel line_buffer]
    mov rcx, r12

tab_find_token_start:
    test rcx, rcx
    jz tab_have_token_start
    mov al, [rbx + rcx - 1]
    cmp al, ' '
    je tab_have_token_start
    cmp al, 9
    je tab_have_token_start
    cmp al, '|'
    je tab_have_token_start
    dec rcx
    jmp tab_find_token_start

tab_have_token_start:
    mov [rel completion_token_start], rcx
    mov rax, r12
    sub rax, rcx
    mov [rel completion_token_len], rax
    test rax, rax
    jz tab_complete_done

    mov qword [rel completion_match_count], 0
    mov qword [rel completion_common_len], 0
    mov qword [rel completion_mode], COMPLETION_MODE_COLLECT
    call enumerate_completion_candidates

    mov rax, [rel completion_match_count]
    test rax, rax
    jz tab_complete_done

    call append_completion_suffix

    mov rax, [rel completion_match_count]
    cmp rax, 1
    je tab_complete_done

    mov rax, [rel completion_common_len]
    cmp rax, [rel completion_token_len]
    ja tab_complete_done

    call print_completion_matches

tab_complete_done:
    ret

extract_redirections:
    lea r10, [rel input_redir_table]
    mov qword [r10 + rdi * 8], 0
    lea r10, [rel output_redir_table]
    mov qword [r10 + rdi * 8], 0
    lea r10, [rel output_append_table]
    mov qword [r10 + rdi * 8], 0

    xor r11, r11
    xor r8, r8

extract_redir_loop:
    cmp r11, r13
    jae extract_redir_done
    mov rsi, [r15 + r11 * 8]
    mov al, [rsi]
    cmp al, '<'
    je extract_input_redir
    cmp al, '>'
    je extract_output_redir
    mov [r15 + r8 * 8], rsi
    inc r8
    inc r11
    jmp extract_redir_loop

extract_input_redir:
    inc r11
    cmp r11, r13
    jae extract_redir_error
    lea r10, [rel input_redir_table]
    mov rax, [r15 + r11 * 8]
    mov [r10 + rdi * 8], rax
    inc r11
    jmp extract_redir_loop

extract_output_redir:
    cmp byte [rsi + 1], '>'
    jne extract_output_redir_trunc
    lea r10, [rel output_append_table]
    mov qword [r10 + rdi * 8], 1

extract_output_redir_trunc:
    inc r11
    cmp r11, r13
    jae extract_redir_error
    lea r10, [rel output_redir_table]
    mov rax, [r15 + r11 * 8]
    mov [r10 + rdi * 8], rax
    inc r11
    jmp extract_redir_loop

extract_redir_error:
    xor r8, r8

extract_redir_done:
    mov r13, r8
    mov qword [r15 + r13 * 8], 0
    ret

apply_foreground_redirections:
    call apply_command_redirections
    test eax, eax
    js apply_foreground_redirections_fail
    xor eax, eax
    ret

apply_foreground_redirections_fail:
    call restore_foreground_redirections
    mov eax, 1
    ret

apply_command_redirections:
    lea r10, [rel input_redir_table]
    mov r11, [r10 + rdi * 8]
    test r11, r11
    jz apply_command_output

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, r11
    xor rdx, rdx
    xor r10, r10
    syscall
    test rax, rax
    js apply_command_failed

    mov r14, rax
    mov rax, SYS_DUP2
    mov rdi, r14
    xor rsi, rsi
    syscall
    mov rbx, rax
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    test rbx, rbx
    js apply_command_failed

apply_command_output:
    lea r10, [rel output_redir_table]
    mov r11, [r10 + rdi * 8]
    test r11, r11
    jz apply_command_done

    lea r10, [rel output_append_table]
    mov r12, [r10 + rdi * 8]

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, r11
    mov rdx, O_WRONLY | O_CREAT
    test r12, r12
    jnz apply_command_open_output
    or rdx, O_TRUNC

apply_command_open_output:
    xor r10, r10
    syscall
    test rax, rax
    js apply_command_failed

    mov r14, rax
    test r12, r12
    jz apply_command_dup_output
    mov rax, SYS_LSEEK
    mov rdi, r14
    xor rsi, rsi
    mov rdx, SEEK_END
    syscall
    test rax, rax
    js apply_command_close_failed

apply_command_dup_output:
    mov rax, SYS_DUP2
    mov rdi, r14
    mov rsi, 1
    syscall
    mov rbx, rax
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    test rbx, rbx
    js apply_command_failed

apply_command_done:
    xor eax, eax
    ret

apply_command_close_failed:
    mov rbx, rax
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    test rbx, rbx
    js apply_command_failed

apply_command_failed:
    mov eax, -1
    ret

save_and_apply_foreground_redirections:
    mov qword [rel saved_stdin_fd], -1
    mov qword [rel saved_stdout_fd], -1
    push rdi
    mov rax, SYS_DUP
    xor rdi, rdi
    syscall
    mov [rel saved_stdin_fd], rax

    mov rax, SYS_DUP
    mov rdi, 1
    syscall
    mov [rel saved_stdout_fd], rax

    pop rdi
    call apply_foreground_redirections
    ret

restore_foreground_redirections:
    mov rax, [rel saved_stdin_fd]
    cmp rax, -1
    je restore_stdout_only
    mov r14, rax
    mov rax, SYS_DUP2
    mov rdi, r14
    xor rsi, rsi
    syscall
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    mov qword [rel saved_stdin_fd], -1

restore_stdout_only:
    mov rax, [rel saved_stdout_fd]
    cmp rax, -1
    je restore_redir_done
    mov r14, rax
    mov rax, SYS_DUP2
    mov rdi, r14
    mov rsi, 1
    syscall
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    mov qword [rel saved_stdout_fd], -1

restore_redir_done:
    ret

enumerate_completion_candidates:
    xor r13, r13

enumerate_builtin_loop:
    cmp r13, BUILTIN_CMD_COUNT
    jae enumerate_open_dir
    lea rbx, [rel builtin_cmd_table]
    mov rsi, [rbx + r13 * 8]
    call completion_process_candidate
    inc r13
    jmp enumerate_builtin_loop

enumerate_open_dir:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel root_dir_path]
    mov rdx, O_DIRECTORY
    xor r10, r10
    syscall
    test rax, rax
    js enumerate_done

    mov r14, rax

enumerate_read_loop:
    mov rax, SYS_GETDENTS64
    mov rdi, r14
    lea rsi, [rel io_buffer]
    mov rdx, IO_BUF_SIZE
    syscall
    test rax, rax
    jle enumerate_close_dir

    mov r10, rax
    lea rbx, [rel io_buffer]

enumerate_emit_entry:
    test r10, r10
    jle enumerate_read_loop

    mov r15, rbx
    lea rsi, [rbx + 19]
    call completion_process_candidate

    movzx rcx, word [r15 + 16]
    mov rbx, r15
    add rbx, rcx
    sub r10, rcx
    jmp enumerate_emit_entry

enumerate_close_dir:
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall

enumerate_done:
    ret

completion_process_candidate:
    mov al, [rsi]
    test al, al
    jz completion_candidate_done
    cmp al, '.'
    jne completion_match_prefix
    cmp byte [rsi + 1], 0
    je completion_candidate_done
    cmp byte [rsi + 1], '.'
    jne completion_match_prefix
    cmp byte [rsi + 2], 0
    je completion_candidate_done

completion_match_prefix:
    mov r11, rsi
    lea rbx, [rel line_buffer]
    add rbx, [rel completion_token_start]
    mov rcx, [rel completion_token_len]

completion_prefix_loop:
    test rcx, rcx
    jz completion_prefix_ok
    mov al, [rsi]
    test al, al
    jz completion_candidate_done
    cmp al, [rbx]
    jne completion_candidate_done
    inc rsi
    inc rbx
    dec rcx
    jmp completion_prefix_loop

completion_prefix_ok:
    mov rax, [rel completion_mode]
    cmp rax, COMPLETION_MODE_PRINT
    je completion_print_candidate

    mov rax, [rel completion_match_count]
    test rax, rax
    jne completion_update_common

    lea rdi, [rel completion_match]
    xor rcx, rcx

completion_store_first:
    mov al, [r11 + rcx]
    mov [rdi + rcx], al
    test al, al
    je completion_store_done
    inc rcx
    cmp rcx, CMD_BUF_SIZE - 1
    jb completion_store_first
    mov byte [rdi + rcx], 0

completion_store_done:
    mov [rel completion_common_len], rcx
    mov qword [rel completion_match_count], 1
    ret

completion_update_common:
    lea rdi, [rel completion_match]
    mov r8, [rel completion_common_len]
    xor rcx, rcx

completion_common_loop:
    cmp rcx, r8
    jae completion_common_done
    mov al, [rdi + rcx]
    mov dl, [r11 + rcx]
    cmp al, dl
    jne completion_common_done
    test al, al
    je completion_common_done
    inc rcx
    jmp completion_common_loop

completion_common_done:
    mov [rel completion_common_len], rcx
    inc qword [rel completion_match_count]
    ret

completion_print_candidate:
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    jmp write_stdout

completion_candidate_done:
    ret

append_completion_suffix:
    mov r8, [rel completion_token_len]
    mov r9, [rel completion_common_len]
    cmp r9, r8
    jbe append_completion_done
    lea rbx, [rel line_buffer]
    lea rsi, [rel completion_match]
    add rsi, r8
    mov r10, r8

append_completion_copy_loop:
    cmp r10, r9
    jae append_completion_emit
    cmp r12, CMD_BUF_SIZE - 1
    jae append_completion_emit
    lea rdi, [rel completion_match]
    mov al, [rdi + r10]
    mov [rbx + r12], al
    inc r12
    inc r10
    jmp append_completion_copy_loop

append_completion_emit:
    mov byte [rbx + r12], 0
    sub r10, r8
    test r10, r10
    jz append_completion_done
    call set_input_color
    lea rsi, [rel completion_match]
    add rsi, r8
    mov rdx, r10
    call write_stdout

append_completion_done:
    ret

print_completion_matches:
    call set_output_color
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    mov qword [rel completion_mode], COMPLETION_MODE_PRINT
    call enumerate_completion_candidates
    call write_prompt
    lea rsi, [rel line_buffer]
    call write_cstring_stdout
    ret

history_prev:
    mov rax, [rel history_count]
    test rax, rax
    jz history_nav_done

    mov rcx, [rel history_browse]
    cmp rcx, -1
    jne history_prev_has_browse

    dec rax
    mov [rel history_browse], rax
    mov rdi, rax
    call history_load_entry
    ret

history_prev_has_browse:
    test rcx, rcx
    jle history_prev_reload
    dec rcx
    mov [rel history_browse], rcx

history_prev_reload:
    mov rdi, [rel history_browse]
    call history_load_entry

history_nav_done:
    ret

history_next:
    mov rcx, [rel history_browse]
    cmp rcx, -1
    je history_nav_done

    mov rax, [rel history_count]
    dec rax
    cmp rcx, rax
    jb history_next_advance

    mov qword [rel history_browse], -1
    call history_load_blank
    ret

history_next_advance:
    inc rcx
    mov [rel history_browse], rcx
    mov rdi, rcx
    call history_load_entry
    ret

history_load_blank:
    call clear_current_line
    mov byte [rel line_buffer], 0
    xor r12, r12
    ret

history_load_entry:
    call clear_current_line
    mov rax, rdi
    imul rax, CMD_BUF_SIZE
    lea rsi, [rel history_lines]
    add rsi, rax
    lea rdi, [rel line_buffer]
    xor rcx, rcx

history_copy_loop:
    mov al, [rsi + rcx]
    mov [rdi + rcx], al
    test al, al
    je history_copy_done
    inc rcx
    cmp rcx, CMD_BUF_SIZE - 1
    jb history_copy_loop
    mov byte [rdi + rcx], 0

history_copy_done:
    mov r12, rcx
    lea rsi, [rel line_buffer]
    call write_cstring_stdout
    ret

store_history_line:
    test r12, r12
    jz store_history_done

    mov rax, [rel history_count]
    cmp rax, HISTORY_SIZE
    jb store_history_has_space

    mov rcx, 1

store_history_shift_entries:
    cmp rcx, HISTORY_SIZE
    jae store_history_shift_done
    mov r8, rcx
    dec r8
    imul r8, CMD_BUF_SIZE
    mov r9, rcx
    imul r9, CMD_BUF_SIZE
    lea rsi, [rel history_lines]
    add rsi, r9
    lea rdi, [rel history_lines]
    add rdi, r8
    xor r10, r10

store_history_shift_copy:
    mov dl, [rsi + r10]
    mov [rdi + r10], dl
    inc r10
    cmp r10, CMD_BUF_SIZE
    jb store_history_shift_copy
    inc rcx
    jmp store_history_shift_entries

store_history_shift_done:
    mov rax, HISTORY_SIZE - 1
    jmp store_history_write

store_history_has_space:
    mov rbx, rax
    inc rax
    mov [rel history_count], rax
    mov rax, rbx

store_history_write:
    imul rax, CMD_BUF_SIZE
    lea rdi, [rel history_lines]
    add rdi, rax
    lea rsi, [rel line_buffer]
    xor rcx, rcx

store_history_copy:
    mov dl, [rsi + rcx]
    mov [rdi + rcx], dl
    inc rcx
    cmp rcx, CMD_BUF_SIZE
    jae store_history_done
    test dl, dl
    jne store_history_copy

store_history_done:
    ret

exit_process:
    mov rax, SYS_EXIT
    syscall
exit_spin:
    jmp exit_spin

run_foreground_command:
    xor rdi, rdi
    call save_and_apply_foreground_redirections
    test eax, eax
    jne run_foreground_redir_failed
    call command_prefers_external
    test eax, eax
    jz run_foreground_dispatch
    call spawn_external_foreground
    jmp run_foreground_restore
run_foreground_dispatch:
    call dispatch_command
    cmp eax, 127
    jne run_foreground_restore
    call spawn_external_foreground
run_foreground_restore:
    mov r14d, eax
    call restore_foreground_redirections
    mov eax, r14d
run_foreground_return:
    ret

run_foreground_redir_failed:
    lea rsi, [rel redirection_failed_msg]
    mov rdx, redirection_failed_msg_end - redirection_failed_msg
    call write_stdout
    mov eax, 1
    ret

run_child_command:
    call command_prefers_external
    test eax, eax
    jz run_child_dispatch
    call exec_external_current
    jmp run_child_return
run_child_dispatch:
    call dispatch_command
    cmp eax, 127
    jne run_child_return
    call exec_external_current
run_child_return:
    ret

spawn_external_foreground:
    mov rax, SYS_FORK
    syscall
    test rax, rax
    js spawn_external_fork_failed
    jz spawn_external_child

    mov rdi, rax
    call wait_for_pid
    xor eax, eax
    ret

spawn_external_child:
    call exec_external_current
    mov edi, eax
    call exit_process

spawn_external_fork_failed:
    lea rsi, [rel command_fork_failed_msg]
    mov rdx, command_fork_failed_msg_end - command_fork_failed_msg
    call write_stdout
    mov eax, 1
    ret

exec_external_current:
    mov rax, SYS_EXECVE
    mov rdi, [r15]
    mov rsi, r15
    xor rdx, rdx
    syscall
    test rax, rax
    jns exec_external_failed

    call build_root_command_path
    mov rax, SYS_EXECVE
    lea rdi, [rel io_buffer]
    mov rsi, r15
    xor rdx, rdx
    syscall
    test rax, rax
    jns exec_external_failed

    call build_commands_command_path
    mov rax, SYS_EXECVE
    lea rdi, [rel io_buffer]
    mov rsi, r15
    xor rdx, rdx
    syscall
    test rax, rax
    jns exec_external_failed

    mov rax, SYS_EXECVE
    lea rdi, [rel coreutils_path]
    mov rsi, r15
    xor rdx, rdx
    syscall
    test rax, rax
    jns exec_external_failed

    mov rax, SYS_EXECVE
    lea rdi, [rel busybox_path]
    mov rsi, r15
    xor rdx, rdx
    syscall

exec_external_failed:

    lea rsi, [rel command_exec_failed_msg]
    mov rdx, command_exec_failed_msg_end - command_exec_failed_msg
    call write_stdout
    mov eax, 127
    ret

build_root_command_path:
    lea rdi, [rel io_buffer]
    mov byte [rdi], '/'
    inc rdi
    mov rsi, [r15]

build_root_command_copy:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz build_root_command_copy
    ret

build_commands_command_path:
    lea rdi, [rel io_buffer]
    lea rsi, [rel commands_prefix]

build_commands_prefix_copy:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz build_commands_prefix_copy

    dec rdi
    mov rsi, [r15]

build_commands_command_copy:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz build_commands_command_copy
    ret

command_prefers_external:
    mov rdi, [r15]
    lea rsi, [rel cat_cmd]
    call strcmp
    test eax, eax
    jz command_prefers_external_yes

    mov rdi, [r15]
    lea rsi, [rel ls_cmd]
    call strcmp
    test eax, eax
    jz command_prefers_external_yes

    mov rdi, [r15]
    lea rsi, [rel pwd_cmd]
    call strcmp
    test eax, eax
    jz command_prefers_external_no

    mov rdi, [r15]
    lea rsi, [rel echo_cmd]
    call strcmp
    test eax, eax
    jz command_prefers_external_yes

    xor eax, eax
    ret

command_prefers_external_yes:
    mov eax, 1
    ret

command_prefers_external_no:
    xor eax, eax
    ret

dispatch_command:
    mov rdi, [r15]
    lea rsi, [rel help_cmd]
    call strcmp
    test eax, eax
    jz cmd_help

    mov rdi, [r15]
    lea rsi, [rel cat_cmd]
    call strcmp
    test eax, eax
    jz cmd_cat

    mov rdi, [r15]
    lea rsi, [rel ls_cmd]
    call strcmp
    test eax, eax
    jz cmd_ls

    mov rdi, [r15]
    lea rsi, [rel cd_cmd]
    call strcmp
    test eax, eax
    jz cmd_cd

    mov rdi, [r15]
    lea rsi, [rel home_cmd]
    call strcmp
    test eax, eax
    jz cmd_home

    mov rdi, [r15]
    lea rsi, [rel pwd_cmd]
    call strcmp
    test eax, eax
    jz cmd_pwd

    mov rdi, [r15]
    lea rsi, [rel clear_cmd]
    call strcmp
    test eax, eax
    jz cmd_clear

    mov rdi, [r15]
    lea rsi, [rel write_cmd]
    call strcmp
    test eax, eax
    jz cmd_write

    mov rdi, [r15]
    lea rsi, [rel exec_cmd]
    call strcmp
    test eax, eax
    jz cmd_exec

    mov rdi, [r15]
    lea rsi, [rel ps_cmd]
    call strcmp
    test eax, eax
    jz cmd_ps

    mov rdi, [r15]
    lea rsi, [rel echo_cmd]
    call strcmp
    test eax, eax
    jz cmd_echo

    mov rdi, [r15]
    lea rsi, [rel mkdir_cmd]
    call strcmp
    test eax, eax
    jz cmd_mkdir

    mov rdi, [r15]
    lea rsi, [rel touch_cmd]
    call strcmp
    test eax, eax
    jz cmd_touch

    mov rdi, [r15]
    lea rsi, [rel rm_cmd]
    call strcmp
    test eax, eax
    jz cmd_rm

    mov rdi, [r15]
    lea rsi, [rel rmdir_cmd]
    call strcmp
    test eax, eax
    jz cmd_rmdir

    mov eax, 127
    ret

cmd_help:
    lea rsi, [rel help_msg]
    mov rdx, help_msg_end - help_msg
    call write_stdout
    xor eax, eax
    ret

cmd_cat:
    cmp r13, 2
    jb cmd_cat_stdin

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, [r15 + 8]
    xor rdx, rdx
    xor r10, r10
    syscall
    test rax, rax
    js cmd_cat_open_failed

    mov r14, rax

cmd_cat_read_loop:
    mov rax, SYS_READ
    mov rdi, r14
    lea rsi, [rel io_buffer]
    mov rdx, IO_BUF_SIZE
    syscall
    test rax, rax
    jle cmd_cat_close

    mov rdx, rax
    lea rsi, [rel io_buffer]
    call write_stdout
    jmp cmd_cat_read_loop

cmd_cat_close:
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    xor eax, eax
    ret

cmd_cat_stdin:
    xor r14, r14
    jmp cmd_cat_read_loop

cmd_cat_usage:
    lea rsi, [rel cat_usage_msg]
    mov rdx, cat_usage_msg_end - cat_usage_msg
    call write_stdout
    mov eax, 1
    ret

cmd_cat_open_failed:
    lea rsi, [rel cat_open_failed_msg]
    mov rdx, cat_open_failed_msg_end - cat_open_failed_msg
    call write_stdout
    mov eax, 1
    ret

cmd_ls:
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    cmp r13, 2
    jb cmd_ls_use_root
    mov rsi, [r15 + 8]
    jmp cmd_ls_open

cmd_ls_use_root:
    lea rsi, [rel root_dir_path]

cmd_ls_open:
    mov rdx, O_DIRECTORY
    xor r10, r10
    syscall
    test rax, rax
    js cmd_ls_open_failed

    mov r14, rax

cmd_ls_read_loop:
    mov rax, SYS_GETDENTS64
    mov rdi, r14
    lea rsi, [rel io_buffer]
    mov rdx, IO_BUF_SIZE
    syscall
    test rax, rax
    js cmd_ls_read_failed
    jz cmd_ls_close

    mov r10, rax
    lea rbx, [rel io_buffer]

cmd_ls_emit_entry:
    test r10, r10
    jle cmd_ls_read_loop

    lea rsi, [rbx + 19]
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

    movzx rcx, word [rbx + 16]
    add rbx, rcx
    sub r10, rcx
    jmp cmd_ls_emit_entry

cmd_ls_close:
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    xor eax, eax
    ret

cmd_ls_open_failed:
    lea rsi, [rel ls_open_failed_msg]
    mov rdx, ls_open_failed_msg_end - ls_open_failed_msg
    call write_stdout
    mov eax, 1
    ret

cmd_ls_read_failed:
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    lea rsi, [rel ls_read_failed_msg]
    mov rdx, ls_read_failed_msg_end - ls_read_failed_msg
    call write_stdout
    mov eax, 1
    ret

cmd_cd:
    cmp r13, 2
    jb cmd_cd_home
    mov rbx, [r15 + 8]
    mov rdi, rbx
    lea rsi, [rel tilde_path]
    call strcmp
    test eax, eax
    jz cmd_cd_home
    cmp byte [rbx], '~'
    jne cmd_cd_regular
    cmp byte [rbx + 1], '/'
    je cmd_cd_expand_home

cmd_cd_regular:
    mov rdi, rbx
    jmp cmd_cd_do

cmd_cd_home:
    call chdir_home
    test eax, eax
    js cmd_cd_failed
    xor eax, eax
    ret

cmd_home:
    call chdir_home
    test eax, eax
    js cmd_cd_failed
    xor eax, eax
    ret

cmd_cd_expand_home:
    lea rdi, [rel io_buffer]
    lea rsi, [rel default_home_path]

cmd_cd_expand_copy_home:
    mov al, [rsi]
    test al, al
    jz cmd_cd_expand_append_tail
    mov [rdi], al
    inc rdi
    inc rsi
    jmp cmd_cd_expand_copy_home

cmd_cd_expand_append_tail:
    mov rsi, rbx
    inc rsi

cmd_cd_expand_copy_tail:
    mov al, [rsi]
    mov [rdi], al
    inc rdi
    inc rsi
    test al, al
    jnz cmd_cd_expand_copy_tail
    lea rdi, [rel io_buffer]
    jmp cmd_cd_do

chdir_home:
    lea rdi, [rel default_home_path]
    mov rax, SYS_CHDIR
    syscall
    test rax, rax
    jns chdir_home_ok
    lea rdi, [rel main_home_path]
    mov rax, SYS_CHDIR
    syscall
    test rax, rax
    jns chdir_home_ok
    lea rdi, [rel slash_path]
    mov rax, SYS_CHDIR
    syscall
    test rax, rax
    jns chdir_home_ok
    mov eax, -1
    ret

chdir_home_ok:
    xor eax, eax
    ret

cmd_cd_do:
    mov rax, SYS_CHDIR
    syscall
    test rax, rax
    js cmd_cd_failed
    xor eax, eax
    ret

cmd_cd_failed:
    lea rsi, [rel cd_failed_msg]
    mov rdx, cd_failed_msg_end - cd_failed_msg
    call write_stdout
    mov eax, 1
    ret

cmd_pwd:
    mov rax, SYS_GETCWD
    lea rdi, [rel io_buffer]
    mov rsi, IO_BUF_SIZE
    syscall
    test rax, rax
    js cmd_pwd_failed
    lea rsi, [rel io_buffer]
    call write_cstring_stdout
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    xor eax, eax
    ret

cmd_pwd_failed:
    lea rsi, [rel pwd_failed_msg]
    mov rdx, pwd_failed_msg_end - pwd_failed_msg
    call write_stdout
    mov eax, 1
    ret

cmd_clear:
    call clear_vga_console
    xor eax, eax
    ret

cmd_write:
    cmp r13, 3
    jb cmd_write_usage

    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    mov rsi, [r15 + 8]
    mov rdx, O_WRONLY | O_CREAT | O_TRUNC
    xor r10, r10
    syscall
    test rax, rax
    js cmd_write_failed

    mov r14, rax
    mov r11, 2

cmd_write_loop:
    mov rax, SYS_WRITE
    mov rdi, r14
    mov rsi, [r15 + r11 * 8]
    call cstring_len
    syscall
    test rax, rax
    js cmd_write_close_failed

    inc r11
    cmp r11, r13
    jae cmd_write_close_ok

    mov rax, SYS_WRITE
    mov rdi, r14
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    syscall
    test rax, rax
    js cmd_write_close_failed
    jmp cmd_write_loop

cmd_write_close_ok:
    mov rax, SYS_WRITE
    mov rdi, r14
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    syscall
    test rax, rax
    js cmd_write_close_failed

    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    xor eax, eax
    ret

cmd_write_close_failed:
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall

cmd_write_failed:
    lea rsi, [rel write_failed_msg]
    mov rdx, write_failed_msg_end - write_failed_msg
    call write_stdout
    mov eax, 1
    ret

cmd_write_usage:
    lea rsi, [rel write_usage_msg]
    mov rdx, write_usage_msg_end - write_usage_msg
    call write_stdout
    mov eax, 1
    ret

cmd_exec:
    cmp r13, 2
    jb cmd_exec_usage

    lea r15, [r15 + 8]
    call exec_external_current
    mov eax, 1
    ret

cmd_exec_usage:
    lea rsi, [rel exec_usage_msg]
    mov rdx, exec_usage_msg_end - exec_usage_msg
    call write_stdout
    mov eax, 1
    ret

cmd_ps:
    lea rsi, [rel ps_header_msg]
    mov rdx, ps_header_msg_end - ps_header_msg
    call write_stdout
    xor r12, r12

cmd_ps_loop:
    cmp r12, MAX_PS_PROCESSES
    jae cmd_ps_done

    mov rax, AOS_SYS_PROCESS_INFO
    mov rdi, r12
    lea rsi, [rel ps_info_buffer]
    syscall
    test rax, rax
    js cmd_ps_next

    cmp byte [rel ps_info_buffer + PS_VALID_OFF], 0
    je cmd_ps_next

    mov edi, [rel ps_info_buffer + PS_PID_OFF]
    call write_u64_decimal
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout

    mov edi, [rel ps_info_buffer + PS_PPID_OFF]
    call write_u64_decimal
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout

    mov edi, [rel ps_info_buffer + PS_UID_OFF]
    call write_u64_decimal
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout

    mov edi, [rel ps_info_buffer + PS_EUID_OFF]
    call write_u64_decimal
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout

    mov al, [rel ps_info_buffer + PS_STATUS_OFF]
    call write_process_status
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout

    lea rsi, [rel ps_info_buffer + PS_USERNAME_OFF]
    call write_cstring_or_dash
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout

    lea rsi, [rel ps_info_buffer + PS_COMMAND_OFF]
    call write_cstring_or_dash
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout

cmd_ps_next:
    inc r12
    jmp cmd_ps_loop

cmd_ps_done:
    xor eax, eax
    ret

cmd_echo:
    mov r14, 1
    cmp r13, 1
    jbe cmd_echo_newline

cmd_echo_loop:
    mov rsi, [r15 + r14 * 8]
    call write_cstring_stdout
    inc r14
    cmp r14, r13
    jae cmd_echo_newline
    lea rsi, [rel space_msg]
    mov rdx, space_msg_end - space_msg
    call write_stdout
    jmp cmd_echo_loop

cmd_echo_newline:
    lea rsi, [rel newline_msg]
    mov rdx, newline_msg_end - newline_msg
    call write_stdout
    xor eax, eax
    ret

cmd_mkdir:
    cmp r13, 2
    jb cmd_path_usage_failed
    mov rdi, [r15 + 8]
    lea rsi, [rel io_buffer]
    call expand_home_path
    mov rax, SYS_MKDIRAT
    mov rdi, AT_FDCWD
    lea rsi, [rel io_buffer]
    mov rdx, 0755o
    xor r10, r10
    syscall
    test rax, rax
    js cmd_path_failed
    xor eax, eax
    ret

cmd_touch:
    cmp r13, 2
    jb cmd_path_usage_failed
    mov rdi, [r15 + 8]
    lea rsi, [rel io_buffer]
    call expand_home_path
    mov rax, SYS_OPENAT
    mov rdi, AT_FDCWD
    lea rsi, [rel io_buffer]
    mov rdx, O_WRONLY | O_CREAT
    xor r10, r10
    syscall
    test rax, rax
    js cmd_path_failed
    mov r14, rax
    mov rax, SYS_CLOSE
    mov rdi, r14
    syscall
    xor eax, eax
    ret

cmd_rm:
    cmp r13, 2
    jb cmd_path_usage_failed
    mov rdi, [r15 + 8]
    lea rsi, [rel io_buffer]
    call expand_home_path
    mov rax, SYS_UNLINKAT
    mov rdi, AT_FDCWD
    lea rsi, [rel io_buffer]
    xor rdx, rdx
    syscall
    test rax, rax
    js cmd_path_failed
    xor eax, eax
    ret

cmd_rmdir:
    cmp r13, 2
    jb cmd_path_usage_failed
    mov rdi, [r15 + 8]
    lea rsi, [rel io_buffer]
    call expand_home_path
    mov rax, SYS_UNLINKAT
    mov rdi, AT_FDCWD
    lea rsi, [rel io_buffer]
    mov rdx, 0x200
    syscall
    test rax, rax
    js cmd_path_failed
    xor eax, eax
    ret

cmd_path_usage_failed:
    lea rsi, [rel path_usage_msg]
    mov rdx, path_usage_msg_end - path_usage_msg
    call write_stdout
    mov eax, 1
    ret

cmd_path_failed:
    lea rsi, [rel path_failed_msg]
    mov rdx, path_failed_msg_end - path_failed_msg
    call write_stdout
    mov eax, 1
    ret

expand_home_path:
    cmp byte [rdi], '~'
    jne expand_home_copy_direct
    cmp byte [rdi + 1], 0
    je expand_home_copy_home_only
    cmp byte [rdi + 1], '/'
    jne expand_home_copy_direct

    lea r8, [rel main_home_path]
expand_home_copy_home_prefix:
    mov al, [r8]
    mov [rsi], al
    inc rsi
    inc r8
    test al, al
    jnz expand_home_copy_home_prefix
    dec rsi
    inc rdi
    jmp expand_home_copy_tail

expand_home_copy_home_only:
    lea r8, [rel main_home_path]
expand_home_copy_home_only_loop:
    mov al, [r8]
    mov [rsi], al
    inc rsi
    inc r8
    test al, al
    jnz expand_home_copy_home_only_loop
    ret

expand_home_copy_direct:
expand_home_copy_tail:
    mov al, [rdi]
    mov [rsi], al
    inc rsi
    inc rdi
    test al, al
    jnz expand_home_copy_tail
    ret

expand_argv_home:
    xor r10, r10

expand_argv_home_loop:
    cmp r10, r13
    jae expand_argv_home_done
    mov r9, [r15 + r10 * 8]
    test r9, r9
    jz expand_argv_home_next
    cmp byte [r9], '~'
    jne expand_argv_home_next
    mov al, [r9 + 1]
    test al, al
    jz expand_argv_home_copy
    cmp al, '/'
    jne expand_argv_home_next

expand_argv_home_copy:
    mov r11, [rel home_expansion_next]
    mov [r15 + r10 * 8], r11
    lea r8, [rel main_home_path]

expand_argv_home_copy_prefix:
    mov al, [r8]
    test al, al
    jz expand_argv_home_copy_tail
    mov [r11], al
    inc r11
    inc r8
    jmp expand_argv_home_copy_prefix

expand_argv_home_copy_tail:
    mov al, [r9 + 1]
    mov [r11], al
    inc r11
    inc r9
    test al, al
    jnz expand_argv_home_copy_tail
    mov [rel home_expansion_next], r11

expand_argv_home_next:
    inc r10
    jmp expand_argv_home_loop

expand_argv_home_done:
    ret

clear_vga_console:
    lea rsi, [rel console_clear_seq]
    mov rdx, console_clear_seq_end - console_clear_seq
    jmp write_stdout

set_output_color:
    lea rsi, [rel output_color_seq]
    mov rdx, output_color_seq_end - output_color_seq
    jmp write_stdout

set_input_color:
    lea rsi, [rel input_color_seq]
    mov rdx, input_color_seq_end - input_color_seq
    jmp write_stdout

set_prompt_color:
    lea rsi, [rel prompt_color_seq]
    mov rdx, prompt_color_seq_end - prompt_color_seq
    jmp write_stdout

write_prompt:
    call set_prompt_color
    lea rsi, [rel prompt_msg]
    mov rdx, prompt_msg_end - prompt_msg
    call write_stdout
    jmp set_input_color

write_stdout:
    mov rax, SYS_WRITE
    mov rdi, 1
    syscall
    ret

write_cstring_stdout:
    xor rdx, rdx

write_cstring_count_loop:
    cmp byte [rsi + rdx], 0
    je write_cstring_emit
    inc rdx
    jmp write_cstring_count_loop

write_cstring_emit:
    jmp write_stdout

write_cstring_or_dash:
    cmp byte [rsi], 0
    jne write_cstring_stdout
    lea rsi, [rel dash_msg]
    mov rdx, dash_msg_end - dash_msg
    jmp write_stdout

write_u64_decimal:
    push rbx
    lea rbx, [rel decimal_buffer + 20]
    mov byte [rbx], 0
    mov rax, rdi
    test rax, rax
    jnz write_u64_decimal_convert
    dec rbx
    mov byte [rbx], '0'
    jmp write_u64_decimal_emit

write_u64_decimal_convert:
    mov rcx, 10

write_u64_decimal_loop:
    xor rdx, rdx
    div rcx
    dec rbx
    add dl, '0'
    mov [rbx], dl
    test rax, rax
    jnz write_u64_decimal_loop

write_u64_decimal_emit:
    mov rsi, rbx
    call write_cstring_stdout
    pop rbx
    ret

write_process_status:
    cmp al, 1
    je write_status_ready
    cmp al, 2
    je write_status_running
    cmp al, 3
    je write_status_waiting
    cmp al, 4
    je write_status_zombie
    lea rsi, [rel status_unknown_msg]
    jmp write_cstring_stdout

write_status_ready:
    lea rsi, [rel status_ready_msg]
    jmp write_cstring_stdout

write_status_running:
    lea rsi, [rel status_running_msg]
    jmp write_cstring_stdout

write_status_waiting:
    lea rsi, [rel status_waiting_msg]
    jmp write_cstring_stdout

write_status_zombie:
    lea rsi, [rel status_zombie_msg]
    jmp write_cstring_stdout

cstring_len:
    xor rdx, rdx

cstring_len_loop:
    cmp byte [rsi + rdx], 0
    je cstring_len_done
    inc rdx
    jmp cstring_len_loop

cstring_len_done:
    ret

strcmp:
strcmp_compare_loop:
    mov al, [rdi]
    mov dl, [rsi]
    cmp al, dl
    jne strcmp_different
    test al, al
    je strcmp_equal
    inc rdi
    inc rsi
    jmp strcmp_compare_loop

strcmp_different:
    movzx eax, al
    movzx edx, dl
    sub eax, edx
    ret

strcmp_equal:
    xor eax, eax
    ret

section .bss
line_buffer:
    resb CMD_BUF_SIZE
io_buffer:
    resb IO_BUF_SIZE
argv_table:
    resq MAX_PIPE_CMDS * (MAX_ARGS + 1)
argc_table:
    resq MAX_PIPE_CMDS
command_starts:
    resq MAX_PIPE_CMDS
child_pids:
    resq MAX_PIPE_CMDS
command_count:
    resq 1
current_read_fd:
    resq 1
input_redir_table:
    resq MAX_PIPE_CMDS
output_redir_table:
    resq MAX_PIPE_CMDS
output_append_table:
    resq MAX_PIPE_CMDS
saved_stdin_fd:
    resq 1
saved_stdout_fd:
    resq 1
completion_token_start:
    resq 1
completion_token_len:
    resq 1
completion_match_count:
    resq 1
completion_common_len:
    resq 1
completion_mode:
    resq 1
completion_match:
    resb CMD_BUF_SIZE
history_lines:
    resb HISTORY_SIZE * CMD_BUF_SIZE
history_count:
    resq 1
history_browse:
    resq 1
argv_primary:
    resq MAX_ARGS + 1
argv_secondary:
    resq MAX_ARGS + 1
pipe_fds:
    resd 2
left_pid:
    resq 1
right_pid:
    resq 1
left_argc:
    resq 1
right_argc:
    resq 1
home_expansion_next:
    resq 1
home_expansion_buffer:
    resb HOME_EXPANSION_SIZE
ps_info_buffer:
    resb 392
decimal_buffer:
    resb 21

section .rodata
welcome_msg:
    db "--- AOS Interactive Shell v0.1 ---", 10
    db "Type help for commands.", 10
welcome_msg_end:

prompt_msg:
    db "AOS# "
prompt_msg_end:

less_token:
    db "<", 0

greater_token:
    db ">", 0

double_greater_token:
    db ">>", 0

console_clear_seq:
    db 27, 'L'
console_clear_seq_end:

prompt_color_seq:
    db 27, 'C', VGA_COLOR_PROMPT
prompt_color_seq_end:

input_color_seq:
    db 27, 'C', VGA_COLOR_INPUT
input_color_seq_end:

output_color_seq:
    db 27, 'C', VGA_COLOR_OUTPUT
output_color_seq_end:

backspace_seq:
    db 8, ' ', 8
backspace_seq_end:

help_cmd:
    db "help", 0
cat_cmd:
    db "cat", 0
ls_cmd:
    db "ls", 0
cd_cmd:
    db "cd", 0
pwd_cmd:
    db "pwd", 0
clear_cmd:
    db "clear", 0
home_cmd:
    db "home", 0
write_cmd:
    db "write", 0
exec_cmd:
    db "exec", 0
ps_cmd:
    db "ps", 0
echo_cmd:
    db "echo", 0
mkdir_cmd:
    db "mkdir", 0
touch_cmd:
    db "touch", 0
rm_cmd:
    db "rm", 0
rmdir_cmd:
    db "rmdir", 0
builtin_cmd_table:
    dq help_cmd
    dq cat_cmd
    dq ls_cmd
    dq cd_cmd
    dq home_cmd
    dq pwd_cmd
    dq clear_cmd
    dq write_cmd
    dq exec_cmd
    dq ps_cmd
    dq echo_cmd
    dq mkdir_cmd
    dq touch_cmd
    dq rm_cmd
    dq rmdir_cmd
root_dir_path:
    db ".", 0
slash_path:
    db "/", 0
tilde_path:
    db "~", 0
default_home_path:
    db "/root", 0
main_home_path:
    db "/root", 0
busybox_path:
    db "busybox", 0
coreutils_path:
    db "coreutils", 0
commands_prefix:
    db "/commands/", 0

help_msg:
    db "AOS shell help", 10
    db "Builtins: help cd home pwd clear write exec ps mkdir touch rm rmdir", 10
    db "PartiotionMANAGAER: run partitions or PartiotionMANAGER", 10
    db "BusyBox applets: sh ash test [ env printf true false head tail uname", 10
    db "BusyBox fallback: commands not found as GNU or initrd programs retry through BusyBox", 10
    db "GNU coreutils: ls cat echo head tail true false", 10
    db "GNU priority: ls cat echo run GNU first, then BusyBox if missing", 10
    db "Per-command help: run ls --help, cat --help, echo --help, pwd --help", 10
    db "AOS tools: afetch lspci driver/drivers net ifconfig ip netstat netcache route neigh dhcp tcp httpget curl acur kshttpget tcpstress dlstress wget download https tlsprobe sockclose gethost wifi fw usb ping ping6 rdisc6 dns nslookup netrawtest mem uptime date uname whoami id ps sudo aossetup settings display gfxdemo inputtest mounts partitions nano aosnano touch rm mkdir rmdir shutdown restart reboot", 10
    db "Redirection: < > and >> are supported", 10
    db "Pipes: cmd1 | cmd2 | cmd3", 10
    db "Quotes: 'one two' and ", 34, "one two", 34, 10
    db "History: Up/Down arrows", 10
    db "Mounts: /tmp /fat32 /mnt/fat32 /ext4 /mnt/ext4", 10
help_msg_end:

cat_usage_msg:
    db "usage: cat [file]", 10
cat_usage_msg_end:

cat_open_failed_msg:
    db "cat: open failed", 10
cat_open_failed_msg_end:

ls_open_failed_msg:
    db "ls: open failed", 10
ls_open_failed_msg_end:

ls_read_failed_msg:
    db "ls: read failed", 10
ls_read_failed_msg_end:

cd_failed_msg:
    db "cd: failed", 10
cd_failed_msg_end:

pwd_failed_msg:
    db "pwd: failed", 10
pwd_failed_msg_end:

write_usage_msg:
    db "usage: write <file> <text...>", 10
write_usage_msg_end:

write_failed_msg:
    db "write: failed", 10
write_failed_msg_end:

exec_usage_msg:
    db "usage: exec <program>", 10
exec_usage_msg_end:

exec_failed_msg:
    db "exec: failed", 10
exec_failed_msg_end:

ps_header_msg:
    db "PID PPID UID EUID STATE USER COMMAND", 10
ps_header_msg_end:

dash_msg:
    db "-", 0
dash_msg_end:

status_ready_msg:
    db "ready", 0
status_running_msg:
    db "run", 0
status_waiting_msg:
    db "wait", 0
status_zombie_msg:
    db "zombie", 0
status_unknown_msg:
    db "unknown", 0

path_usage_msg:
    db "usage: mkdir/touch/rm/rmdir PATH", 10
path_usage_msg_end:

path_failed_msg:
    db "path command failed", 10
path_failed_msg_end:

pipeline_usage_msg:
    db "usage: cmd1 | cmd2 [| cmd3 ...]", 10
pipeline_usage_msg_end:

pipeline_pipe_failed_msg:
    db "pipeline: failed", 10
pipeline_pipe_failed_msg_end:

pipeline_fork_failed_msg:
    db "pipeline: fork failed", 10
pipeline_fork_failed_msg_end:

command_fork_failed_msg:
    db "command: fork failed", 10
command_fork_failed_msg_end:

command_exec_failed_msg:
    db "command: exec failed", 10
command_exec_failed_msg_end:

redirection_failed_msg:
    db "redirection: failed", 10
redirection_failed_msg_end:

space_msg:
    db " "
space_msg_end:

newline_msg:
    db 10
newline_msg_end:
