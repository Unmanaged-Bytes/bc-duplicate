// SPDX-License-Identifier: MIT

#include "bc_duplicate_dispatch_decision_internal.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

static double bc_duplicate_dispatch_decision_algorithm_throughput(bc_duplicate_algorithm_t algorithm,
                                                                  const bc_duplicate_throughput_constants_t* constants)
{
    switch (algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3:
        return constants->xxh3_gigabytes_per_second;
    case BC_DUPLICATE_ALGORITHM_XXH128:
        return constants->xxh128_gigabytes_per_second;
    case BC_DUPLICATE_ALGORITHM_SHA256:
        return constants->sha256_gigabytes_per_second;
    }
    return 0.0;
}

bool bc_duplicate_dispatch_decision_should_go_multithread(size_t file_count, size_t total_bytes, bc_duplicate_algorithm_t algorithm,
                                                          const bc_duplicate_throughput_constants_t* throughput_constants, size_t worker_count)
{
    if (worker_count <= 1) {
        return false;
    }
    if (file_count == 0) {
        return false;
    }

    double gigabytes_per_second = bc_duplicate_dispatch_decision_algorithm_throughput(algorithm, throughput_constants);
    if (gigabytes_per_second <= 0.0) {
        return true;
    }
    double bytes_per_microsecond = gigabytes_per_second * 1e9 / 1e6;
    double per_file_cost_microseconds = throughput_constants->per_file_cost_warm_microseconds;
    double parallel_startup_overhead_microseconds = throughput_constants->parallel_startup_overhead_microseconds;

    double mono_wall_microseconds = (double)file_count * per_file_cost_microseconds + (double)total_bytes / bytes_per_microsecond;
    double multi_wall_microseconds = parallel_startup_overhead_microseconds + mono_wall_microseconds / (double)worker_count;

    return multi_wall_microseconds < mono_wall_microseconds;
}

size_t bc_duplicate_dispatch_decision_optimal_worker_count(double single_thread_gigabytes_per_second,
                                                           double memory_bandwidth_gigabytes_per_second, size_t physical_core_count)
{
    size_t compute_bound = physical_core_count > 1 ? physical_core_count - 1 : 1;
    if (single_thread_gigabytes_per_second <= 0.0 || memory_bandwidth_gigabytes_per_second <= 0.0) {
        return compute_bound;
    }
    double memory_bound_fractional = memory_bandwidth_gigabytes_per_second / single_thread_gigabytes_per_second;
    size_t memory_bound = (size_t)ceil(memory_bound_fractional);
    if (memory_bound < 1) {
        memory_bound = 1;
    }
    return memory_bound < compute_bound ? memory_bound : compute_bound;
}
