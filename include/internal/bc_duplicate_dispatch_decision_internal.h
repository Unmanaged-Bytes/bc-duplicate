// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_DISPATCH_DECISION_INTERNAL_H
#define BC_DUPLICATE_DISPATCH_DECISION_INTERNAL_H

#include "bc_duplicate_throughput_internal.h"
#include "bc_duplicate_types_internal.h"

#include <stdbool.h>
#include <stddef.h>

bool bc_duplicate_dispatch_decision_should_go_multithread(size_t file_count, size_t total_bytes, bc_duplicate_algorithm_t algorithm,
                                                          const bc_duplicate_throughput_constants_t* throughput_constants, size_t worker_count);

size_t bc_duplicate_dispatch_decision_optimal_worker_count(double single_thread_gigabytes_per_second,
                                                           double memory_bandwidth_gigabytes_per_second, size_t physical_core_count);

#endif /* BC_DUPLICATE_DISPATCH_DECISION_INTERNAL_H */
