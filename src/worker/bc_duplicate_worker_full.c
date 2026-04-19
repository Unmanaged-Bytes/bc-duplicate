// SPDX-License-Identifier: MIT

#include "bc_duplicate_reader_internal.h"
#include "bc_duplicate_worker_internal.h"

#include "bc_allocators_pool.h"
#include "bc_concurrency.h"
#include "bc_core.h"
#include "bc_core_hash.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xxhash.h>

#define BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY ((size_t)BC_DUPLICATE_READER_RING_SLOT_COUNT)

typedef struct bc_duplicate_worker_full_consumer_state {
    bc_duplicate_algorithm_t algorithm;
    XXH3_state_t* xxh_state;
    bc_core_sha256_context_t sha256_context;
    bool failed;
} bc_duplicate_worker_full_consumer_state_t;

typedef struct __attribute__((aligned(64))) bc_duplicate_worker_full_slot {
    bc_duplicate_reader_ring_t* ring;
    XXH3_state_t* xxh_states[BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY];
    bool ring_initialised;
    bool xxh_states_ready;
    bc_duplicate_reader_batch_item_t batch_items[BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY];
    bc_duplicate_worker_full_consumer_state_t consumer_states[BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY];
} bc_duplicate_worker_full_slot_t;

typedef struct bc_duplicate_worker_full_dispatch {
    bc_duplicate_file_entry_t* entries;
    const size_t* candidate_indices;
    size_t candidate_count;
    bc_concurrency_signal_handler_t* signal_handler;
    bc_duplicate_algorithm_t algorithm;
    size_t batch_size;
    size_t worker_slot_index;
    bc_allocators_context_t* main_memory_context;
} bc_duplicate_worker_full_dispatch_t;

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

static bool bc_duplicate_worker_full_should_stop(const bc_concurrency_signal_handler_t* signal_handler)
{
    if (signal_handler == NULL) {
        return false;
    }
    bool should_stop = false;
    bc_concurrency_signal_handler_should_stop(signal_handler, &should_stop);
    return should_stop;
}

static bool bc_duplicate_worker_full_consumer_reset(bc_duplicate_worker_full_consumer_state_t* state, XXH3_state_t* xxh_state,
                                                    bc_duplicate_algorithm_t algorithm)
{
    state->algorithm = algorithm;
    state->failed = false;
    state->xxh_state = xxh_state;
    switch (algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3:
        XXH3_64bits_reset(xxh_state);
        return true;
    case BC_DUPLICATE_ALGORITHM_XXH128:
        XXH3_128bits_reset(xxh_state);
        return true;
    case BC_DUPLICATE_ALGORITHM_SHA256:
        return bc_core_sha256_init(&state->sha256_context);
    }
    return false;
}

static bool bc_duplicate_worker_full_consumer_callback(void* consumer_context, const void* chunk_data, size_t chunk_size)
{
    bc_duplicate_worker_full_consumer_state_t* state = (bc_duplicate_worker_full_consumer_state_t*)consumer_context;
    switch (state->algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3:
        XXH3_64bits_update(state->xxh_state, chunk_data, chunk_size);
        return true;
    case BC_DUPLICATE_ALGORITHM_XXH128:
        XXH3_128bits_update(state->xxh_state, chunk_data, chunk_size);
        return true;
    case BC_DUPLICATE_ALGORITHM_SHA256:
        if (!bc_core_sha256_update(&state->sha256_context, chunk_data, chunk_size)) {
            state->failed = true;
            return false;
        }
        return true;
    }
    state->failed = true;
    return false;
}

