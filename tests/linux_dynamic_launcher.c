/* Boots a Linux ABI fixture without depending on MUI input automation. */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_EXECVE 59
#define SYS_EXIT 60

static long syscall1(long number, long arg1) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(arg1)
                     : "rcx", "r11", "memory");
    return result;
}

static long syscall3(long number, long arg1, long arg2, long arg3) {
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
                     : "rcx", "r11", "memory");
    return result;
}

static uint64_t string_length(const char* text) {
    uint64_t length = 0;
    while (text[length] != '\0') length++;
    return length;
}

static void write_text(const char* text) {
    (void)syscall3(SYS_WRITE, 1, (long)text, (long)string_length(text));
}

__attribute__((used, noreturn)) static void run_fixture(void) {
    static const char path[] = "/linux-dynamic-hello";
    static const char* const argv[] = { path, 0 };
    static const char* const envp[] = { "PATH=/usr/bin:/bin", 0 };
    long result;

    write_text("AOS Linux compatibility test: launching glibc fixture\n");
    result = syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
    write_text("AOS Linux compatibility test: execve failed\n");
    syscall1(SYS_EXIT, result < 0 ? (long)-result : 1);
    for (;;) {}
}

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile("call run_fixture\n");
}
