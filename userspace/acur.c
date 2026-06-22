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
#define SYS_OPENAT 257
#define SYS_UNLINKAT 263
#define AOS_SYS_DNS_LOOKUP 534
#define AOS_SYS_SOCKET_BIND_NETDEV 538

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 64
#define O_TRUNC 512

#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_TCP 6

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint8_t sin_addr[4];
    uint8_t sin_zero[8];
} __attribute__((packed));

struct package_entry {
    const char* name;
    const char* host;
    const char* path;
    const char* outfile;
    const char* description;
    const char* sha256_hex;
};

struct sha256_ctx {
    uint32_t state[8];
    uint64_t total_len;
    uint8_t block[64];
    uint32_t block_len;
};

static char db[4096];
static char installed_db[4096];
static char req[512];
static char rx[512];
static char header[1024];
static char parsed_path[256];
static char num_buf[21];
static char live_outfile[160];
static char remove_path[160];
static char installed_path[160];
static char current_host_buf[128];
static char current_path_buf[256];
static char redirect_host_buf[128];
static char redirect_path_buf[256];
static char digest_hex[65];

static const char installed_db_path[] = "/tmp/acur-installed.txt";

static const uint32_t sha256_k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static long syscall3(long n, long a, long b, long c) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c)
                     : "rcx", "r11", "memory");
    return ret;
}

static long syscall4(long n, long a, long b, long c, long d) {
    long ret;
    register long r10 __asm__("r10") = d;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10)
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