static void bc_duplicate_worker_full_consumer_finalize(bc_duplicate_worker_full_consumer_state_t* state,
                                                       uint8_t out_digest[BC_DUPLICATE_MAX_DIGEST_SIZE])
{
    bc_core_zero(out_digest, BC_DUPLICATE_MAX_DIGEST_SIZE);
    switch (state->algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3: {
        const XXH64_hash_t digest = XXH3_64bits_digest(state->xxh_state);
        bc_core_copy(out_digest, &digest, sizeof(digest));
        return;
    }
    case BC_DUPLICATE_ALGORITHM_XXH128: {
        const XXH128_hash_t digest = XXH3_128bits_digest(state->xxh_state);
        bc_core_copy(out_digest, &digest.high64, sizeof(digest.high64));
        bc_core_copy(out_digest + sizeof(digest.high64), &digest.low64, sizeof(digest.low64));
        return;
    }
    case BC_DUPLICATE_ALGORITHM_SHA256:
        if (!bc_core_sha256_finalize(&state->sha256_context, out_digest)) {
            state->failed = true;
        }
        return;
    }
}

static bool bc_duplicate_worker_full_ensure_slot(const bc_duplicate_worker_full_dispatch_t* dispatch, bc_allocators_context_t* worker_memory,
                                                 bc_duplicate_worker_full_slot_t** out_slot)
{
    bc_duplicate_worker_full_slot_t* slot = (bc_duplicate_worker_full_slot_t*)bc_concurrency_worker_slot(dispatch->worker_slot_index);
    if (slot == NULL) {
        return false;
    }
    if (!slot->ring_initialised) {
        if (!bc_allocators_pool_allocate(worker_memory, bc_duplicate_reader_ring_struct_size(), (void**)&slot->ring)) {
            return false;
        }
        if (!bc_duplicate_reader_ring_init(slot->ring)) {
            bc_allocators_pool_free(worker_memory, slot->ring);
            slot->ring = NULL;
            return false;
        }
        slot->ring_initialised = true;
    }
    if (!slot->xxh_states_ready) {
        for (size_t state_index = 0; state_index < BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY; ++state_index) {
            slot->xxh_states[state_index] = XXH3_createState();
            if (slot->xxh_states[state_index] == NULL) {
                for (size_t cleanup_index = 0; cleanup_index < state_index; ++cleanup_index) {
                    XXH3_freeState(slot->xxh_states[cleanup_index]);
                    slot->xxh_states[cleanup_index] = NULL;
                }
                return false;
            }
        }
        slot->xxh_states_ready = true;
    }
    *out_slot = slot;
    return true;
}

static void bc_duplicate_worker_full_process_batch(bc_duplicate_worker_full_dispatch_t* dispatch, bc_duplicate_worker_full_slot_t* slot,
                                                   size_t batch_start, size_t batch_count)
{
    for (size_t offset = 0; offset < batch_count; ++offset) {
        const size_t entry_index = dispatch->candidate_indices[batch_start + offset];
        const bc_duplicate_file_entry_t* entry = &dispatch->entries[entry_index];
        bc_duplicate_worker_full_consumer_state_t* consumer_state = &slot->consumer_states[offset];
        bc_duplicate_worker_full_consumer_reset(consumer_state, slot->xxh_states[offset], dispatch->algorithm);
        slot->batch_items[offset].absolute_path = entry->absolute_path;
        slot->batch_items[offset].file_size = entry->file_size;
        slot->batch_items[offset].consumer_context = consumer_state;
        slot->batch_items[offset].success = false;
        slot->batch_items[offset].errno_value = 0;
    }

    bc_duplicate_reader_consume_batch(slot->ring, slot->batch_items, batch_count, bc_duplicate_worker_full_consumer_callback);

    for (size_t offset = 0; offset < batch_count; ++offset) {
        const size_t entry_index = dispatch->candidate_indices[batch_start + offset];
        bc_duplicate_file_entry_t* entry = &dispatch->entries[entry_index];
        bc_duplicate_worker_full_consumer_state_t* consumer_state = &slot->consumer_states[offset];
        const bc_duplicate_reader_batch_item_t* item = &slot->batch_items[offset];

        entry->full_hash_computed = true;
        if (!item->success || consumer_state->failed) {
            entry->full_hash_errno = item->errno_value != 0 ? item->errno_value : EIO;
            continue;
        }
        bc_duplicate_worker_full_consumer_finalize(consumer_state, entry->full_hash);
        entry->full_hash_errno = consumer_state->failed ? EIO : 0;
    }
}

