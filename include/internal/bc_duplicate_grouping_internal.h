// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_GROUPING_INTERNAL_H
#define BC_DUPLICATE_GROUPING_INTERNAL_H

#include "bc_duplicate_types_internal.h"

#include "bc_allocators.h"

#include <stdbool.h>
#include <stddef.h>

bool bc_duplicate_grouping_by_size(bc_allocators_context_t* memory_context, bc_duplicate_file_entry_t* entries, size_t entry_count,
                                   bool match_hardlinks, bc_duplicate_group_t** out_groups, size_t* out_group_count,
                                   size_t* out_candidate_count, size_t* out_hardlinks_collapsed);

bool bc_duplicate_grouping_by_fast_hash(bc_allocators_context_t* memory_context, bc_duplicate_file_entry_t* entries,
                                        const bc_duplicate_group_t* size_groups, size_t size_group_count,
                                        bc_duplicate_group_t** out_groups, size_t* out_group_count, size_t* out_candidate_count);

bool bc_duplicate_grouping_by_full_hash(bc_allocators_context_t* memory_context, bc_duplicate_file_entry_t* entries,
                                        const bc_duplicate_group_t* fast_hash_groups, size_t fast_hash_group_count, size_t digest_size,
                                        bc_duplicate_group_t** out_groups, size_t* out_group_count);

#endif /* BC_DUPLICATE_GROUPING_INTERNAL_H */
