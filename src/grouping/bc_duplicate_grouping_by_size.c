// SPDX-License-Identifier: MIT

#include "bc_duplicate_grouping_internal.h"

#include "bc_allocators_pool.h"
#include "bc_core.h"

#include <stdlib.h>

static int bc_duplicate_grouping_size_dev_ino_compare(const void* lhs, const void* rhs)
{
    const bc_duplicate_file_entry_t* left = (const bc_duplicate_file_entry_t*)lhs;
    const bc_duplicate_file_entry_t* right = (const bc_duplicate_file_entry_t*)rhs;
    if (left->file_size > right->file_size) {
        return -1;
    }
    if (left->file_size < right->file_size) {
        return 1;
    }
    if (left->device_id < right->device_id) {
        return -1;
    }
    if (left->device_id > right->device_id) {
        return 1;
    }
    if (left->inode_number < right->inode_number) {
        return -1;
    }
    if (left->inode_number > right->inode_number) {
        return 1;
    }
    return 0;
}

static size_t bc_duplicate_grouping_collapse_hardlinks_in_run(bc_duplicate_file_entry_t* entries, size_t run_start_index, size_t run_length)
{
    if (run_length <= 1) {
        return 0;
    }
    size_t write_index = run_start_index + 1;
    size_t collapsed = 0;
    for (size_t read_index = run_start_index + 1; read_index < run_start_index + run_length; ++read_index) {
        if (entries[read_index].device_id == entries[write_index - 1].device_id &&
            entries[read_index].inode_number == entries[write_index - 1].inode_number) {
            collapsed++;
            continue;
        }
        if (write_index != read_index) {
            entries[write_index] = entries[read_index];
        }
        write_index++;
    }
    return collapsed;
}

bool bc_duplicate_grouping_by_size(bc_allocators_context_t* memory_context, bc_duplicate_file_entry_t* entries, size_t entry_count,
                                   bool match_hardlinks, bc_duplicate_group_t** out_groups, size_t* out_group_count,
                                   size_t* out_candidate_count, size_t* out_hardlinks_collapsed)
{
    *out_groups = NULL;
    *out_group_count = 0;
    *out_candidate_count = 0;
    *out_hardlinks_collapsed = 0;

    if (entry_count == 0) {
        return true;
    }

    qsort(entries, entry_count, sizeof(bc_duplicate_file_entry_t), bc_duplicate_grouping_size_dev_ino_compare);

    if (!match_hardlinks) {
        size_t write_index = 0;
        size_t run_start_index = 0;
        while (run_start_index < entry_count) {
            size_t run_length = 1;
            while (run_start_index + run_length < entry_count && entries[run_start_index + run_length].file_size == entries[run_start_index].file_size) {
                run_length++;
            }
            if (write_index != run_start_index) {
                bc_core_copy(&entries[write_index], &entries[run_start_index], run_length * sizeof(bc_duplicate_file_entry_t));
            }
            size_t collapsed = bc_duplicate_grouping_collapse_hardlinks_in_run(entries, write_index, run_length);
            *out_hardlinks_collapsed += collapsed;
            write_index += run_length - collapsed;
            run_start_index += run_length;
        }
        entry_count = write_index;
    }

    size_t group_capacity = 16;
    size_t group_count = 0;
    bc_duplicate_group_t* groups = NULL;
    if (!bc_allocators_pool_allocate(memory_context, group_capacity * sizeof(bc_duplicate_group_t), (void**)&groups)) {
        return false;
    }

    size_t cursor = 0;
    while (cursor < entry_count) {
        size_t run_start = cursor;
        size_t group_size = entries[cursor].file_size;
        cursor++;
        while (cursor < entry_count && entries[cursor].file_size == group_size) {
            cursor++;
        }
        size_t run_length = cursor - run_start;
        if (run_length < 2) {
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

        groups[group_count].start_index = run_start;
        groups[group_count].entry_count = run_length;
        groups[group_count].file_size = group_size;
        group_count++;
        *out_candidate_count += run_length;
    }

    *out_groups = groups;
    *out_group_count = group_count;
    return true;
}
