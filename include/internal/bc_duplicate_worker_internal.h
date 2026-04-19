// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_WORKER_INTERNAL_H
#define BC_DUPLICATE_WORKER_INTERNAL_H

#include "bc_duplicate_types_internal.h"

#include "bc_allocators.h"
#include "bc_concurrency.h"
#include "bc_concurrency_signal.h"

#include <stdbool.h>
#include <stddef.h>

bool bc_duplicate_worker_fast_pass(bc_allocators_context_t* memory_context, bc_concurrency_context_t* concurrency_context,
                                   bc_concurrency_signal_handler_t* signal_handler, bc_duplicate_file_entry_t* entries,
                                   const bc_duplicate_group_t* size_groups, size_t size_group_count, size_t* out_files_hashed);

#endif /* BC_DUPLICATE_WORKER_INTERNAL_H */
