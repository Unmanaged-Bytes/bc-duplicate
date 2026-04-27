// SPDX-License-Identifier: MIT

#include "bc_allocators.h"
#include "bc_core.h"
#include "bc_duplicate_filter_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 3) {
        return 0;
    }

    const uint8_t include_length = data[0];
    const uint8_t exclude_length = data[1];
    size_t position = 2;
    if (position + include_length + exclude_length > size) {
        return 0;
    }

    char include_buffer[512];
    char exclude_buffer[512];
    const size_t include_actual = (size_t)include_length;
    const size_t exclude_actual = (size_t)exclude_length;

    memcpy(include_buffer, data + position, include_actual);
    include_buffer[include_actual] = '\0';
    position += include_length;
    memcpy(exclude_buffer, data + position, exclude_actual);
    exclude_buffer[exclude_actual] = '\0';
    position += exclude_length;

    bc_allocators_context_t* memory_context = NULL;
    if (!bc_allocators_context_create(NULL, &memory_context)) {
        return 0;
    }

    const char* include_ptr = include_length == 0 ? NULL : include_buffer;
    const char* exclude_ptr = exclude_length == 0 ? NULL : exclude_buffer;

    bc_duplicate_filter_t* filter = NULL;
    if (!bc_duplicate_filter_create(memory_context, include_ptr, exclude_ptr, &filter)) {
        bc_allocators_context_destroy(memory_context);
        return 0;
    }

    char basename_buffer[256];
    const size_t basename_available = size - position;
    const size_t basename_actual = basename_available < sizeof(basename_buffer) - 1 ? basename_available : sizeof(basename_buffer) - 1;
    memcpy(basename_buffer, data + position, basename_actual);
    basename_buffer[basename_actual] = '\0';

    (void)bc_duplicate_filter_accepts_file(filter, basename_buffer);
    (void)bc_duplicate_filter_accepts_directory(filter, basename_buffer);

    bc_duplicate_filter_destroy(memory_context, filter);
    bc_allocators_context_destroy(memory_context);
    return 0;
}

#ifndef BC_FUZZ_LIBFUZZER
int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <iterations> [seed]\n", argv[0]);
        return 2;
    }
    uint64_t iterations = 0;
    size_t iterations_consumed = 0;
    const size_t iterations_length = strlen(argv[1]);
    if (!bc_core_parse_unsigned_integer_64_decimal(argv[1], iterations_length, &iterations, &iterations_consumed)
        || iterations_consumed != iterations_length) {
        fprintf(stderr, "%s: invalid iterations value '%s'\n", argv[0], argv[1]);
        return 2;
    }
    uint64_t seed = 0;
    if (argc >= 3) {
        size_t seed_consumed = 0;
        const size_t seed_length = strlen(argv[2]);
        if (!bc_core_parse_unsigned_integer_64_decimal(argv[2], seed_length, &seed, &seed_consumed) || seed_consumed != seed_length) {
            fprintf(stderr, "%s: invalid seed value '%s'\n", argv[0], argv[2]);
            return 2;
        }
    }
    srand((unsigned int)seed);

    uint8_t buffer[1024];
    for (uint64_t i = 0; i < iterations; i++) {
        const size_t length = (size_t)(rand() % (int)sizeof(buffer));
        for (size_t j = 0; j < length; j++) {
            buffer[j] = (uint8_t)(rand() & 0xFF);
        }
        LLVMFuzzerTestOneInput(buffer, length);
    }
    return 0;
}
#endif
