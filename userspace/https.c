/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <stdint.h>

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_CLOSE 3
#define SYS_SOCKET 41
#define SYS_CONNECT 42
#define SYS_EXIT 60
#define SYS_GETRANDOM 318
#define AOS_SYS_DNS_LOOKUP 534
#define AOS_SYS_SOCKET_BIND_NETDEV 538

#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6

#define TLS_RECORD_HANDSHAKE 22
#define TLS_RECORD_ALERT 21
#define TLS_HANDSHAKE_SERVER_HELLO 2

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint8_t sin_addr[4];
    uint8_t sin_zero[8];
} __attribute__((packed));

static char host_buf[128];
static char path_buf[256];
static char num_buf[21];
static uint8_t tx[768];
static uint8_t rx[2048];

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static void exit_code(int code) {
    syscall3(SYS_EXIT, code, 0, 0);
    for (;;) {}
}

static uint64_t cstrlen(const char* s) {
    uint64_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void write_cstr(const char* s) {
    syscall3(SYS_WRITE, 1, (long)s, (long)cstrlen(s));
}

static void write_u64(uint64_t v) {
    char* p = &num_buf[20];
    *p = 0;
    if (v == 0) {
        *--p = '0';
    } else {
        while (v) {
            *--p = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    write_cstr(p);
}

static void write_hex_nibble(uint8_t v) {
    v &= 0x0f;
    syscall3(SYS_WRITE, 1, (long)&"0123456789abcdef"[v], 1);
}

static void write_hex16(uint16_t v) {
    write_cstr("0x");
    write_hex_nibble((uint8_t)(v >> 12));
    write_hex_nibble((uint8_t)(v >> 8));
    write_hex_nibble((uint8_t)(v >> 4));
    write_hex_nibble((uint8_t)v);
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int starts_with(const char* s, const char* prefix) {
    uint64_t i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static int copy_bytes(char* dst, uint64_t cap, const char* src, uint64_t len) {
    if (len + 1 > cap) return 0;
    for (uint64_t i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = 0;
    return 1;
}

static int parse_target(const char* target, const char** host, const char** path) {
    const char* p = target;
    uint64_t host_len = 0;
    uint64_t path_len = 0;

    if (starts_with(p, "https://")) p += 8;
    else if (starts_with(p, "http://")) p += 7;

    while (p[host_len] && p[host_len] != '/') host_len++;
    if (host_len == 0 || !copy_bytes(host_buf, sizeof(host_buf), p, host_len)) return 0;

    if (p[host_len] == '/') {
        while (p[host_len + path_len]) path_len++;
        if (!copy_bytes(path_buf, sizeof(path_buf), p + host_len, path_len)) return 0;
    } else {
        path_buf[0] = '/';
        path_buf[1] = 0;
    }

    *host = host_buf;
    *path = path_buf;
    return 1;
}

static int parse_iface_index(const char* s, uint64_t* out) {
    if (!s || s[0] < '0' || s[0] > '7' || s[1] != 0) return 0;
    *out = (uint64_t)(s[0] - '0');
    return 1;
}

static int parse_ipv4(const char* s, uint8_t out[4]) {
    uint32_t part = 0;
    uint32_t digits = 0;
    uint32_t index = 0;

    while (*s) {
        if (*s >= '0' && *s <= '9') {
            part = part * 10 + (uint32_t)(*s - '0');
            if (part > 255) return 0;
            digits++;
        } else if (*s == '.') {
            if (digits == 0 || index >= 3) return 0;
            out[index++] = (uint8_t)part;
            part = 0;
            digits = 0;
        } else {
            return 0;
        }
        s++;
    }
    if (digits == 0 || index != 3) return 0;
    out[index] = (uint8_t)part;
    return 1;
}

static void put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put_be24(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 16);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)v;
}

static uint16_t get_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get_be24(const uint8_t* p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static void append_u8(uint64_t* n, uint8_t v) {
    tx[(*n)++] = v;
}

static void append_be16(uint64_t* n, uint16_t v) {
    put_be16(tx + *n, v);
    *n += 2;
}

static void append_bytes(uint64_t* n, const uint8_t* src, uint64_t len) {
    for (uint64_t i = 0; i < len; i++) tx[(*n)++] = src[i];
}

static const char* cipher_name(uint16_t cipher) {
    switch (cipher) {
        case 0xc02f: return "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";
        case 0xc02b: return "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
        case 0xc027: return "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256";
        case 0xc023: return "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256";
        case 0x009c: return "TLS_RSA_WITH_AES_128_GCM_SHA256";
        case 0x003c: return "TLS_RSA_WITH_AES_128_CBC_SHA256";
        default: return "unknown";
    }
}

static const char* tls_version_name(uint8_t maj, uint8_t min) {
    if (maj == 3 && min == 1) return "TLS 1.0";
    if (maj == 3 && min == 2) return "TLS 1.1";
    if (maj == 3 && min == 3) return "TLS 1.2";
    if (maj == 3 && min == 4) return "TLS 1.3";
    return "unknown";
}

static uint64_t build_client_hello(const char* host) {
    uint64_t n = 0;
    uint64_t hs_start;
    uint64_t body_start;
    uint64_t ext_len_pos;
    uint64_t ext_start;
    uint64_t host_len = cstrlen(host);
    uint8_t random[32];
    uint16_t suites[] = {
        0xc02f, 0xc02b, 0xc027, 0xc023, 0x009c, 0x003c, 0x00ff
    };

    for (uint64_t i = 0; i < sizeof(random); i++) random[i] = (uint8_t)(0xa0 + i);
    if (syscall3(SYS_GETRANDOM, (long)random, sizeof(random), 0) < 0) {
        random[0] ^= 0x55;
    }

    append_u8(&n, TLS_RECORD_HANDSHAKE);
    append_u8(&n, 0x03);
    append_u8(&n, 0x01);
    append_be16(&n, 0);

    hs_start = n;
    append_u8(&n, 0x01);
    put_be24(tx + n, 0);
    n += 3;
    body_start = n;

    append_u8(&n, 0x03);
    append_u8(&n, 0x03);
    append_bytes(&n, random, sizeof(random));
    append_u8(&n, 0);

    append_be16(&n, (uint16_t)(sizeof(suites)));
    for (uint64_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        append_be16(&n, suites[i]);
    }
    append_u8(&n, 1);
    append_u8(&n, 0);

    ext_len_pos = n;
    append_be16(&n, 0);
    ext_start = n;

    if (host_len > 0 && host_len < 256) {
        uint16_t server_name_list_len = (uint16_t)(host_len + 3);
        append_be16(&n, 0x0000);
        append_be16(&n, (uint16_t)(server_name_list_len + 2));
        append_be16(&n, server_name_list_len);
        append_u8(&n, 0);
        append_be16(&n, (uint16_t)host_len);
        append_bytes(&n, (const uint8_t*)host, host_len);
    }

    append_be16(&n, 0x000a);
    append_be16(&n, 8);
    append_be16(&n, 6);
    append_be16(&n, 0x001d);
    append_be16(&n, 0x0017);
    append_be16(&n, 0x0018);

    append_be16(&n, 0x000b);
    append_be16(&n, 2);
    append_u8(&n, 1);
    append_u8(&n, 0);

    append_be16(&n, 0x000d);
    append_be16(&n, 10);
    append_be16(&n, 8);
    append_be16(&n, 0x0401);
    append_be16(&n, 0x0403);
    append_be16(&n, 0x0501);
    append_be16(&n, 0x0503);

    put_be16(tx + ext_len_pos, (uint16_t)(n - ext_start));
    put_be24(tx + hs_start + 1, (uint32_t)(n - body_start));
    put_be16(tx + 3, (uint16_t)(n - hs_start));
    return n;
}

static int parse_server_hello(const uint8_t* buf, uint64_t len) {
    uint64_t p = 0;
    uint64_t body_end;
    uint8_t maj;
    uint8_t min;
    uint16_t cipher;
    uint8_t sid_len;

    if (len < 9) {
        write_cstr("https: TLS response too short\n");
        return 1;
    }
    if (buf[0] == TLS_RECORD_ALERT) {
        write_cstr("https: TLS alert level=");
        write_u64(buf[5]);
        write_cstr(" description=");
        write_u64(buf[6]);
        write_cstr("\n");
        return 1;
    }
    if (buf[0] != TLS_RECORD_HANDSHAKE) {
        write_cstr("https: unexpected TLS record type ");
        write_u64(buf[0]);
        write_cstr("\n");
        return 1;
    }
    if (buf[5] != TLS_HANDSHAKE_SERVER_HELLO) {
        write_cstr("https: first handshake was type ");
        write_u64(buf[5]);
        write_cstr(", expected ServerHello\n");
        return 1;
    }

    body_end = 9 + get_be24(buf + 6);
    if (body_end > len || body_end < 45) {
        write_cstr("https: incomplete ServerHello\n");
        return 1;
    }

    p = 9;
    maj = buf[p++];
    min = buf[p++];
    p += 32;
    sid_len = buf[p++];
    if (p + sid_len + 3 > body_end) {
        write_cstr("https: bad ServerHello session id\n");
        return 1;
    }
    p += sid_len;
    cipher = get_be16(buf + p);

    write_cstr("https: TLS ServerHello OK\n");
    write_cstr("version: ");
    write_cstr(tls_version_name(maj, min));
    write_cstr(" (");
    write_u64(maj);
    write_cstr(".");
    write_u64(min);
    write_cstr(")\n");
    write_cstr("cipher: ");
    write_hex16(cipher);
    write_cstr(" ");
    write_cstr(cipher_name(cipher));
    write_cstr("\n");
    write_cstr("status: TLS handshake reached ServerHello; encrypted HTTPS records come next\n");
    return 0;
}

static void usage(void) {
    write_cstr("usage: https [--iface N] HOST|https://HOST/PATH\n");
    write_cstr("does: TCP 443 + TLS ClientHello/ServerHello probe\n");
}

void aos_main(uint64_t argc, char** argv) {
    struct sockaddr_in addr;
    const char* host = 0;
    const char* path = 0;
    uint8_t parsed_ip[4];
    uint64_t iface_index = 0;
    uint64_t hello_len;
    int sock_fd;
    long rc;

    for (uint64_t i = 1; i < argc; i++) {
        if (streq(argv[i], "--help") || streq(argv[i], "-h")) {
            usage();
            exit_code(0);
        } else if (streq(argv[i], "--iface")) {
            if (i + 1 >= argc || !parse_iface_index(argv[i + 1], &iface_index)) {
                usage();
                exit_code(1);
            }
            i++;
        } else if (!host) {
            if (!parse_target(argv[i], &host, &path)) {
                usage();
                exit_code(1);
            }
        } else {
            usage();
            exit_code(1);
        }
    }

    if (!host) {
        usage();
        exit_code(1);
    }
    (void)path;

    sock_fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd < 0) {
        write_cstr("https: socket failed\n");
        exit_code(1);
    }
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, sock_fd, (long)iface_index, 0);
    if (rc < 0) {
        write_cstr("https: interface bind failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        exit_code(1);
    }

    for (uint64_t i = 0; i < sizeof(addr); i++) ((uint8_t*)&addr)[i] = 0;
    addr.sin_family = AF_INET;
    put_be16((uint8_t*)&addr.sin_port, 443);

    if (parse_ipv4(host, parsed_ip)) {
        for (uint64_t i = 0; i < sizeof(parsed_ip); i++) addr.sin_addr[i] = parsed_ip[i];
    } else {
        write_cstr("dns: query ");
        write_cstr(host);
        write_cstr("\n");
        rc = -1;
        for (uint64_t attempt = 0; attempt < 3 && rc < 0; attempt++) {
            rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)host, (long)addr.sin_addr, (long)iface_index);
        }
        if (rc < 0) {
            write_cstr("https: DNS lookup failed\n");
            syscall3(SYS_CLOSE, sock_fd, 0, 0);
            exit_code(1);
        }
    }

    write_cstr("https: connect ");
    write_cstr(host);
    write_cstr(":443\n");
    rc = syscall3(SYS_CONNECT, sock_fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        write_cstr("https: TCP connect failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        exit_code(1);
    }

    hello_len = build_client_hello(host);
    rc = syscall3(SYS_WRITE, sock_fd, (long)tx, hello_len);
    if (rc < 0) {
        write_cstr("https: ClientHello write failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        exit_code(1);
    }
    write_cstr("https: sent TLS ClientHello ");
    write_u64(hello_len);
    write_cstr(" bytes\n");

    rc = syscall3(SYS_READ, sock_fd, (long)rx, sizeof(rx));
    if (rc < 0) {
        write_cstr("https: TLS read failed\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        exit_code(1);
    }
    if (rc == 0) {
        write_cstr("https: server closed before ServerHello\n");
        syscall3(SYS_CLOSE, sock_fd, 0, 0);
        exit_code(1);
    }

    syscall3(SYS_CLOSE, sock_fd, 0, 0);
    exit_code(parse_server_hello(rx, (uint64_t)rc));
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