static int streq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int copy_bytes(char* dst, uint64_t cap, const char* src, uint64_t len) {
    if (len + 1 > cap) return 0;
    for (uint64_t i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = 0;
    return 1;
}

static int copy_cstr(char* dst, uint64_t cap, const char* src) {
    uint64_t len = cstrlen(src);
    return copy_bytes(dst, cap, src, len);
}

static void write_cstr(const char* s) {
    syscall3(SYS_WRITE, 1, (long)s, (long)cstrlen(s));
}

static void write_buf_fd(int fd, const char* s, uint64_t n) {
    uint64_t off = 0;
    while (off < n) {
        long rc = syscall3(SYS_WRITE, fd, (long)(s + off), (long)(n - off));
        if (rc <= 0) break;
        off += (uint64_t)rc;
    }
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

static uint32_t load_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void store_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t rotr32(uint32_t v, uint32_t n) {
    return (v >> n) | (v << (32U - n));
}

static void sha256_transform(struct sha256_ctx* ctx, const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;

    for (uint32_t i = 0; i < 16; i++) {
        w[i] = load_be32(block + (i * 4U));
    }
    for (uint32_t i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (uint32_t i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(struct sha256_ctx* ctx) {
    ctx->state[0] = 0x6a09e667U;
    ctx->state[1] = 0xbb67ae85U;
    ctx->state[2] = 0x3c6ef372U;
    ctx->state[3] = 0xa54ff53aU;
    ctx->state[4] = 0x510e527fU;
    ctx->state[5] = 0x9b05688cU;
    ctx->state[6] = 0x1f83d9abU;
    ctx->state[7] = 0x5be0cd19U;
    ctx->total_len = 0;
    ctx->block_len = 0;
}

static void sha256_update(struct sha256_ctx* ctx, const uint8_t* data, uint64_t len) {
    ctx->total_len += len;
    for (uint64_t i = 0; i < len; i++) {
        ctx->block[ctx->block_len++] = data[i];
        if (ctx->block_len == 64) {
            sha256_transform(ctx, ctx->block);
            ctx->block_len = 0;
        }
    }
}

static void sha256_final(struct sha256_ctx* ctx, uint8_t out[32]) {
    uint64_t bit_len = ctx->total_len * 8U;

    ctx->block[ctx->block_len++] = 0x80U;
    if (ctx->block_len > 56) {
        while (ctx->block_len < 64) ctx->block[ctx->block_len++] = 0;
        sha256_transform(ctx, ctx->block);
        ctx->block_len = 0;
    }
    while (ctx->block_len < 56) ctx->block[ctx->block_len++] = 0;
    for (uint32_t i = 0; i < 8; i++) {
        ctx->block[56 + i] = (uint8_t)(bit_len >> (56U - (i * 8U)));
    }
    sha256_transform(ctx, ctx->block);

    for (uint32_t i = 0; i < 8; i++) {
        store_be32(out + (i * 4U), ctx->state[i]);
    }
}

static void bytes_to_hex(const uint8_t* bytes, uint64_t len, char* out) {
    const char* hex = "0123456789abcdef";
    for (uint64_t i = 0; i < len; i++) {
        out[i * 2U] = hex[bytes[i] >> 4];
        out[(i * 2U) + 1U] = hex[bytes[i] & 15U];
    }
    out[len * 2U] = 0;
}

static void put_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
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

static int parse_iface_index(const char* s, uint64_t* out) {
    if (!s || s[0] < '0' || s[0] > '7' || s[1] != 0) return 0;
    *out = (uint64_t)(s[0] - '0');
    return 1;
}

static uint64_t append(char* dst, uint64_t at, uint64_t cap, const char* src) {
    while (*src && at + 1 < cap) dst[at++] = *src++;
    dst[at] = 0;
    return at;
}

static int starts_with(const char* s, const char* prefix) {
    uint64_t i = 0;
    while (prefix[i]) {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

static int contains_text(const char* text, const char* needle) {
    if (!text || !needle || !needle[0]) return 1;
    for (uint64_t i = 0; text[i]; i++) {
        uint64_t j = 0;
        while (needle[j] && text[i + j] == needle[j]) j++;
        if (!needle[j]) return 1;
    }
    return 0;
}

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static int is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int sha256_hex_valid(const char* s) {
    uint64_t i = 0;
    if (!s) return 0;
    while (s[i]) {
        if (!is_hex_char(s[i])) return 0;
        i++;
    }
    return i == 64;
}

static int hex_digest_matches(const char* a, const char* b) {
    for (uint64_t i = 0; i < 64; i++) {
        char ca;
        char cb;
        if (!a[i] || !b[i]) return 0;
        ca = a[i];
        cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if (ca != cb) return 0;
    }
    return a[64] == 0 && b[64] == 0;
}

static const char* live_download_path(const char* requested) {
    const char* leaf = requested;
    uint64_t n = 0;

    if (!requested || !requested[0]) return "/tmp/aos-download";
    if (starts_with(requested, "/tmp/") || starts_with(requested, "tmp/")) return requested;

    for (uint64_t i = 0; requested[i]; i++) {
        if (requested[i] == '/') leaf = &requested[i + 1];
    }
    if (!leaf[0]) leaf = "aos-download";

    n = append(live_outfile, n, sizeof(live_outfile), "/tmp/");
    n = append(live_outfile, n, sizeof(live_outfile), leaf);
    if (n <= 5) return "/tmp/aos-download";
    return live_outfile;
}

static const char* live_package_path(const char* name) {
    uint64_t n = 0;
    n = append(remove_path, n, sizeof(remove_path), "/tmp/");
    n = append(remove_path, n, sizeof(remove_path), name);
    if (n <= 5) return "/tmp/aos-download";
    return remove_path;
}

static int load_installed_db(void) {
    int fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)installed_db_path, O_RDONLY, 0);
    long total = 0;
    long rc;

    installed_db[0] = 0;
    if (fd < 0) return 0;

    while (total + 1 < (long)sizeof(installed_db)) {
        rc = syscall3(SYS_READ,
                      fd,
                      (long)(installed_db + total),
                      (long)(sizeof(installed_db) - 1 - (uint64_t)total));
        if (rc < 0) {
            syscall3(SYS_CLOSE, fd, 0, 0);
            installed_db[0] = 0;
            return -1;
        }
        if (rc == 0) break;
        total += rc;
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    installed_db[total] = 0;
    return 0;
}

static int installed_line_name_matches(const char* line, const char* name) {
    uint64_t i = 0;
    while (name[i]) {
        if (line[i] != name[i]) return 0;
        i++;
    }
    return line[i] == ' ' || line[i] == '\t' || line[i] == 0 || line[i] == '\n' || line[i] == '\r';
}

static int rewrite_installed_db_without(const char* name) {
    char next_db[4096];
    uint64_t in = 0;
    uint64_t out = 0;
    int removed = 0;
    int fd;

    if (load_installed_db() != 0) return -1;

    while (installed_db[in]) {
        uint64_t start = in;
        uint64_t end;
        while (installed_db[in] && installed_db[in] != '\n' && installed_db[in] != '\r') in++;
        end = in;
        while (installed_db[in] == '\n' || installed_db[in] == '\r') in++;

        if (installed_line_name_matches(&installed_db[start], name)) {
            removed = 1;
            continue;
        }

        for (uint64_t i = start; i < end && out + 2 < sizeof(next_db); i++) {
            next_db[out++] = installed_db[i];
        }
        if (out + 2 < sizeof(next_db)) next_db[out++] = '\n';
    }
    next_db[out] = 0;

    fd = (int)syscall4(SYS_OPENAT,
                       AT_FDCWD,
                       (long)installed_db_path,
                       O_WRONLY | O_CREAT | O_TRUNC,
                       0);
    if (fd < 0) return -1;
    write_buf_fd(fd, next_db, out);
    syscall3(SYS_CLOSE, fd, 0, 0);
    return removed;
}

static int copy_installed_path_from_line(const char* line, const char* name, char* out, uint64_t cap) {
    uint64_t i = 0;
    uint64_t out_i = 0;

    if (!installed_line_name_matches(line, name)) return 0;
    while (name[i] && line[i]) i++;
    while (line[i] == ' ' || line[i] == '\t') i++;
    if (!line[i] || line[i] == '\n' || line[i] == '\r') return 0;

    while (line[i] && line[i] != '\n' && line[i] != '\r' && line[i] != ' ' && line[i] != '\t') {
        if (out_i + 1 >= cap) return 0;
        out[out_i++] = line[i++];
    }
    out[out_i] = 0;
    return out_i > 0;
}

static int find_installed_package_path(const char* name, char* out, uint64_t cap) {
    uint64_t in = 0;

    if (load_installed_db() != 0) return -1;
    while (installed_db[in]) {
        uint64_t start = in;
        while (installed_db[in] && installed_db[in] != '\n' && installed_db[in] != '\r') in++;
        if (copy_installed_path_from_line(&installed_db[start], name, out, cap)) return 0;
        while (installed_db[in] == '\n' || installed_db[in] == '\r') in++;
    }
    return -1;
}

static int record_installed_package(const char* name, const char* path) {
    int fd;
    uint64_t len = 0;

    if (rewrite_installed_db_without(name) < 0) return -1;
    if (load_installed_db() != 0) return -1;
    while (installed_db[len]) len++;

    fd = (int)syscall4(SYS_OPENAT,
                       AT_FDCWD,
                       (long)installed_db_path,
                       O_WRONLY | O_CREAT | O_TRUNC,
                       0);
    if (fd < 0) return -1;

    if (len) write_buf_fd(fd, installed_db, len);
    write_buf_fd(fd, name, cstrlen(name));
    write_buf_fd(fd, " ", 1);
    write_buf_fd(fd, path, cstrlen(path));
    write_buf_fd(fd, "\n", 1);
    syscall3(SYS_CLOSE, fd, 0, 0);
    return 0;
}

static void list_installed_packages(void) {
    if (load_installed_db() != 0 || !installed_db[0]) {
        write_cstr("installed packages:\n");
        write_cstr("(none)\n");
        return;
    }
    write_cstr("installed packages:\n");
    write_cstr(installed_db);
    if (installed_db[cstrlen(installed_db) - 1] != '\n') write_cstr("\n");
}

static int remove_installed_package(const char* name) {
    const char* path = live_package_path(name);
    int have_installed_path = find_installed_package_path(name, installed_path, sizeof(installed_path)) == 0;
    int removed_record;
    long rc;

    if (have_installed_path) {
        path = installed_path;
    }
    removed_record = rewrite_installed_db_without(name);
    rc = syscall3(SYS_UNLINKAT, AT_FDCWD, (long)path, 0);

    if (rc < 0 && removed_record <= 0) {
        write_cstr("acur: package is not installed: ");
        write_cstr(name);
        write_cstr("\n");
        return -1;
    }

    write_cstr("removed ");
    write_cstr(name);
    write_cstr(" from live mode\n");
    return 0;
}

static void cleanup_failed_fetch(int sock_fd, int out_fd, const char* outfile) {
    if (sock_fd >= 0) syscall3(SYS_CLOSE, sock_fd, 0, 0);
    if (out_fd >= 0) syscall3(SYS_CLOSE, out_fd, 0, 0);
    if (outfile) syscall3(SYS_UNLINKAT, AT_FDCWD, (long)outfile, 0);
}

static uint64_t build_request(const char* host, const char* path) {
    uint64_t n = 0;
    if (!path || !path[0]) path = "/";
    n = append(req, n, sizeof(req), "GET ");
    n = append(req, n, sizeof(req), path);
    n = append(req, n, sizeof(req), " HTTP/1.0\r\nHost: ");
    n = append(req, n, sizeof(req), host);
    n = append(req, n, sizeof(req), "\r\nUser-Agent: AOS-acur/0.1\r\nConnection: close\r\n\r\n");
    return n;
}

static int find_header_end(const char* buf, uint64_t len, uint64_t* body_at) {
    for (uint64_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            *body_at = i + 4;
            return 1;
        }
    }
    return 0;
}

static int http_status_code(const char* buf, uint64_t len) {
    uint64_t i = 0;
    int code = 0;
    int digits = 0;

    if (len < 12 ||
        buf[0] != 'H' || buf[1] != 'T' || buf[2] != 'T' || buf[3] != 'P' ||
        buf[4] != '/') {
        return -1;
    }

    while (i < len && buf[i] != ' ') i++;
    while (i < len && buf[i] == ' ') i++;
    while (i < len && buf[i] >= '0' && buf[i] <= '9' && digits < 3) {
        code = code * 10 + (int)(buf[i] - '0');
        digits++;
        i++;
    }
    return digits == 3 ? code : -1;
}

static int http_status_is_success(const char* buf, uint64_t len, int* code_out) {
    int code = http_status_code(buf, len);
    if (code_out) *code_out = code;
    return code >= 200 && code < 300;
}

static int lower_char(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int header_name_matches(const char* buf, uint64_t at, const char* name) {
    uint64_t i = 0;
    while (name[i]) {
        if (lower_char(buf[at + i]) != lower_char(name[i])) return 0;
        i++;
    }
    return buf[at + i] == ':';
}

static int find_header_value(const char* buf,
                             uint64_t len,
                             const char* name,
                             uint64_t* value_start,
                             uint64_t* value_end) {
    uint64_t name_len = cstrlen(name);

    for (uint64_t i = 0; i + name_len < len; i++) {
        if ((i == 0 || buf[i - 1] == '\n') && header_name_matches(buf, i, name)) {
            uint64_t p = i + name_len + 1;
            while (p < len && (buf[p] == ' ' || buf[p] == '\t')) p++;
            *value_start = p;
            while (p < len && buf[p] != '\r' && buf[p] != '\n') p++;
            *value_end = p;
            return 1;
        }
    }
    return 0;
}

static int slice_starts_with(const char* buf, uint64_t start, uint64_t end, const char* prefix) {
    uint64_t i = 0;
    while (prefix[i]) {
        if (start + i >= end || lower_char(buf[start + i]) != lower_char(prefix[i])) return 0;
        i++;
    }
    return 1;
}

static void print_slice(const char* buf, uint64_t start, uint64_t end) {
    for (uint64_t p = start; p < end; p++) {
        syscall3(SYS_WRITE, 1, (long)(buf + p), 1);
    }
}

static void print_redirect_location(const char* buf, uint64_t len) {
    uint64_t start = 0;
    uint64_t end = 0;

    if (!find_header_value(buf, len, "location", &start, &end)) return;
    write_cstr("acur: redirect location: ");
    print_slice(buf, start, end);
    write_cstr("\n");
    if (slice_starts_with(buf, start, end, "https://")) {
        write_cstr("acur: HTTPS is not supported yet; use a direct HTTP package URL\n");
    }
}

static int parse_redirect_target(const char* buf,
                                 uint64_t len,
                                 const char* current_host,
                                 char* out_host,
                                 uint64_t out_host_cap,
                                 char* out_path,
                                 uint64_t out_path_cap) {
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t host_start;
    uint64_t host_end;
    uint64_t path_start;

    if (!find_header_value(buf, len, "location", &start, &end)) return 0;
    if (slice_starts_with(buf, start, end, "https://")) return -2;
    if (slice_starts_with(buf, start, end, "http://")) {
        host_start = start + 7;
        host_end = host_start;
        while (host_end < end && buf[host_end] != '/') host_end++;
        if (host_end == host_start) return 0;
        if (!copy_bytes(out_host, out_host_cap, buf + host_start, host_end - host_start)) return 0;
        if (host_end < end) {
            path_start = host_end;
            if (!copy_bytes(out_path, out_path_cap, buf + path_start, end - path_start)) return 0;
        } else if (!copy_cstr(out_path, out_path_cap, "/")) {
            return 0;
        }
        return 1;
    }
    if (start < end && buf[start] == '/') {
        if (!copy_cstr(out_host, out_host_cap, current_host)) return 0;
        if (!copy_bytes(out_path, out_path_cap, buf + start, end - start)) return 0;
        return 1;
    }
    return 0;
}

static int load_db(void) {
    int fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"/pakages/pakages.txt", O_RDONLY, 0);
    long total = 0;
    long rc;

    if (fd < 0) {
        fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"pakages/pakages.txt", O_RDONLY, 0);
    }
    if (fd < 0) {
        fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"/pakages.txt", O_RDONLY, 0);
    }
    if (fd < 0) {
        fd = (int)syscall4(SYS_OPENAT, AT_FDCWD, (long)"pakages.txt", O_RDONLY, 0);
    }
    if (fd < 0) return -1;

    while (total + 1 < (long)sizeof(db)) {
        rc = syscall3(SYS_READ, fd, (long)(db + total), (long)(sizeof(db) - 1 - (uint64_t)total));
        if (rc < 0) {
            syscall3(SYS_CLOSE, fd, 0, 0);
            return -1;
        }
        if (rc == 0) break;
        total += rc;
    }
    syscall3(SYS_CLOSE, fd, 0, 0);
    db[total] = 0;
    return total > 0 ? 0 : -1;
}

static int split_entry(char* line, struct package_entry* out) {
    char* p = line;
    char* url;
    char* name;
    char* host;
    char* path;
    char* path_start;
    char* sha256 = (char*)0;
    uint64_t path_len;

    if (!line) return 0;
    while (is_space(*p)) p++;
    if (!p[0] || p[0] == '#') return 0;

    if (!starts_with(p, "http://")) return 0;
    url = p;
    p += 7;
    host = p;

    while (*p && !is_space(*p) && *p != '/') p++;
    if (!*p) return 0;
    if (*p == '/') {
        path_start = p;
        path_len = 0;
        while (path_start[path_len] && !is_space(path_start[path_len])) path_len++;
        if (path_len + 1 > sizeof(parsed_path)) return 0;
        for (uint64_t i = 0; i < path_len; i++) parsed_path[i] = path_start[i];
        parsed_path[path_len] = 0;
        path = parsed_path;
        *p = 0;
        p = path_start + path_len;
    } else {
        path = "/";
    }
    if (*p) *p++ = 0;

    while (is_space(*p)) p++;
    if (p[0] != '-' || p[1] != '-' || !p[2]) return 0;
    name = p + 2;
    p = name;
    while (*p && !is_space(*p)) p++;
    if (*p) *p++ = 0;
    while (is_space(*p)) p++;
    while (*p) {
        char* token = p;
        while (*p && !is_space(*p)) p++;
        if (*p) *p++ = 0;

        if (starts_with(token, "--sha256=")) {
            sha256 = token + 9;
        } else if (streq(token, "--sha256")) {
            while (is_space(*p)) p++;
            sha256 = p;
            while (*p && !is_space(*p)) p++;
            if (*p) *p++ = 0;
        }
        while (is_space(*p)) p++;
    }
    if (!name[0] || !host[0]) return 0;
    if (sha256 && !sha256_hex_valid(sha256)) return 0;

    out->name = name;
    out->host = host;
    out->path = path;
    out->outfile = name;
    out->description = url;
    out->sha256_hex = sha256;
    return 1;
}

static int next_line(uint64_t* pos, char** line) {
    uint64_t start;
    if (!db[*pos]) return 0;
    while (db[*pos] == '\n' || db[*pos] == '\r') (*pos)++;
    if (!db[*pos]) return 0;
    start = *pos;
    while (db[*pos] && db[*pos] != '\n' && db[*pos] != '\r') (*pos)++;
    if (db[*pos]) db[(*pos)++] = 0;
    *line = &db[start];
    return 1;
}

static void print_entry(const struct package_entry* entry) {
    write_cstr(entry->name);
    write_cstr(" -> /tmp/");
    write_cstr(entry->name);
    write_cstr("\n    http://");
    write_cstr(entry->host);
    write_cstr(entry->path);
    write_cstr("\n");
    if (entry->sha256_hex) {
        write_cstr("    sha256 ");
        write_cstr(entry->sha256_hex);
        write_cstr("\n");
    }
}

static int find_entry(const char* name, struct package_entry* out) {
    uint64_t pos = 0;
    char* line;
    struct package_entry entry;

    while (next_line(&pos, &line)) {
        if (split_entry(line, &entry) && streq(entry.name, name)) {
            *out = entry;
            return 0;
        }
    }
    return -1;
}

static void list_entries(void) {
    uint64_t pos = 0;
    char* line;
    struct package_entry entry;
    int count = 0;

    write_cstr("Acur package list\n");
    while (next_line(&pos, &line)) {
        if (split_entry(line, &entry)) {
            print_entry(&entry);
            count++;
        }
    }
    if (!count) write_cstr("acur: package list is empty\n");
}

static void search_entries(const char* term) {
    uint64_t pos = 0;
    char* line;
    struct package_entry entry;
    int count = 0;

    write_cstr("Acur search: ");
    write_cstr(term);
    write_cstr("\n");
    while (next_line(&pos, &line)) {
        if (split_entry(line, &entry) &&
            (contains_text(entry.name, term) ||
             contains_text(entry.host, term) ||
             contains_text(entry.path, term))) {
            print_entry(&entry);
            count++;
        }
    }
    if (!count) write_cstr("acur: no packages matched\n");
}

static void info_entry(const struct package_entry* entry) {
    write_cstr("name: ");
    write_cstr(entry->name);
    write_cstr("\nurl:  http://");
    write_cstr(entry->host);
    write_cstr(entry->path);
    write_cstr("\nhost: ");
    write_cstr(entry->host);
    write_cstr("\npath: ");
    write_cstr(entry->path);
    write_cstr("\nout:  /tmp/");
    write_cstr(entry->name);
    write_cstr("\nsha256: ");
    write_cstr(entry->sha256_hex ? entry->sha256_hex : "(none)");
    write_cstr("\n");
}

static int fetch_entry(const struct package_entry* entry, uint64_t iface_index, int install_mode) {
    struct sockaddr_in addr;
    struct sha256_ctx sha_ctx;
    uint8_t parsed_ip[4];
    uint8_t digest[32];
    uint64_t req_len;
    uint64_t header_len = 0;
    uint64_t saved = 0;
    const char* outfile = live_download_path(entry->outfile);
    int body_started = 0;
    int first_arg_is_ip;
    uint64_t redirects = 0;
    int sock_fd = -1;
    int out_fd = -1;
    long rc;

    if (install_mode) {
        write_cstr("installing ");
        write_cstr(entry->name);
        write_cstr("...\n");
    }

    if (!copy_cstr(current_host_buf, sizeof(current_host_buf), entry->host) ||
        !copy_cstr(current_path_buf, sizeof(current_path_buf), entry->path)) {
        write_cstr("acur: package URL is too long\n");
        return -1;
    }

    out_fd = (int)syscall4(SYS_OPENAT,
                           AT_FDCWD,
                           (long)outfile,
                           O_WRONLY | O_CREAT | O_TRUNC,
                           0);
    if (out_fd < 0) {
        write_cstr("acur: output open failed\n");
        return -1;
    }

    if (entry->sha256_hex) {
        sha256_init(&sha_ctx);
    }

retry_request:
    header_len = 0;
    body_started = 0;

    sock_fd = (int)syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd < 0) {
        write_cstr("acur: socket failed\n");
        cleanup_failed_fetch(sock_fd, out_fd, outfile);
        return -1;
    }
    rc = syscall3(AOS_SYS_SOCKET_BIND_NETDEV, sock_fd, (long)iface_index, 0);
    if (rc < 0) {
        write_cstr("acur: interface bind failed\n");
        cleanup_failed_fetch(sock_fd, out_fd, outfile);
        return -1;
    }

    for (uint64_t i = 0; i < sizeof(addr); i++) ((uint8_t*)&addr)[i] = 0;
    addr.sin_family = AF_INET;
    put_be16((uint8_t*)&addr.sin_port, 80);

    first_arg_is_ip = parse_ipv4(current_host_buf, parsed_ip);
    if (first_arg_is_ip) {
        for (uint64_t i = 0; i < sizeof(parsed_ip); i++) addr.sin_addr[i] = parsed_ip[i];
    } else {
        write_cstr("dns: query ");
        write_cstr(current_host_buf);
        write_cstr("\n");
        rc = -1;
        for (uint64_t attempt = 0; attempt < 3 && rc < 0; attempt++) {
            rc = syscall3(AOS_SYS_DNS_LOOKUP, (long)current_host_buf, (long)addr.sin_addr, (long)iface_index);
        }
        if (rc < 0) {
            write_cstr("acur: DNS lookup failed\n");
            cleanup_failed_fetch(sock_fd, out_fd, outfile);
            return -1;
        }
    }

    rc = syscall3(SYS_CONNECT, sock_fd, (long)&addr, sizeof(addr));
    if (rc < 0) {
        write_cstr("acur: connect failed\n");
        cleanup_failed_fetch(sock_fd, out_fd, outfile);
        return -1;
    }

    req_len = build_request(current_host_buf, current_path_buf);
    rc = syscall3(SYS_WRITE, sock_fd, (long)req, req_len);
    if (rc < 0) {
        write_cstr("acur: request write failed\n");
        cleanup_failed_fetch(sock_fd, out_fd, outfile);
        return -1;
    }

    for (;;) {
        rc = syscall3(SYS_READ, sock_fd, (long)rx, sizeof(rx));
        if (rc < 0) {
            write_cstr("acur: network read failed\n");
            cleanup_failed_fetch(sock_fd, out_fd, outfile);
            return -1;
        }
        if (rc == 0) break;
        if (!body_started) {
            uint64_t body_at = 0;
            if (header_len + (uint64_t)rc > sizeof(header)) {
                write_cstr("acur: HTTP header too large\n");
                cleanup_failed_fetch(sock_fd, out_fd, outfile);
                return -1;
            }
            for (uint64_t i = 0; i < (uint64_t)rc; i++) header[header_len + i] = rx[i];
            header_len += (uint64_t)rc;
            if (!find_header_end(header, header_len, &body_at)) continue;
            {
                int status = 0;
                if (!http_status_is_success(header, header_len, &status)) {
                    if (status >= 300 && status < 400 && redirects < 3) {
                        int redirect = parse_redirect_target(header,
                                                             header_len,
                                                             current_host_buf,
                                                             redirect_host_buf,
                                                             sizeof(redirect_host_buf),
                                                             redirect_path_buf,
                                                             sizeof(redirect_path_buf));
                        print_redirect_location(header, header_len);
                        if (redirect == 1) {
                            if (!copy_cstr(current_host_buf, sizeof(current_host_buf), redirect_host_buf) ||
                                !copy_cstr(current_path_buf, sizeof(current_path_buf), redirect_path_buf)) {
                                write_cstr("acur: redirect URL is too long\n");
                                cleanup_failed_fetch(sock_fd, out_fd, outfile);
                                return -1;
                            }
                            redirects++;
                            write_cstr("acur: following HTTP redirect\n");
                            syscall3(SYS_CLOSE, sock_fd, 0, 0);
                            sock_fd = -1;
                            goto retry_request;
                        }
                    }
                    write_cstr("acur: HTTP request failed");
                    if (status > 0) {
                        write_cstr(" status=");
                        write_u64((uint64_t)status);
                    }
                    write_cstr("\n");
                    if (status >= 300 && status < 400 && redirects >= 3) {
                        print_redirect_location(header, header_len);
                    }
                    cleanup_failed_fetch(sock_fd, out_fd, outfile);
                    return -1;
                }
            }
            body_started = 1;
            if (body_at < header_len) {
                uint64_t body_len = header_len - body_at;
                if (entry->sha256_hex) {
                    sha256_update(&sha_ctx, (const uint8_t*)(header + body_at), body_len);
                }
                write_buf_fd(out_fd, header + body_at, body_len);
                saved += body_len;
            }
        } else {
            if (entry->sha256_hex) {
                sha256_update(&sha_ctx, (const uint8_t*)rx, (uint64_t)rc);
            }
            write_buf_fd(out_fd, rx, (uint64_t)rc);
            saved += (uint64_t)rc;
        }
    }

    syscall3(SYS_CLOSE, sock_fd, 0, 0);
    sock_fd = -1;
    syscall3(SYS_CLOSE, out_fd, 0, 0);
    out_fd = -1;
    if (!body_started) {
        write_cstr("acur: no HTTP body received\n");
        syscall3(SYS_UNLINKAT, AT_FDCWD, (long)outfile, 0);
        return -1;
    }

    if (entry->sha256_hex) {
        sha256_final(&sha_ctx, digest);
        bytes_to_hex(digest, sizeof(digest), digest_hex);
        if (!hex_digest_matches(digest_hex, entry->sha256_hex)) {
            write_cstr("acur: sha256 mismatch\nexpected: ");
            write_cstr(entry->sha256_hex);
            write_cstr("\nactual:   ");
            write_cstr(digest_hex);
            write_cstr("\n");
            syscall3(SYS_UNLINKAT, AT_FDCWD, (long)outfile, 0);
            return -1;
        }
        write_cstr("acur: sha256 verified ");
        write_cstr(digest_hex);
        write_cstr("\n");
    }

    if (install_mode) {
        if (record_installed_package(entry->name, outfile) != 0) {
            write_cstr("acur: install database update failed\n");
            syscall3(SYS_UNLINKAT, AT_FDCWD, (long)outfile, 0);
            return -1;
        }
        write_cstr("installed ");
        write_cstr(entry->name);
        write_cstr(" to ");
        write_cstr(outfile);
        write_cstr(" (");
        write_u64(saved);
        write_cstr(" bytes)\n");
    } else {
        write_cstr("acur: fetched ");
        write_cstr(entry->name);
        write_cstr(" -> ");
        write_cstr(outfile);
        write_cstr(" (");
        write_u64(saved);
        write_cstr(" bytes)\n");
    }
    return 0;
}

static void usage(void) {
    write_cstr("usage: acur list | acur search TEXT | acur installed | acur info NAME | acur fetch NAME | acur install NAME | acur remove NAME | acur sha256 TEXT [--iface N]\n");
    write_cstr("package format: http://host/path --name [--sha256 HEX]\n");
    write_cstr("live mode: installed packages are saved in /tmp only\n");
}

static void print_sha256_text(const char* text) {
    struct sha256_ctx ctx;
    uint8_t digest[32];

    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)text, cstrlen(text));
    sha256_final(&ctx, digest);
    bytes_to_hex(digest, sizeof(digest), digest_hex);
    write_cstr(digest_hex);
    write_cstr("  ");
    write_cstr(text);
    write_cstr("\n");
}

