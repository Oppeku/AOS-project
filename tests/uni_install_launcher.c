/* Boots Uni directly and validates an install/remove transaction. */

#include <stdint.h>

#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_FORK 57
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61
#define SYS_OPENAT 257

#define AT_FDCWD -100
#define O_RDONLY 0

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

static long syscall4(long number, long arg1, long arg2,
                     long arg3, long arg4) {
    long result;
    register long r10 __asm__("r10") = arg4;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(arg1), "S"(arg2),
                       "d"(arg3), "r"(r10)
                     : "rcx", "r11", "memory");
    return result;
}

static uint64_t string_length(const char* text) {
    uint64_t length = 0;
    while (text[length]) length++;
    return length;
}

static void write_text(const char* text) {
    uint64_t size = string_length(text);
    uint64_t done = 0;
    while (done < size) {
        long written = syscall3(SYS_WRITE, 1, (long)(text + done),
                                (long)(size - done));
        if (written <= 0) return;
        done += (uint64_t)written;
    }
}

static int path_exists(const char* path) {
    long fd = syscall4(SYS_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0);
    if (fd < 0) return 0;
    syscall1(SYS_CLOSE, fd);
    return 1;
}

static int run_program(const char* path, const char* const* argv) {
    static const char* const envp[] = { "PATH=/usr/bin:/bin:/", 0 };
    int status = 0;
    long pid = syscall1(SYS_FORK, 0);

    if (pid < 0) return -1;
    if (pid == 0) {
        syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
        syscall1(SYS_EXIT, 127);
        for (;;) {}
    }
    if (syscall4(SYS_WAIT4, pid, (long)&status, 0, 0) != pid) return -1;
    return (status >> 8) & 0xff;
}

__attribute__((used, noreturn)) static void run_test(void) {
    static const char* const install_argv[] = {
        "uni", "install", "/aos-hello.deb", 0
    };
    static const char* const app_argv[] = {
        "/usr/bin/aos-hello", "Uni executable test passed", 0
    };
    static const char* const remove_argv[] = {
        "uni", "remove", "aos-hello", 0
    };
    int failed = 0;

    write_text("AOS Uni transaction test: install\n");
    if (run_program("/uni", install_argv) != 0 ||
        !path_exists("/usr/bin/aos-hello") ||
        !path_exists("/var/lib/uni/installed.db") ||
        !path_exists("/var/lib/uni/packages/aos-hello.manifest")) {
        write_text("AOS Uni transaction test: install verification failed\n");
        failed = 1;
    }

    if (!failed) {
        write_text("AOS Uni transaction test: launch installed entry\n");
        if (run_program("/usr/bin/aos-hello", app_argv) != 0) {
            write_text("AOS Uni transaction test: executable failed\n");
            failed = 1;
        }
    }

    if (!failed) {
        write_text("AOS Uni transaction test: remove\n");
        if (run_program("/uni", remove_argv) != 0 ||
            path_exists("/usr/bin/aos-hello") ||
            path_exists("/var/lib/uni/packages/aos-hello.manifest")) {
            write_text("AOS Uni transaction test: removal verification failed\n");
            failed = 1;
        }
    }

    write_text(failed ? "AOS Uni transaction test: FAILED\n"
                      : "AOS Uni transaction test: PASSED\n");
    syscall1(SYS_EXIT, failed ? 1 : 0);
    for (;;) {}
}

__attribute__((naked, noreturn)) void _start(void) {
    __asm__ volatile("call run_test\n");
}
