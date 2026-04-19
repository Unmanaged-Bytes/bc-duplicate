// SPDX-License-Identifier: MIT

#include "bc_duplicate_grouping_internal.h"

#include "bc_allocators_pool.h"
#include "bc_core.h"

#include <stdlib.h>

typedef struct bc_duplicate_full_hash_compare_context {
    size_t digest_size;
} bc_duplicate_full_hash_compare_context_t;

/* cppcheck-suppress constParameterCallback; signature fixed by qsort_r */
static int bc_duplicate_grouping_full_hash_compare_r(const void* lhs, const void* rhs, void* user)
{
    const bc_duplicate_file_entry_t* left = (const bc_duplicate_file_entry_t*)lhs;
    const bc_duplicate_file_entry_t* right = (const bc_duplicate_file_entry_t*)rhs;
    const bc_duplicate_full_hash_compare_context_t* context = (const bc_duplicate_full_hash_compare_context_t*)user;
    int result = 0;
    bc_core_compare(left->full_hash, right->full_hash, context->digest_size, &result);
    return result;
}

bool bc_duplicate_grouping_by_full_hash(bc_allocators_context_t* memory_context, bc_duplicate_file_entry_t* entries,
                                        const bc_duplicate_group_t* fast_hash_groups, size_t fast_hash_group_count, size_t digest_size,
                                        bc_duplicate_group_t** out_groups, size_t* out_group_count)
{
    *out_groups = NULL;
    *out_group_count = 0;

    if (fast_hash_group_count == 0) {
        return true;
    }

    size_t group_capacity = fast_hash_group_count;
    bc_duplicate_group_t* groups = NULL;
    if (!bc_allocators_pool_allocate(memory_context, group_capacity * sizeof(bc_duplicate_group_t), (void**)&groups)) {
        return false;
    }
    size_t group_count = 0;

    bc_duplicate_full_hash_compare_context_t compare_context = {.digest_size = digest_size};

    for (size_t fast_group_index = 0; fast_group_index < fast_hash_group_count; ++fast_group_index) {
        const bc_duplicate_group_t* fast_group = &fast_hash_groups[fast_group_index];
        bc_duplicate_file_entry_t* run_begin = &entries[fast_group->start_index];
        size_t run_length = fast_group->entry_count;

        qsort_r(run_begin, run_length, sizeof(bc_duplicate_file_entry_t), bc_duplicate_grouping_full_hash_compare_r, &compare_context);

        size_t cursor = 0;
        while (cursor < run_length) {
            size_t sub_start = cursor;
            cursor++;
            for (;;) {
                if (cursor >= run_length) {
                    break;
                }
                bool same_digest = false;
                bc_core_equal(run_begin[cursor].full_hash, run_begin[sub_start].full_hash, digest_size, &same_digest);
                if (!same_digest) {
                    break;
                }
                cursor++;
            }
            size_t sub_length = cursor - sub_start;
            if (sub_length < 2) {
                continue;
            }
            if (group_count == group_capacity) {
                size_t new_capacity = group_capacity * 2;
                void* new_buffer = NULL;
                if (!bc_allocators_pool_reallocate(memory_context, groups, new_capacity * sizeof(bc_duplicate_group_t), &new_buffer)) {
                    bc_allocators_pool_free(memory_context, groups);
                    return false;
                }
                groups = (bc_duplicate_group_t*)new_buffer;
                group_capacity = new_capacity;
            }
            groups[group_count].start_index = fast_group->start_index + sub_start;
            groups[group_count].entry_count = sub_length;
            groups[group_count].file_size = fast_group->file_size;
            group_count++;
        }
    }

    *out_groups = groups;
    *out_group_count = group_count;
    return true;
}
