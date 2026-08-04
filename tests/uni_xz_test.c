/* Host-side regression test for Uni's freestanding XZ decoder. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../userspace/lib/uni_xz.h"

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
    size_t advertised_size = 0;
    size_t output_size = 0;
    size_t stream_input_at = 0;
    size_t stream_output_at = 0;
    uint32_t dictionary_size = 0;
    unsigned char* workspace;
    struct uni_xz_stream stream;
    int result;

    if (argc != 3) return 2;
    compressed = read_file(argv[1], &compressed_size);
    expected = read_file(argv[2], &expected_size);
    output = malloc(expected_size + 1);
    workspace = malloc(65U * 1024U * 1024U);
    if (!compressed || !expected || !output || !workspace) return 2;

    result = uni_xz_output_size(compressed, compressed_size,
                                &advertised_size);
    if (result == 0) {
        result = uni_xz_dictionary_size(compressed, compressed_size,
                                        &dictionary_size);
    }
    if (result == 0) {
        result = uni_xz_decode(compressed, compressed_size,
                               output, expected_size + 1, &output_size);
    }
    if (result != 0 || dictionary_size == 0 ||
        dictionary_size > 64U * 1024U * 1024U ||
        advertised_size != expected_size ||
        output_size != expected_size ||
        memcmp(output, expected, expected_size) != 0) {
        fprintf(stderr,
                "xz mismatch: rc=%d advertised=%zu got=%zu expected=%zu\n",
                result, advertised_size, output_size, expected_size);
        return 1;
    }

    memset(output, 0, expected_size + 1);
    result = uni_xz_stream_init(&stream, workspace,
                                65U * 1024U * 1024U,
                                64U * 1024U * 1024U);
    while (result == 0 && !stream.finished &&
           stream_input_at < compressed_size) {
        size_t input_size = compressed_size - stream_input_at;
        size_t output_capacity = expected_size - stream_output_at;
        size_t input_used = 0;
        size_t output_used = 0;

        if (input_size > 37) input_size = 37;
        if (output_capacity > 113) output_capacity = 113;
        result = uni_xz_stream_decode(
            &stream, compressed + stream_input_at, input_size, &input_used,
            output + stream_output_at, output_capacity, &output_used);
        stream_input_at += input_used;
        stream_output_at += output_used;
        if (result == 0 && input_used == 0 && output_used == 0) {
            result = -1;
        }
    }
    if (result != 0 || !stream.finished ||
        stream_input_at != compressed_size ||
        stream_output_at != expected_size ||
        memcmp(output, expected, expected_size) != 0) {
        fprintf(stderr,
                "xz stream mismatch: rc=%d in=%zu/%zu out=%zu/%zu end=%d\n",
                result, stream_input_at, compressed_size,
                stream_output_at, expected_size, stream.finished);
        uni_xz_stream_end(&stream);
        return 1;
    }
    uni_xz_stream_end(&stream);
    return 0;
}
