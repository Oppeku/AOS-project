/* Host-linked glibc smoke test for AOS PT_INTERP compatibility. */

#include <stddef.h>
#include <unistd.h>

int main(void) {
    static const char message[] =
        "AOS Linux dynamic loader: glibc hello passed\n";
    ssize_t written = write(STDOUT_FILENO, message, sizeof(message) - 1);
    return written == (ssize_t)(sizeof(message) - 1) ? 0 : 1;
}