static void bc_duplicate_worker_full_iter(size_t iteration_index, void* user_argument)
{
    bc_duplicate_worker_full_dispatch_t* dispatch = (bc_duplicate_worker_full_dispatch_t*)user_argument;
    if (bc_duplicate_worker_full_should_stop(dispatch->signal_handler)) {
        return;
    }
    bc_allocators_context_t* worker_memory = bc_concurrency_worker_memory();
    if (worker_memory == NULL) {
        worker_memory = dispatch->main_memory_context;
    }
    bc_duplicate_worker_full_slot_t* slot = NULL;
    if (!bc_duplicate_worker_full_ensure_slot(dispatch, worker_memory, &slot)) {
        return;
    }
    const size_t batch_start = iteration_index * dispatch->batch_size;
    if (batch_start >= dispatch->candidate_count) {
        return;
    }
    const size_t remaining = dispatch->candidate_count - batch_start;
    const size_t batch_count = remaining < dispatch->batch_size ? remaining : dispatch->batch_size;
    bc_duplicate_worker_full_process_batch(dispatch, slot, batch_start, batch_count);
}

static void bc_duplicate_worker_full_destroy_slot(void* slot_data, size_t worker_index, void* arg)
{
    BC_UNUSED(worker_index);
    BC_UNUSED(arg);
    bc_duplicate_worker_full_slot_t* slot = (bc_duplicate_worker_full_slot_t*)slot_data;
    if (slot->ring_initialised && slot->ring != NULL) {
        bc_duplicate_reader_ring_destroy(slot->ring);
        slot->ring_initialised = false;
    }
    if (slot->xxh_states_ready) {
        for (size_t state_index = 0; state_index < BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY; ++state_index) {
            if (slot->xxh_states[state_index] != NULL) {
                XXH3_freeState(slot->xxh_states[state_index]);
                slot->xxh_states[state_index] = NULL;
            }
        }
        slot->xxh_states_ready = false;
    }
}

bool bc_duplicate_worker_full_pass(bc_allocators_context_t* memory_context, bc_concurrency_context_t* concurrency_context,
                                   bc_concurrency_signal_handler_t* signal_handler, bc_duplicate_algorithm_t algorithm,
                                   bc_duplicate_file_entry_t* entries, const bc_duplicate_group_t* candidate_groups,
                                   size_t candidate_group_count, bool force_single_thread, size_t* out_files_hashed)
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

    const bc_concurrency_slot_config_t slot_config = {
        .size = sizeof(bc_duplicate_worker_full_slot_t),
        .init = NULL,
        .destroy = bc_duplicate_worker_full_destroy_slot,
        .arg = NULL,
    };
    size_t worker_slot_index = 0;
    if (!bc_concurrency_register_slot(concurrency_context, &slot_config, &worker_slot_index)) {
        bc_allocators_pool_free(memory_context, candidate_indices);
        return false;
    }

    bc_duplicate_worker_full_dispatch_t dispatch = {
        .entries = entries,
        .candidate_indices = candidate_indices,
        .candidate_count = total_candidates,
        .signal_handler = signal_handler,
        .algorithm = algorithm,
        .batch_size = BC_DUPLICATE_FULL_BATCH_ITEM_CAPACITY,
        .worker_slot_index = worker_slot_index,
        .main_memory_context = memory_context,
    };

    const size_t batch_count = (total_candidates + dispatch.batch_size - 1) / dispatch.batch_size;
    bool dispatch_ok = true;
    if (force_single_thread) {
        for (size_t batch_index = 0; batch_index < batch_count; ++batch_index) {
            if (bc_duplicate_worker_full_should_stop(signal_handler)) {
                break;
            }
            bc_duplicate_worker_full_iter(batch_index, &dispatch);
        }
    } else {
        dispatch_ok = bc_concurrency_for(concurrency_context, 0, batch_count, 1, bc_duplicate_worker_full_iter, &dispatch);
    }

    bc_allocators_pool_free(memory_context, candidate_indices);
    if (!dispatch_ok) {
        return false;
    }

    *out_files_hashed = total_candidates;
    return true;
}