void aos_main(uint64_t argc, char** argv) {
    struct package_entry entry;
    uint64_t iface_index = 0;

    if (argc >= 5 && streq(argv[argc - 2], "--iface")) {
        if (!parse_iface_index(argv[argc - 1], &iface_index)) {
            usage();
            exit_code(1);
        }
        argc -= 2;
    }

    if (argc == 1 || (argc == 2 && streq(argv[1], "list"))) {
        if (load_db() != 0) {
            write_cstr("acur: could not read package list\n");
            exit_code(1);
        }
        list_entries();
        exit_code(0);
    }

    if (argc == 2 && streq(argv[1], "installed")) {
        list_installed_packages();
        exit_code(0);
    }

    if (argc == 2 && (streq(argv[1], "-h") || streq(argv[1], "--help"))) {
        usage();
        exit_code(0);
    }

    if (argc != 3 ||
        (!streq(argv[1], "info") &&
         !streq(argv[1], "search") &&
         !streq(argv[1], "fetch") &&
         !streq(argv[1], "install") &&
         !streq(argv[1], "remove") &&
         !streq(argv[1], "sha256"))) {
        usage();
        exit_code(1);
    }

    if (streq(argv[1], "sha256")) {
        print_sha256_text(argv[2]);
        exit_code(0);
    }

    if (streq(argv[1], "remove")) {
        exit_code(remove_installed_package(argv[2]) == 0 ? 0 : 1);
    }

    if (load_db() != 0) {
        write_cstr("acur: could not read package list\n");
        exit_code(1);
    }

    if (streq(argv[1], "search")) {
        search_entries(argv[2]);
        exit_code(0);
    }

    if (find_entry(argv[2], &entry) != 0) {
        write_cstr("acur: package not found: ");
        write_cstr(argv[2]);
        write_cstr("\n");
        exit_code(1);
    }

    if (streq(argv[1], "info")) {
        info_entry(&entry);
        exit_code(0);
    }

    exit_code(fetch_entry(&entry, iface_index, streq(argv[1], "install")) == 0 ? 0 : 1);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov (%rsp), %rdi\n"
        "lea 8(%rsp), %rsi\n"
        "call aos_main\n"
    );
}
