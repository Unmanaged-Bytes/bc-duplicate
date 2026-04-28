// SPDX-License-Identifier: MIT

#include "bc_duplicate_grouping_internal.h"

#include "bc_allocators_pool.h"
#include "bc_core_sort.h"

static bool bc_duplicate_grouping_fast_hash_less_than(const void* lhs, const void* rhs, void* user_data)
{
    (void)user_data;
    const bc_duplicate_file_entry_t* left = (const bc_duplicate_file_entry_t*)lhs;
    const bc_duplicate_file_entry_t* right = (const bc_duplicate_file_entry_t*)rhs;
    return left->fast_hash < right->fast_hash;
}

bool bc_duplicate_grouping_by_fast_hash(bc_allocators_context_t* memory_context, bc_duplicate_file_entry_t* entries,
                                        const bc_duplicate_group_t* size_groups, size_t size_group_count,
                                        bc_duplicate_group_t** out_groups, size_t* out_group_count, size_t* out_candidate_count)
{
    *out_groups = NULL;
    *out_group_count = 0;
    *out_candidate_count = 0;

    if (size_group_count == 0) {
        return true;
    }

    size_t group_capacity = size_group_count;
    bc_duplicate_group_t* groups = NULL;
    if (!bc_allocators_pool_allocate(memory_context, group_capacity * sizeof(bc_duplicate_group_t), (void**)&groups)) {
        return false;
    }
    size_t group_count = 0;

    for (size_t size_group_index = 0; size_group_index < size_group_count; ++size_group_index) {
        const bc_duplicate_group_t* size_group = &size_groups[size_group_index];
        bc_duplicate_file_entry_t* run_begin = &entries[size_group->start_index];
        size_t run_length = size_group->entry_count;

        bc_core_sort_with_compare(run_begin, run_length, sizeof(bc_duplicate_file_entry_t), bc_duplicate_grouping_fast_hash_less_than, NULL);

        size_t cursor = 0;
        while (cursor < run_length) {
            size_t sub_start = cursor;
            uint64_t current_hash = run_begin[cursor].fast_hash;
            cursor++;
            while (cursor < run_length && run_begin[cursor].fast_hash == current_hash) {
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
            groups[group_count].start_index = size_group->start_index + sub_start;
            groups[group_count].entry_count = sub_length;
            groups[group_count].file_size = size_group->file_size;
            group_count++;
            *out_candidate_count += sub_length;
        }
    }

    *out_groups = groups;
    *out_group_count = group_count;
    return true;
}
