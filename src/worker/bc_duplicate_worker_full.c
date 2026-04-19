// SPDX-License-Identifier: MIT

#include "bc_duplicate_worker_internal.h"

#include "bc_allocators_pool.h"
#include "bc_concurrency.h"
#include "bc_core.h"
#include "bc_core_hash.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <xxhash.h>

#define BC_DUPLICATE_FULL_HASH_CHUNK_SIZE ((size_t)(64 * 1024))

typedef struct bc_duplicate_worker_full_context {
    bc_duplicate_file_entry_t* entries;
    const size_t* candidate_indices;
    size_t candidate_count;
    bc_concurrency_signal_handler_t* signal_handler;
    bc_duplicate_algorithm_t algorithm;
} bc_duplicate_worker_full_context_t;

size_t bc_duplicate_worker_digest_size(bc_duplicate_algorithm_t algorithm)
{
    switch (algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3:
        return BC_DUPLICATE_XXH3_DIGEST_SIZE;
    case BC_DUPLICATE_ALGORITHM_XXH128:
        return BC_DUPLICATE_XXH128_DIGEST_SIZE;
    case BC_DUPLICATE_ALGORITHM_SHA256:
        return BC_DUPLICATE_SHA256_DIGEST_SIZE;
    }
    return 0;
}

static bool bc_duplicate_worker_full_should_stop(const bc_duplicate_worker_full_context_t* context)
{
    if (context->signal_handler == NULL) {
        return false;
    }
    bool should_stop = false;
    bc_concurrency_signal_handler_should_stop(context->signal_handler, &should_stop);
    return should_stop;
}

static bool bc_duplicate_worker_full_hash_xxh3(int file_descriptor, uint8_t* buffer, uint8_t out_digest[BC_DUPLICATE_MAX_DIGEST_SIZE],
                                               int* out_errno)
{
    XXH3_state_t* state = XXH3_createState();
    if (state == NULL) {
        *out_errno = ENOMEM;
        return false;
    }
    XXH3_64bits_reset(state);
    for (;;) {
        ssize_t bytes_read = read(file_descriptor, buffer, BC_DUPLICATE_FULL_HASH_CHUNK_SIZE);
        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            *out_errno = errno;
            XXH3_freeState(state);
            return false;
        }
        XXH3_64bits_update(state, buffer, (size_t)bytes_read);
    }
    XXH64_hash_t digest = XXH3_64bits_digest(state);
    XXH3_freeState(state);
    bc_core_zero(out_digest, BC_DUPLICATE_MAX_DIGEST_SIZE);
    bc_core_copy(out_digest, &digest, sizeof(digest));
    *out_errno = 0;
    return true;
}

static bool bc_duplicate_worker_full_hash_xxh128(int file_descriptor, uint8_t* buffer, uint8_t out_digest[BC_DUPLICATE_MAX_DIGEST_SIZE],
                                                 int* out_errno)
{
    XXH3_state_t* state = XXH3_createState();
    if (state == NULL) {
        *out_errno = ENOMEM;
        return false;
    }
    XXH3_128bits_reset(state);
    for (;;) {
        ssize_t bytes_read = read(file_descriptor, buffer, BC_DUPLICATE_FULL_HASH_CHUNK_SIZE);
        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            *out_errno = errno;
            XXH3_freeState(state);
            return false;
        }
        XXH3_128bits_update(state, buffer, (size_t)bytes_read);
    }
    XXH128_hash_t digest = XXH3_128bits_digest(state);
    XXH3_freeState(state);
    bc_core_zero(out_digest, BC_DUPLICATE_MAX_DIGEST_SIZE);
    bc_core_copy(out_digest, &digest.high64, sizeof(digest.high64));
    bc_core_copy(out_digest + sizeof(digest.high64), &digest.low64, sizeof(digest.low64));
    *out_errno = 0;
    return true;
}

static bool bc_duplicate_worker_full_hash_sha256(int file_descriptor, uint8_t* buffer, uint8_t out_digest[BC_DUPLICATE_MAX_DIGEST_SIZE],
                                                 int* out_errno)
{
    bc_core_sha256_context_t context;
    if (!bc_core_sha256_init(&context)) {
        *out_errno = EINVAL;
        return false;
    }
    for (;;) {
        ssize_t bytes_read = read(file_descriptor, buffer, BC_DUPLICATE_FULL_HASH_CHUNK_SIZE);
        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            *out_errno = errno;
            return false;
        }
        if (!bc_core_sha256_update(&context, buffer, (size_t)bytes_read)) {
            *out_errno = EINVAL;
            return false;
        }
    }
    bc_core_zero(out_digest, BC_DUPLICATE_MAX_DIGEST_SIZE);
    if (!bc_core_sha256_finalize(&context, out_digest)) {
        *out_errno = EINVAL;
        return false;
    }
    *out_errno = 0;
    return true;
}

