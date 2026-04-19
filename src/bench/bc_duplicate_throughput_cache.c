// SPDX-License-Identifier: MIT

#include "bc_duplicate_strings_internal.h"
#include "bc_duplicate_throughput_internal.h"

#include "bc_core.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#define BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY ((size_t)256)
#define BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY      ((size_t)512)

static bool bc_duplicate_throughput_cache_read_cpuinfo_field(const char* field_name, char* out_value, size_t value_capacity)
{
    FILE* stream = fopen("/proc/cpuinfo", "r");
    if (stream == NULL) {
        return false;
    }
    char line_buffer[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    size_t field_name_length = bc_duplicate_strings_length(field_name);
    bool found = false;
    while (fgets(line_buffer, sizeof(line_buffer), stream) != NULL) {
        size_t line_buffer_length = bc_duplicate_strings_length(line_buffer);
        if (line_buffer_length < field_name_length) {
            continue;
        }
        bool prefix_matches = false;
        bc_core_equal(line_buffer, field_name, field_name_length, &prefix_matches);
        if (!prefix_matches) {
            continue;
        }
        const char* cursor = line_buffer + field_name_length;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor != ':') {
            continue;
        }
        cursor++;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        size_t copied = 0;
        while (*cursor != '\n' && *cursor != '\0' && copied + 1 < value_capacity) {
            out_value[copied] = *cursor;
            cursor++;
            copied++;
        }
        out_value[copied] = '\0';
        found = true;
        break;
    }
    fclose(stream);
    return found;
}

bool bc_duplicate_throughput_cache_read_host_signature(char* out_cpu_model, size_t cpu_model_capacity, char* out_microcode,
                                                       size_t microcode_capacity, char* out_kernel_version, size_t kernel_version_capacity)
{
    if (!bc_duplicate_throughput_cache_read_cpuinfo_field("model name", out_cpu_model, cpu_model_capacity)) {
        return false;
    }
    if (!bc_duplicate_throughput_cache_read_cpuinfo_field("microcode", out_microcode, microcode_capacity)) {
        out_microcode[0] = '\0';
    }
    struct utsname uname_info;
    if (uname(&uname_info) != 0) {
        return false;
    }
    size_t copied = 0;
    while (uname_info.release[copied] != '\0' && copied + 1 < kernel_version_capacity) {
        out_kernel_version[copied] = uname_info.release[copied];
        copied++;
    }
    out_kernel_version[copied] = '\0';
    return true;
}

static bool bc_duplicate_throughput_cache_parse_double(const char* value_text, double* out_value)
{
    char* end_pointer = NULL;
    double parsed = strtod(value_text, &end_pointer);
    if (end_pointer == value_text) {
        return false;
    }
    *out_value = parsed;
    return true;
}

