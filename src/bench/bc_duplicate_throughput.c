// SPDX-License-Identifier: MIT

#include "bc_duplicate_throughput_internal.h"

#include "bc_concurrency.h"
#include "bc_core.h"
#include "bc_core_hash.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <xxhash.h>

#define BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES            ((size_t)(64 * 1024 * 1024))
#define BC_DUPLICATE_THROUGHPUT_BUFFER_ALIGNMENT        ((size_t)4096)
#define BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT    ((size_t)5)
#define BC_DUPLICATE_THROUGHPUT_MEMORY_ITERATION_COUNT  ((size_t)5)
#define BC_DUPLICATE_THROUGHPUT_STARTUP_ITERATION_COUNT ((size_t)32)
#define BC_DUPLICATE_THROUGHPUT_PER_FILE_ITERATION_COUNT ((size_t)256)
#define BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE     ((size_t)100)

static double bc_duplicate_throughput_monotonic_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static bool bc_duplicate_throughput_double_less_than(const void* left, const void* right, void* user_data)
{
    (void)user_data;
    double left_value = *(const double*)left;
    double right_value = *(const double*)right;
    return left_value < right_value;
}

static double bc_duplicate_throughput_median_seconds(double* samples, size_t sample_count)
{
    bc_core_sort_with_compare(samples, sample_count, sizeof(double), bc_duplicate_throughput_double_less_than, NULL);
    return samples[sample_count / 2];
}

static bool bc_duplicate_throughput_allocate_buffer(uint8_t** out_buffer)
{
    void* raw = NULL;
    if (posix_memalign(&raw, BC_DUPLICATE_THROUGHPUT_BUFFER_ALIGNMENT, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES) != 0) {
        return false;
    }
    uint8_t* buffer = (uint8_t*)raw;
    for (size_t offset = 0; offset < BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES; ++offset) {
        buffer[offset] = (uint8_t)(offset & 0xFFU);
    }
    *out_buffer = buffer;
    return true;
}

static volatile uint64_t bc_duplicate_throughput_benchmark_sink;

static double bc_duplicate_throughput_measure_xxh3(const uint8_t* buffer)
{
    XXH64_hash_t digest = XXH3_64bits(buffer, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES);
    bc_duplicate_throughput_benchmark_sink += digest;

    double samples[BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT];
    for (size_t iteration_index = 0; iteration_index < BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT; ++iteration_index) {
        double start_seconds = bc_duplicate_throughput_monotonic_seconds();
        digest = XXH3_64bits(buffer, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES);
        double end_seconds = bc_duplicate_throughput_monotonic_seconds();
        samples[iteration_index] = end_seconds - start_seconds;
        bc_duplicate_throughput_benchmark_sink += digest;
    }
    double median_seconds = bc_duplicate_throughput_median_seconds(samples, BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT);
    double bytes_per_second = (double)BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES / median_seconds;
    return bytes_per_second / 1e9;
}

static double bc_duplicate_throughput_measure_xxh128(const uint8_t* buffer)
{
    XXH128_hash_t digest = XXH3_128bits(buffer, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES);
    bc_duplicate_throughput_benchmark_sink += digest.low64;

    double samples[BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT];
    for (size_t iteration_index = 0; iteration_index < BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT; ++iteration_index) {
        double start_seconds = bc_duplicate_throughput_monotonic_seconds();
        digest = XXH3_128bits(buffer, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES);
        double end_seconds = bc_duplicate_throughput_monotonic_seconds();
        samples[iteration_index] = end_seconds - start_seconds;
        bc_duplicate_throughput_benchmark_sink += digest.low64;
    }
    double median_seconds = bc_duplicate_throughput_median_seconds(samples, BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT);
    double bytes_per_second = (double)BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES / median_seconds;
    return bytes_per_second / 1e9;
}

static double bc_duplicate_throughput_measure_sha256(const uint8_t* buffer)
{
    uint8_t digest[BC_CORE_SHA256_DIGEST_SIZE];
    bc_core_sha256(buffer, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES, digest);
    bc_duplicate_throughput_benchmark_sink += digest[0];

    double samples[BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT];
    for (size_t iteration_index = 0; iteration_index < BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT; ++iteration_index) {
        double start_seconds = bc_duplicate_throughput_monotonic_seconds();
        bc_core_sha256(buffer, BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES, digest);
        double end_seconds = bc_duplicate_throughput_monotonic_seconds();
        samples[iteration_index] = end_seconds - start_seconds;
        bc_duplicate_throughput_benchmark_sink += digest[0];
    }
    double median_seconds = bc_duplicate_throughput_median_seconds(samples, BC_DUPLICATE_THROUGHPUT_HASH_ITERATION_COUNT);
    double bytes_per_second = (double)BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES / median_seconds;
    return bytes_per_second / 1e9;
}

static double bc_duplicate_throughput_measure_memory_bandwidth(uint8_t* buffer)
{
    size_t half_size = BC_DUPLICATE_THROUGHPUT_BUFFER_BYTES / 2;
    const uint8_t* source_region = buffer;
    uint8_t* destination_region = buffer + half_size;

    bc_core_copy(destination_region, source_region, half_size);
    bc_duplicate_throughput_benchmark_sink += destination_region[0];

    double samples[BC_DUPLICATE_THROUGHPUT_MEMORY_ITERATION_COUNT];
    for (size_t iteration_index = 0; iteration_index < BC_DUPLICATE_THROUGHPUT_MEMORY_ITERATION_COUNT; ++iteration_index) {
        double start_seconds = bc_duplicate_throughput_monotonic_seconds();
        bc_core_copy(destination_region, source_region, half_size);
        double end_seconds = bc_duplicate_throughput_monotonic_seconds();
        samples[iteration_index] = end_seconds - start_seconds;
        bc_duplicate_throughput_benchmark_sink += destination_region[0];
    }
    double median_seconds = bc_duplicate_throughput_median_seconds(samples, BC_DUPLICATE_THROUGHPUT_MEMORY_ITERATION_COUNT);
    double bytes_per_second = (double)half_size * 2.0 / median_seconds;
    return bytes_per_second / 1e9;
}

