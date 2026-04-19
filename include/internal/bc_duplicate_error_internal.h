// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_ERROR_INTERNAL_H
#define BC_DUPLICATE_ERROR_INTERNAL_H

#include "bc_allocators_context.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct bc_duplicate_error_collector bc_duplicate_error_collector_t;

bool bc_duplicate_error_collector_create(bc_allocators_context_t* memory_context, bc_duplicate_error_collector_t** out_collector);

void bc_duplicate_error_collector_destroy(bc_allocators_context_t* memory_context, bc_duplicate_error_collector_t* collector);

bool bc_duplicate_error_collector_record(bc_duplicate_error_collector_t* collector, bc_allocators_context_t* memory_context, const char* path,
                                         const char* stage, int errno_value);

bool bc_duplicate_error_collector_flush_to_stderr(const bc_duplicate_error_collector_t* collector);

size_t bc_duplicate_error_collector_count(const bc_duplicate_error_collector_t* collector);

#endif /* BC_DUPLICATE_ERROR_INTERNAL_H */
