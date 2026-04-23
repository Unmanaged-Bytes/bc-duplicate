// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_WORKER_SORT_INTERNAL_H
#define BC_DUPLICATE_WORKER_SORT_INTERNAL_H

#include "bc_duplicate_types_internal.h"

#include <stddef.h>

void bc_duplicate_worker_sort_indices_by_size_desc(const bc_duplicate_file_entry_t* entries,
                                                   size_t* candidate_indices,
                                                   size_t candidate_count);

#endif /* BC_DUPLICATE_WORKER_SORT_INTERNAL_H */