static void bc_duplicate_throughput_noop_task(void* task_argument)
{
    (void)task_argument;
}

static double bc_duplicate_throughput_measure_parallel_startup_overhead(bc_concurrency_context_t* concurrency_context)
{
    size_t effective_worker_count = bc_concurrency_effective_worker_count(concurrency_context);
    if (effective_worker_count < 2) {
        return 0.0;
    }
    size_t task_count = effective_worker_count;

    double cold_start_seconds = bc_duplicate_throughput_monotonic_seconds();
    for (size_t task_index = 0; task_index < task_count; ++task_index) {
        bc_concurrency_submit(concurrency_context, bc_duplicate_throughput_noop_task, NULL);
    }
    bc_concurrency_dispatch_and_wait(concurrency_context);
    double cold_end_seconds = bc_duplicate_throughput_monotonic_seconds();
    double cold_first_dispatch_microseconds = (cold_end_seconds - cold_start_seconds) * 1e6;

    double warm_start_seconds = bc_duplicate_throughput_monotonic_seconds();
    for (size_t iteration_index = 0; iteration_index < BC_DUPLICATE_THROUGHPUT_STARTUP_ITERATION_COUNT; ++iteration_index) {
        for (size_t task_index = 0; task_index < task_count; ++task_index) {
            bc_concurrency_submit(concurrency_context, bc_duplicate_throughput_noop_task, NULL);
        }
        bc_concurrency_dispatch_and_wait(concurrency_context);
    }
    double warm_end_seconds = bc_duplicate_throughput_monotonic_seconds();
    double warm_per_iteration_microseconds = ((warm_end_seconds - warm_start_seconds) / (double)BC_DUPLICATE_THROUGHPUT_STARTUP_ITERATION_COUNT) * 1e6;

    return cold_first_dispatch_microseconds > warm_per_iteration_microseconds ? cold_first_dispatch_microseconds
                                                                              : warm_per_iteration_microseconds;
}

static double bc_duplicate_throughput_measure_per_file_cost_warm(void)
{
    char absolute_path[] = "/tmp/bc_duplicate_throughput_probe_XXXXXX";
    int probe_fd = mkstemp(absolute_path);
    if (probe_fd < 0) {
        return 0.0;
    }
    char probe_payload[BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE];
    for (size_t offset = 0; offset < BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE; ++offset) {
        probe_payload[offset] = (char)(offset & 0xFFU);
    }
    ssize_t written = write(probe_fd, probe_payload, BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE);
    close(probe_fd);
    if (written < 0 || (size_t)written != BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE) {
        unlink(absolute_path);
        return 0.0;
    }

    uint8_t read_buffer[BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE];
    int warmup_fd = open(absolute_path, O_RDONLY);
    if (warmup_fd >= 0) {
        ssize_t warmup_bytes_read = read(warmup_fd, read_buffer, BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE);
        (void)warmup_bytes_read;
        close(warmup_fd);
    }

    double start_seconds = bc_duplicate_throughput_monotonic_seconds();
    for (size_t iteration_index = 0; iteration_index < BC_DUPLICATE_THROUGHPUT_PER_FILE_ITERATION_COUNT; ++iteration_index) {
        int iteration_fd = open(absolute_path, O_RDONLY);
        if (iteration_fd < 0) {
            continue;
        }
        ssize_t bytes_read = read(iteration_fd, read_buffer, BC_DUPLICATE_THROUGHPUT_PER_FILE_PROBE_SIZE);
        close(iteration_fd);
        if (bytes_read > 0) {
            XXH64_hash_t digest = XXH3_64bits(read_buffer, (size_t)bytes_read);
            bc_duplicate_throughput_benchmark_sink += digest;
        }
    }
    double end_seconds = bc_duplicate_throughput_monotonic_seconds();
    double total_seconds = end_seconds - start_seconds;

    unlink(absolute_path);

    double per_iteration_seconds = total_seconds / (double)BC_DUPLICATE_THROUGHPUT_PER_FILE_ITERATION_COUNT;
    return per_iteration_seconds * 1e6;
}

bool bc_duplicate_throughput_measure(bc_concurrency_context_t* concurrency_context, bc_duplicate_throughput_constants_t* out_constants)
{
    uint8_t* buffer = NULL;
    if (!bc_duplicate_throughput_allocate_buffer(&buffer)) {
        return false;
    }

    out_constants->xxh3_gigabytes_per_second = bc_duplicate_throughput_measure_xxh3(buffer);
    out_constants->xxh128_gigabytes_per_second = bc_duplicate_throughput_measure_xxh128(buffer);
    out_constants->sha256_gigabytes_per_second = bc_duplicate_throughput_measure_sha256(buffer);
    out_constants->memory_bandwidth_gigabytes_per_second = bc_duplicate_throughput_measure_memory_bandwidth(buffer);

    free(buffer);

    out_constants->parallel_startup_overhead_microseconds = bc_duplicate_throughput_measure_parallel_startup_overhead(concurrency_context);
    out_constants->per_file_cost_warm_microseconds = bc_duplicate_throughput_measure_per_file_cost_warm();

    return true;
}
