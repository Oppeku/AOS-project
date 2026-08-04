/* Host-side regression test for the freestanding Uni gzip decoder. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../userspace/lib/uni_inflate.h"

static unsigned char* read_file(const char* path, size_t* size) {
    FILE* file = fopen(path, "rb");
    unsigned char* data;
    long length;

    if (!file || fseek(file, 0, SEEK_END) != 0 ||
        (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        if (file) fclose(file);
        return NULL;
    }
    data = malloc((size_t)length + 1);
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (size_t)length;
    return data;
}

int main(int argc, char** argv) {
    unsigned char* compressed;
    unsigned char* expected;
    unsigned char* output;
    size_t compressed_size;
    size_t expected_size;
    size_t output_size = 0;
    int result;

    if (argc != 3) return 2;
    compressed = read_file(argv[1], &compressed_size);
    expected = read_file(argv[2], &expected_size);
    output = malloc(expected_size + 1);
    if (!compressed || !expected || !output) return 2;

    result = uni_inflate_gzip(compressed, compressed_size,
                              output, expected_size + 1, &output_size);
    if (result != 0 || output_size != expected_size ||
        memcmp(output, expected, expected_size) != 0) {
        fprintf(stderr, "inflate mismatch: rc=%d got=%zu expected=%zu\n",
                result, output_size, expected_size);
        return 1;
    }
    return 0;
}
