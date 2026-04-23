// SPDX-License-Identifier: MIT

#include "bc_duplicate_worker_sort_internal.h"
#include "bc_duplicate_types_internal.h"

#include <stdlib.h>

typedef struct {
    const bc_duplicate_file_entry_t* entries;
} bc_duplicate_sort_context_t;

/* cppcheck-suppress constParameterCallback; signature fixed by qsort_r */
static int bc_duplicate_sort_indices_compare(const void* left, const void* right, void* user_data)
{
    const bc_duplicate_sort_context_t* context = (const bc_duplicate_sort_context_t*)user_data;
    const size_t left_index = *(const size_t*)left;
    const size_t right_index = *(const size_t*)right;
    const size_t left_size = context->entries[left_index].file_size;
    const size_t right_size = context->entries[right_index].file_size;
    if (left_size > right_size) {
        return -1;
    }
    if (left_size < right_size) {
        return 1;
    }
    return 0;
}

void bc_duplicate_worker_sort_indices_by_size_desc(const bc_duplicate_file_entry_t* entries,
                                                   size_t* candidate_indices,
                                                   size_t candidate_count)
{
    bc_duplicate_sort_context_t context = {.entries = entries};
    qsort_r(candidate_indices, candidate_count, sizeof(size_t),
            bc_duplicate_sort_indices_compare, &context);
}