static void bc_duplicate_worker_full_iteration(size_t iteration_index, void* user_argument)
{
    bc_duplicate_worker_full_context_t* context = (bc_duplicate_worker_full_context_t*)user_argument;
    if (bc_duplicate_worker_full_should_stop(context)) {
        return;
    }
    size_t entry_index = context->candidate_indices[iteration_index];
    bc_duplicate_file_entry_t* entry = &context->entries[entry_index];

    int file_descriptor = open(entry->absolute_path, O_RDONLY | O_NOATIME | O_CLOEXEC);
    if (file_descriptor < 0 && errno == EPERM) {
        file_descriptor = open(entry->absolute_path, O_RDONLY | O_CLOEXEC);
    }
    if (file_descriptor < 0) {
        entry->full_hash_computed = true;
        entry->full_hash_errno = errno;
        return;
    }

    bc_allocators_context_t* worker_memory = bc_concurrency_worker_memory();
    uint8_t stack_buffer[BC_DUPLICATE_FULL_HASH_CHUNK_SIZE];
    uint8_t* buffer = stack_buffer;
    void* heap_buffer = NULL;
    if (worker_memory != NULL) {
        if (bc_allocators_pool_allocate(worker_memory, BC_DUPLICATE_FULL_HASH_CHUNK_SIZE, &heap_buffer)) {
            buffer = (uint8_t*)heap_buffer;
        }
    }

    int operation_errno = 0;
    bool ok = false;
    switch (context->algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3:
        ok = bc_duplicate_worker_full_hash_xxh3(file_descriptor, buffer, entry->full_hash, &operation_errno);
        break;
    case BC_DUPLICATE_ALGORITHM_XXH128:
        ok = bc_duplicate_worker_full_hash_xxh128(file_descriptor, buffer, entry->full_hash, &operation_errno);
        break;
    case BC_DUPLICATE_ALGORITHM_SHA256:
        ok = bc_duplicate_worker_full_hash_sha256(file_descriptor, buffer, entry->full_hash, &operation_errno);
        break;
    }

    close(file_descriptor);
    if (heap_buffer != NULL) {
        bc_allocators_pool_free(worker_memory, heap_buffer);
    }

    entry->full_hash_computed = true;
    entry->full_hash_errno = ok ? 0 : (operation_errno != 0 ? operation_errno : EIO);
}

bool bc_duplicate_worker_full_pass(bc_allocators_context_t* memory_context, bc_concurrency_context_t* concurrency_context,
                                   bc_concurrency_signal_handler_t* signal_handler, bc_duplicate_algorithm_t algorithm,
                                   bc_duplicate_file_entry_t* entries, const bc_duplicate_group_t* candidate_groups,
                                   size_t candidate_group_count, size_t* out_files_hashed)
{
    *out_files_hashed = 0;

    size_t total_candidates = 0;
    for (size_t group_index = 0; group_index < candidate_group_count; ++group_index) {
        total_candidates += candidate_groups[group_index].entry_count;
    }
    if (total_candidates == 0) {
        return true;
    }

    size_t* candidate_indices = NULL;
    if (!bc_allocators_pool_allocate(memory_context, total_candidates * sizeof(size_t), (void**)&candidate_indices)) {
        return false;
    }

    size_t cursor = 0;
    for (size_t group_index = 0; group_index < candidate_group_count; ++group_index) {
        const bc_duplicate_group_t* group = &candidate_groups[group_index];
        for (size_t offset = 0; offset < group->entry_count; ++offset) {
            candidate_indices[cursor++] = group->start_index + offset;
        }
    }

    bc_duplicate_worker_full_context_t context = {
        .entries = entries,
        .candidate_indices = candidate_indices,
        .candidate_count = total_candidates,
        .signal_handler = signal_handler,
        .algorithm = algorithm,
    };

    bool dispatch_ok = bc_concurrency_for(concurrency_context, 0, total_candidates, 1, bc_duplicate_worker_full_iteration, &context);

    bc_allocators_pool_free(memory_context, candidate_indices);
    if (!dispatch_ok) {
        return false;
    }

    *out_files_hashed = total_candidates;
    return true;
}
