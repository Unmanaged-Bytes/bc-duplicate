// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_DISCOVERY_INTERNAL_H
#define BC_DUPLICATE_DISCOVERY_INTERNAL_H

#include "bc_duplicate_error_internal.h"
#include "bc_duplicate_filter_internal.h"
#include "bc_duplicate_types_internal.h"

#include "bc_allocators_context.h"
#include "bc_concurrency.h"
#include "bc_concurrency_signal.h"
#include "bc_containers_vector.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct bc_duplicate_discovery_options {
    bool include_hidden;
    bool follow_symlinks;
    bool one_file_system;
    size_t minimum_file_size;
} bc_duplicate_discovery_options_t;

bool bc_duplicate_discovery_glob_contains_metacharacter(const char* pattern, bool* out_contains);

bool bc_duplicate_discovery_path_is_pseudo_filesystem(const char* path, bool* out_is_pseudo);

bool bc_duplicate_discovery_expand(bc_allocators_context_t* memory_context, bc_containers_vector_t* entries,
                                   bc_duplicate_error_collector_t* errors, bc_concurrency_signal_handler_t* signal_handler,
                                   const bc_duplicate_filter_t* filter, const bc_duplicate_discovery_options_t* options,
                                   const char* const* input_paths, size_t input_count);

bool bc_duplicate_discovery_expand_parallel(bc_allocators_context_t* memory_context, bc_concurrency_context_t* concurrency_context,
                                            bc_containers_vector_t* entries, bc_duplicate_error_collector_t* errors,
                                            bc_concurrency_signal_handler_t* signal_handler, const bc_duplicate_filter_t* filter,
                                            const bc_duplicate_discovery_options_t* options, const char* const* input_paths,
                                            size_t input_count);

#endif /* BC_DUPLICATE_DISCOVERY_INTERNAL_H */
