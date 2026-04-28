// SPDX-License-Identifier: MIT

#include "bc_duplicate_strings_internal.h"
#include "bc_duplicate_throughput_internal.h"

#include "bc_core.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#define BC_DUPLICATE_THROUGHPUT_CACHE_SIGNATURE_CAPACITY ((size_t)256)
#define BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY      ((size_t)512)

static bool bc_duplicate_throughput_cache_read_cpuinfo_field(const char* field_name, char* out_value, size_t value_capacity)
{
    int fd = open("/proc/cpuinfo", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    char reader_buffer[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    bc_core_reader_t reader;
    if (!bc_core_reader_init(&reader, fd, reader_buffer, sizeof(reader_buffer))) {
        close(fd);
        return false;
    }
    size_t field_name_length = bc_duplicate_strings_length(field_name);
    bool found = false;
    const char* line = NULL;
    size_t line_length = 0;
    while (bc_core_reader_read_line(&reader, &line, &line_length)) {
        if (line_length < field_name_length) {
            continue;
        }
        bool prefix_matches = false;
        bc_core_equal(line, field_name, field_name_length, &prefix_matches);
        if (!prefix_matches) {
            continue;
        }
        size_t cursor_offset = field_name_length;
        while (cursor_offset < line_length && (line[cursor_offset] == ' ' || line[cursor_offset] == '\t')) {
            cursor_offset++;
        }
        if (cursor_offset >= line_length || line[cursor_offset] != ':') {
            continue;
        }
        cursor_offset++;
        while (cursor_offset < line_length && (line[cursor_offset] == ' ' || line[cursor_offset] == '\t')) {
            cursor_offset++;
        }
        size_t copied = 0;
        while (cursor_offset < line_length && copied + 1 < value_capacity) {
            out_value[copied] = line[cursor_offset];
            cursor_offset++;
            copied++;
        }
        out_value[copied] = '\0';
        found = true;
        break;
    }
    bc_core_reader_destroy(&reader);
    close(fd);
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
    int fd = open(absolute_cache_path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
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

    char reader_buffer[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    bc_core_reader_t reader;
    if (!bc_core_reader_init(&reader, fd, reader_buffer, sizeof(reader_buffer))) {
        close(fd);
        return false;
    }

    char line_storage[BC_DUPLICATE_THROUGHPUT_CACHE_LINE_CAPACITY];
    const char* line = NULL;
    size_t line_length = 0;
    while (bc_core_reader_read_line(&reader, &line, &line_length)) {
        if (line_length >= sizeof(line_storage)) {
            continue;
        }
        bc_core_copy(line_storage, line, line_length);
        line_storage[line_length] = '\0';
        size_t separator_offset = 0;
        if (!bc_core_find_byte(line_storage, line_length, (unsigned char)'=', &separator_offset)) {
            continue;
        }
        line_storage[separator_offset] = '\0';
        const char* key = line_storage;
        const char* value = line_storage + separator_offset + 1;
        size_t value_length = line_length - separator_offset - 1U;

        if (bc_duplicate_strings_equal(key, "cpu_model")) {
            size_t copy_length = value_length + 1U > sizeof(cached_cpu_model) ? sizeof(cached_cpu_model) - 1U : value_length;
            bc_core_copy(cached_cpu_model, value, copy_length);
            cached_cpu_model[copy_length] = '\0';
        } else if (bc_duplicate_strings_equal(key, "microcode")) {
            size_t copy_length = value_length + 1U > sizeof(cached_microcode) ? sizeof(cached_microcode) - 1U : value_length;
            bc_core_copy(cached_microcode, value, copy_length);
            cached_microcode[copy_length] = '\0';
        } else if (bc_duplicate_strings_equal(key, "kernel_version")) {
            size_t copy_length = value_length + 1U > sizeof(cached_kernel_version) ? sizeof(cached_kernel_version) - 1U : value_length;
            bc_core_copy(cached_kernel_version, value, copy_length);
            cached_kernel_version[copy_length] = '\0';
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
    bc_core_reader_destroy(&reader);
    close(fd);

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
    size_t source_length = bc_duplicate_strings_length(absolute_cache_path);
    if (source_length + 1U > sizeof(directory_path)) {
        return false;
    }
    bc_core_copy(directory_path, absolute_cache_path, source_length);
    directory_path[source_length] = '\0';
    size_t last_slash_offset = 0;
    if (!bc_core_find_last_byte(directory_path, source_length, (unsigned char)'/', &last_slash_offset) || last_slash_offset == 0) {
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
    int fd = open(absolute_cache_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return false;
    }
    char writer_buffer[1024];
    bc_core_writer_t writer;
    if (!bc_core_writer_init(&writer, fd, writer_buffer, sizeof(writer_buffer))) {
        close(fd);
        return false;
    }
    bool ok = true;
    ok = ok && bc_core_writer_write_cstring(&writer, "cpu_model=");
    ok = ok && bc_core_writer_write_cstring(&writer, cpu_model);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "microcode=");
    ok = ok && bc_core_writer_write_cstring(&writer, microcode);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "kernel_version=");
    ok = ok && bc_core_writer_write_cstring(&writer, kernel_version);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "xxh3_gbps=");
    ok = ok && bc_core_writer_write_double(&writer, constants->xxh3_gigabytes_per_second, 6);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "xxh128_gbps=");
    ok = ok && bc_core_writer_write_double(&writer, constants->xxh128_gigabytes_per_second, 6);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "sha256_gbps=");
    ok = ok && bc_core_writer_write_double(&writer, constants->sha256_gigabytes_per_second, 6);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "mem_bw_gbps=");
    ok = ok && bc_core_writer_write_double(&writer, constants->memory_bandwidth_gigabytes_per_second, 6);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "parallel_startup_us=");
    ok = ok && bc_core_writer_write_double(&writer, constants->parallel_startup_overhead_microseconds, 6);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    ok = ok && bc_core_writer_write_cstring(&writer, "per_file_cost_us=");
    ok = ok && bc_core_writer_write_double(&writer, constants->per_file_cost_warm_microseconds, 6);
    ok = ok && bc_core_writer_write_char(&writer, '\n');
    if (!bc_core_writer_destroy(&writer)) {
        ok = false;
    }
    int close_status = close(fd);
    return ok && close_status == 0;
}

static bool bc_duplicate_throughput_cache_compose_path(char* out_path, size_t path_capacity, const char* prefix, const char* suffix)
{
    if (path_capacity == 0) {
        return false;
    }
    bc_core_writer_t writer;
    if (!bc_core_writer_init_buffer_only(&writer, out_path, path_capacity - 1U)) {
        return false;
    }
    if (!bc_core_writer_write_cstring(&writer, prefix)) {
        return false;
    }
    if (!bc_core_writer_write_cstring(&writer, suffix)) {
        return false;
    }
    if (bc_core_writer_has_error(&writer)) {
        return false;
    }
    const char* data = NULL;
    size_t length = 0;
    if (!bc_core_writer_buffer_data(&writer, &data, &length)) {
        return false;
    }
    out_path[length] = '\0';
    return true;
}

static bool bc_duplicate_throughput_cache_default_path(char* out_path, size_t path_capacity)
{
    const char* xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home != NULL && xdg_cache_home[0] != '\0') {
        return bc_duplicate_throughput_cache_compose_path(out_path, path_capacity, xdg_cache_home, "/bc-duplicate/throughput.txt");
    }
    const char* home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return false;
    }
    return bc_duplicate_throughput_cache_compose_path(out_path, path_capacity, home, "/.cache/bc-duplicate/throughput.txt");
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