bool bc_duplicate_throughput_cache_load(const char* absolute_cache_path, bc_duplicate_throughput_constants_t* out_constants)
{
    FILE* stream = fopen(absolute_cache_path, "r");
    if (stream == NULL) {
        return false;
    }

    char cached_cpu_model[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY] = {0};
    char cached_microcode[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY] = {0};
    char cached_kernel_version[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY] = {0};
    bool xxh3_ok = false;
    bool xxh128_ok = false;
    bool sha256_ok = false;
    bool memory_bandwidth_ok = false;
    bool parallel_startup_ok = false;
    bool per_file_cost_ok = false;

    char line_buffer[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    while (fgets(line_buffer, sizeof(line_buffer), stream) != NULL) {
        size_t line_length = bc_duplicate_strings_length(line_buffer);
        if (line_length > 0 && line_buffer[line_length - 1] == '\n') {
            line_buffer[line_length - 1] = '\0';
            line_length--;
        }
        size_t separator_offset = 0;
        if (!bc_core_find_byte(line_buffer, line_length, (unsigned char)'=', &separator_offset)) {
            continue;
        }
        line_buffer[separator_offset] = '\0';
        const char* key = line_buffer;
        const char* value = line_buffer + separator_offset + 1;

        if (bc_duplicate_strings_equal(key, "cpu_model")) {
            snprintf(cached_cpu_model, sizeof(cached_cpu_model), "%s", value);
        } else if (bc_duplicate_strings_equal(key, "microcode")) {
            snprintf(cached_microcode, sizeof(cached_microcode), "%s", value);
        } else if (bc_duplicate_strings_equal(key, "kernel_version")) {
            snprintf(cached_kernel_version, sizeof(cached_kernel_version), "%s", value);
        } else if (bc_duplicate_strings_equal(key, "xxh3_gbps")) {
            xxh3_ok = bc_duplicate_throughput_cache_parse_double(value, &out_constants->xxh3_gigabytes_per_second);
        } else if (bc_duplicate_strings_equal(key, "xxh128_gbps")) {
            xxh128_ok = bc_duplicate_throughput_cache_parse_double(value, &out_constants->xxh128_gigabytes_per_second);
        } else if (bc_duplicate_strings_equal(key, "sha256_gbps")) {
            sha256_ok = bc_duplicate_throughput_cache_parse_double(value, &out_constants->sha256_gigabytes_per_second);
        } else if (bc_duplicate_strings_equal(key, "mem_bw_gbps")) {
            memory_bandwidth_ok = bc_duplicate_throughput_cache_parse_double(value, &out_constants->memory_bandwidth_gigabytes_per_second);
        } else if (bc_duplicate_strings_equal(key, "parallel_startup_us")) {
            parallel_startup_ok = bc_duplicate_throughput_cache_parse_double(value, &out_constants->parallel_startup_overhead_microseconds);
        } else if (bc_duplicate_strings_equal(key, "per_file_cost_us")) {
            per_file_cost_ok = bc_duplicate_throughput_cache_parse_double(value, &out_constants->per_file_cost_warm_microseconds);
        }
    }
    fclose(stream);

    if (!xxh3_ok || !xxh128_ok || !sha256_ok || !memory_bandwidth_ok || !parallel_startup_ok || !per_file_cost_ok) {
        return false;
    }

    char current_cpu_model[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY];
    char current_microcode[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY];
    char current_kernel_version[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY];
    if (!bc_duplicate_throughput_cache_read_host_signature(current_cpu_model, sizeof(current_cpu_model), current_microcode,
                                                            sizeof(current_microcode), current_kernel_version, sizeof(current_kernel_version))) {
        return false;
    }
    if (!bc_duplicate_strings_equal(cached_cpu_model, current_cpu_model)) {
        return false;
    }
    if (!bc_duplicate_strings_equal(cached_microcode, current_microcode)) {
        return false;
    }
    if (!bc_duplicate_strings_equal(cached_kernel_version, current_kernel_version)) {
        return false;
    }
    return true;
}

static bool bc_duplicate_throughput_cache_ensure_parent_directory(const char* absolute_cache_path)
{
    char directory_path[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    snprintf(directory_path, sizeof(directory_path), "%s", absolute_cache_path);
    size_t directory_path_length = bc_duplicate_strings_length(directory_path);
    size_t last_slash_offset = 0;
    if (!bc_core_find_last_byte(directory_path, directory_path_length, (unsigned char)'/', &last_slash_offset) || last_slash_offset == 0) {
        return true;
    }
    directory_path[last_slash_offset] = '\0';
    if (mkdir(directory_path, 0755) == 0) {
        return true;
    }
    if (errno == EEXIST) {
        return true;
    }
    return false;
}

bool bc_duplicate_throughput_cache_store(const char* absolute_cache_path, const bc_duplicate_throughput_constants_t* constants)
{
    if (!bc_duplicate_throughput_cache_ensure_parent_directory(absolute_cache_path)) {
        return false;
    }
    char cpu_model[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY];
    char microcode[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY];
    char kernel_version[BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY];
    if (!bc_duplicate_throughput_cache_read_host_signature(cpu_model, sizeof(cpu_model), microcode, sizeof(microcode), kernel_version,
                                                            sizeof(kernel_version))) {
        return false;
    }
    FILE* stream = fopen(absolute_cache_path, "w");
    if (stream == NULL) {
        return false;
    }
    int printed = fprintf(stream,
                          "cpu_model=%s\n"
                          "microcode=%s\n"
                          "kernel_version=%s\n"
                          "xxh3_gbps=%.6f\n"
                          "xxh128_gbps=%.6f\n"
                          "sha256_gbps=%.6f\n"
                          "mem_bw_gbps=%.6f\n"
                          "parallel_startup_us=%.6f\n"
                          "per_file_cost_us=%.6f\n",
                          cpu_model, microcode, kernel_version, constants->xxh3_gigabytes_per_second, constants->xxh128_gigabytes_per_second,
                          constants->sha256_gigabytes_per_second, constants->memory_bandwidth_gigabytes_per_second,
                          constants->parallel_startup_overhead_microseconds, constants->per_file_cost_warm_microseconds);
    int close_status = fclose(stream);
    return printed > 0 && close_status == 0;
}

static bool bc_duplicate_throughput_cache_default_path(char* out_path, size_t path_capacity)
{
    const char* xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home != NULL && xdg_cache_home[0] != '\0') {
        int written = snprintf(out_path, path_capacity, "%s/bc-duplicate/throughput.txt", xdg_cache_home);
        return written > 0 && (size_t)written < path_capacity;
    }
    const char* home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return false;
    }
    int written = snprintf(out_path, path_capacity, "%s/.cache/bc-duplicate/throughput.txt", home);
    return written > 0 && (size_t)written < path_capacity;
}

bool bc_duplicate_throughput_get_or_measure(bc_concurrency_context_t* concurrency_context, bc_duplicate_throughput_constants_t* out_constants)
{
    char cache_path[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    bool have_cache_path = bc_duplicate_throughput_cache_default_path(cache_path, sizeof(cache_path));

    if (have_cache_path && bc_duplicate_throughput_cache_load(cache_path, out_constants)) {
        return true;
    }
    if (!bc_duplicate_throughput_measure(concurrency_context, out_constants)) {
        return false;
    }
    if (have_cache_path) {
        (void)bc_duplicate_throughput_cache_store(cache_path, out_constants);
    }
    return true;
}
