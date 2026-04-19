// SPDX-License-Identifier: MIT

#include "bc_duplicate_worker_internal.h"

#include "bc_allocators_pool.h"
#include "bc_concurrency.h"
#include "bc_core.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <xxhash.h>

#define BC_DUPLICATE_FAST_DUAL_MIN_FILE_SIZE (BC_DUPLICATE_FAST_HASH_BLOCK_SIZE * 2)

typedef struct bc_duplicate_worker_fast_context {
    bc_duplicate_file_entry_t* entries;
    const size_t* candidate_indices;
    size_t candidate_count;
    bc_concurrency_signal_handler_t* signal_handler;
} bc_duplicate_worker_fast_context_t;

static bool bc_duplicate_worker_fast_should_stop(const bc_duplicate_worker_fast_context_t* context)
{
    if (context->signal_handler == NULL) {
        return false;
    }
    bool should_stop = false;
    bc_concurrency_signal_handler_should_stop(context->signal_handler, &should_stop);
    return should_stop;
}

static void bc_duplicate_worker_fast_iteration(size_t iteration_index, void* user_argument)
{
    bc_duplicate_worker_fast_context_t* context = (bc_duplicate_worker_fast_context_t*)user_argument;
    if (bc_duplicate_worker_fast_should_stop(context)) {
        return;
    }
    size_t entry_index = context->candidate_indices[iteration_index];
    bc_duplicate_file_entry_t* entry = &context->entries[entry_index];

    int file_descriptor = open(entry->absolute_path, O_RDONLY | O_NOATIME | O_CLOEXEC);
    if (file_descriptor < 0 && errno == EPERM) {
        file_descriptor = open(entry->absolute_path, O_RDONLY | O_CLOEXEC);
    }
    if (file_descriptor < 0) {
        entry->fast_hash = 0;
        entry->fast_hash_computed = true;
        entry->fast_hash_errno = errno;
        return;
    }

    uint8_t prefix_buffer[BC_DUPLICATE_FAST_HASH_BLOCK_SIZE];
    const ssize_t prefix_bytes_read = pread(file_descriptor, prefix_buffer, BC_DUPLICATE_FAST_HASH_BLOCK_SIZE, 0);
    if (prefix_bytes_read < 0) {
        const int prefix_errno = errno;
        close(file_descriptor);
        entry->fast_hash = 0;
        entry->fast_hash_computed = true;
        entry->fast_hash_errno = prefix_errno;
        return;
    }

    uint64_t combined_hash = XXH3_64bits(prefix_buffer, (size_t)prefix_bytes_read);

    if (entry->file_size >= BC_DUPLICATE_FAST_DUAL_MIN_FILE_SIZE) {
        uint8_t suffix_buffer[BC_DUPLICATE_FAST_HASH_BLOCK_SIZE];
        const off_t suffix_offset = (off_t)(entry->file_size - BC_DUPLICATE_FAST_HASH_BLOCK_SIZE);
        const ssize_t suffix_bytes_read = pread(file_descriptor, suffix_buffer, BC_DUPLICATE_FAST_HASH_BLOCK_SIZE, suffix_offset);
        if (suffix_bytes_read > 0) {
            combined_hash ^= XXH3_64bits(suffix_buffer, (size_t)suffix_bytes_read);
        }
    }

    close(file_descriptor);

    entry->fast_hash = combined_hash;
    entry->fast_hash_computed = true;
    entry->fast_hash_errno = 0;
}

bool bc_duplicate_worker_fast_pass(bc_allocators_context_t* memory_context, bc_concurrency_context_t* concurrency_context,
                                   bc_concurrency_signal_handler_t* signal_handler, bc_duplicate_file_entry_t* entries,
                                   const bc_duplicate_group_t* size_groups, size_t size_group_count, size_t* out_files_hashed)
{
    *out_files_hashed = 0;

    size_t total_candidates = 0;
    for (size_t group_index = 0; group_index < size_group_count; ++group_index) {
        total_candidates += size_groups[group_index].entry_count;
    }
    if (total_candidates == 0) {
        return true;
    }

    size_t* candidate_indices = NULL;
    if (!bc_allocators_pool_allocate(memory_context, total_candidates * sizeof(size_t), (void**)&candidate_indices)) {
        return false;
    }

    size_t cursor = 0;
    for (size_t group_index = 0; group_index < size_group_count; ++group_index) {
        const bc_duplicate_group_t* group = &size_groups[group_index];
        for (size_t offset = 0; offset < group->entry_count; ++offset) {
            candidate_indices[cursor++] = group->start_index + offset;
        }
    }

    bc_duplicate_worker_fast_context_t context = {
        .entries = entries,
        .candidate_indices = candidate_indices,
        .candidate_count = total_candidates,
        .signal_handler = signal_handler,
    };

    bool dispatch_ok =
        bc_concurrency_for(concurrency_context, 0, total_candidates, 1, bc_duplicate_worker_fast_iteration, &context);

    bc_allocators_pool_free(memory_context, candidate_indices);
    if (!dispatch_ok) {
        return false;
    }

    *out_files_hashed = total_candidates;
    return true;
}
