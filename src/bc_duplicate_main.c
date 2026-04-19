// SPDX-License-Identifier: MIT

#include "bc_duplicate_cli_internal.h"
#include "bc_duplicate_discovery_internal.h"
#include "bc_duplicate_error_internal.h"
#include "bc_duplicate_filter_internal.h"
#include "bc_duplicate_grouping_internal.h"
#include "bc_duplicate_types_internal.h"
#include "bc_duplicate_worker_internal.h"

#include "bc_allocators.h"
#include "bc_concurrency.h"
#include "bc_containers_vector.h"
#include "bc_core.h"
#include "bc_runtime.h"
#include "bc_runtime_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BC_DUPLICATE_APPLICATION_ENTRY_INITIAL_CAPACITY ((size_t)1024)
#define BC_DUPLICATE_APPLICATION_ENTRY_MAX_CAPACITY     ((size_t)1U << 28)

typedef struct bc_duplicate_application_state {
    const bc_runtime_cli_parsed_t* parsed;
    bc_duplicate_cli_options_t cli_options;
    bc_containers_vector_t* entries;
    bc_duplicate_filter_t* filter;
    bc_duplicate_error_collector_t* errors;
    int exit_code;
} bc_duplicate_application_state_t;

static bool bc_duplicate_application_init(const bc_runtime_t* application, void* user_data)
{
    bc_duplicate_application_state_t* state = (bc_duplicate_application_state_t*)user_data;

    bc_allocators_context_t* memory_context = NULL;
    if (!bc_runtime_memory_context(application, &memory_context)) {
        state->exit_code = 1;
        return false;
    }

    if (!bc_duplicate_error_collector_create(memory_context, &state->errors)) {
        state->exit_code = 1;
        return false;
    }

    if (!bc_containers_vector_create(memory_context, sizeof(bc_duplicate_file_entry_t), BC_DUPLICATE_APPLICATION_ENTRY_INITIAL_CAPACITY,
                                     BC_DUPLICATE_APPLICATION_ENTRY_MAX_CAPACITY, &state->entries)) {
        state->exit_code = 1;
        return false;
    }

    if (state->cli_options.include_list != NULL || state->cli_options.exclude_list != NULL) {
        if (!bc_duplicate_filter_create(memory_context, state->cli_options.include_list, state->cli_options.exclude_list, &state->filter)) {
            state->exit_code = 1;
            return false;
        }
    }

    return true;
}

static bool bc_duplicate_application_run(const bc_runtime_t* application, void* user_data)
{
    bc_duplicate_application_state_t* state = (bc_duplicate_application_state_t*)user_data;

    bc_allocators_context_t* memory_context = NULL;
    if (!bc_runtime_memory_context(application, &memory_context)) {
        state->exit_code = 1;
        return false;
    }

    bc_concurrency_context_t* concurrency_context = NULL;
    if (!bc_runtime_parallel_context(application, &concurrency_context)) {
        state->exit_code = 1;
        return false;
    }

    bc_concurrency_signal_handler_t* signal_handler = NULL;
    bc_runtime_signal_handler(application, &signal_handler);

    bc_duplicate_discovery_options_t discovery_options = {
        .include_hidden = state->cli_options.include_hidden,
        .follow_symlinks = state->cli_options.follow_symlinks,
        .one_file_system = state->cli_options.one_file_system,
        .minimum_file_size = state->cli_options.minimum_file_size,
    };

    const char* const* positional_argument_values = state->cli_options.positional_argument_values;
    size_t positional_argument_count = (size_t)state->cli_options.positional_argument_count;

    size_t discovery_worker_count = bc_concurrency_effective_worker_count(concurrency_context);
    bool discovery_ok;
    if (discovery_worker_count >= 2) {
        discovery_ok = bc_duplicate_discovery_expand_parallel(memory_context, concurrency_context, state->entries, state->errors, signal_handler,
                                                              state->filter, &discovery_options, positional_argument_values,
                                                              positional_argument_count);
    } else {
        discovery_ok = bc_duplicate_discovery_expand(memory_context, state->entries, state->errors, signal_handler, state->filter,
                                                     &discovery_options, positional_argument_values, positional_argument_count);
    }
    if (!discovery_ok) {
        state->exit_code = 1;
        return false;
    }

    bool interrupted_after_discovery = false;
    bc_runtime_should_stop(application, &interrupted_after_discovery);
    if (interrupted_after_discovery) {
        fputs("bc-duplicate: interrupted by signal, aborting before grouping\n", stderr);
        state->exit_code = 130;
        return false;
    }

    bc_duplicate_error_collector_flush_to_stderr(state->errors);

    size_t entry_count = bc_containers_vector_length(state->entries);
    size_t error_count = bc_duplicate_error_collector_count(state->errors);
    fprintf(stderr, "bc-duplicate: discovery: %zu file(s) found, %zu error(s)\n", entry_count, error_count);

    if (entry_count == 0) {
        state->exit_code = error_count == 0 ? 0 : 1;
        return true;
    }

    bc_duplicate_file_entry_t* entries_array = NULL;
    if (!bc_allocators_pool_allocate(memory_context, entry_count * sizeof(bc_duplicate_file_entry_t), (void**)&entries_array)) {
        state->exit_code = 1;
        return false;
    }
    for (size_t index = 0; index < entry_count; ++index) {
        if (!bc_containers_vector_get(state->entries, index, &entries_array[index])) {
            bc_allocators_pool_free(memory_context, entries_array);
            state->exit_code = 1;
            return false;
        }
    }

    bc_duplicate_group_t* size_groups = NULL;
    size_t size_group_count = 0;
    size_t size_candidate_count = 0;
    size_t hardlinks_collapsed = 0;
    if (!bc_duplicate_grouping_by_size(memory_context, entries_array, entry_count, state->cli_options.match_hardlinks, &size_groups,
                                       &size_group_count, &size_candidate_count, &hardlinks_collapsed)) {
        bc_allocators_pool_free(memory_context, entries_array);
        state->exit_code = 1;
        return false;
    }

    fprintf(stderr, "bc-duplicate: size groups: %zu (%zu candidates, %zu hardlink(s) collapsed)\n", size_group_count, size_candidate_count,
            hardlinks_collapsed);

    if (size_group_count == 0) {
        if (size_groups != NULL) {
            bc_allocators_pool_free(memory_context, size_groups);
        }
        bc_allocators_pool_free(memory_context, entries_array);
        state->exit_code = 0;
        return true;
    }

    size_t fast_files_hashed = 0;
    if (!bc_duplicate_worker_fast_pass(memory_context, concurrency_context, signal_handler, entries_array, size_groups, size_group_count,
                                       &fast_files_hashed)) {
        bc_allocators_pool_free(memory_context, size_groups);
        bc_allocators_pool_free(memory_context, entries_array);
        state->exit_code = 1;
        return false;
    }

    bc_duplicate_group_t* fast_hash_groups = NULL;
    size_t fast_hash_group_count = 0;
    size_t fast_hash_candidate_count = 0;
    if (!bc_duplicate_grouping_by_fast_hash(memory_context, entries_array, size_groups, size_group_count, &fast_hash_groups,
                                            &fast_hash_group_count, &fast_hash_candidate_count)) {
        bc_allocators_pool_free(memory_context, size_groups);
        bc_allocators_pool_free(memory_context, entries_array);
        state->exit_code = 1;
        return false;
    }

    fprintf(stderr, "bc-duplicate: fast-hash groups: %zu (%zu candidates after %zu hashed) (full-hash pass not implemented yet)\n",
            fast_hash_group_count, fast_hash_candidate_count, fast_files_hashed);

    if (fast_hash_groups != NULL) {
        bc_allocators_pool_free(memory_context, fast_hash_groups);
    }
    bc_allocators_pool_free(memory_context, size_groups);
    bc_allocators_pool_free(memory_context, entries_array);

    state->exit_code = 0;
    return true;
}

static void bc_duplicate_application_cleanup(const bc_runtime_t* application, void* user_data)
{
    bc_duplicate_application_state_t* state = (bc_duplicate_application_state_t*)user_data;

    bc_allocators_context_t* memory_context = NULL;
    if (!bc_runtime_memory_context(application, &memory_context)) {
        return;
    }

    if (state->filter != NULL) {
        bc_duplicate_filter_destroy(memory_context, state->filter);
        state->filter = NULL;
    }
    if (state->entries != NULL) {
        size_t entry_count = bc_containers_vector_length(state->entries);
        for (size_t index = 0; index < entry_count; ++index) {
            bc_duplicate_file_entry_t entry;
            if (bc_containers_vector_get(state->entries, index, &entry) && entry.absolute_path != NULL) {
                bc_allocators_pool_free(memory_context, entry.absolute_path);
            }
        }
        bc_containers_vector_destroy(memory_context, state->entries);
        state->entries = NULL;
    }
    if (state->errors != NULL) {
        bc_duplicate_error_collector_destroy(memory_context, state->errors);
        state->errors = NULL;
    }
}

int main(int argument_count, char** argument_values)
{
    bc_allocators_context_config_t cli_memory_config = {.tracking_enabled = false};
    bc_allocators_context_t* cli_memory_context = NULL;
    if (!bc_allocators_context_create(&cli_memory_config, &cli_memory_context)) {
        fputs("bc-duplicate: failed to create CLI memory context\n", stderr);
        return 1;
    }

    bc_runtime_config_store_t* cli_store = NULL;
    if (!bc_runtime_config_store_create(cli_memory_context, &cli_store)) {
        fputs("bc-duplicate: failed to create CLI config store\n", stderr);
        bc_allocators_context_destroy(cli_memory_context);
        return 1;
    }

    const bc_runtime_cli_program_spec_t* spec = bc_duplicate_cli_program_spec();
    bc_runtime_cli_parsed_t parsed;
    bc_runtime_cli_parse_status_t parse_status =
        bc_runtime_cli_parse(spec, argument_count, (const char* const*)argument_values, cli_store, &parsed, stderr);

    if (parse_status == BC_RUNTIME_CLI_PARSE_HELP_GLOBAL) {
        bc_runtime_cli_print_help_global(spec, stdout);
        bc_runtime_config_store_destroy(cli_memory_context, cli_store);
        bc_allocators_context_destroy(cli_memory_context);
        return 0;
    }
    if (parse_status == BC_RUNTIME_CLI_PARSE_HELP_COMMAND) {
        bc_runtime_cli_print_help_command(spec, parsed.command, stdout);
        bc_runtime_config_store_destroy(cli_memory_context, cli_store);
        bc_allocators_context_destroy(cli_memory_context);
        return 0;
    }
    if (parse_status == BC_RUNTIME_CLI_PARSE_VERSION) {
        bc_runtime_cli_print_version(spec, stdout);
        bc_runtime_config_store_destroy(cli_memory_context, cli_store);
        bc_allocators_context_destroy(cli_memory_context);
        return 0;
    }
    if (parse_status == BC_RUNTIME_CLI_PARSE_ERROR) {
        bc_runtime_config_store_destroy(cli_memory_context, cli_store);
        bc_allocators_context_destroy(cli_memory_context);
        return 2;
    }

    bc_duplicate_application_state_t state;
    bc_core_zero(&state, sizeof(state));
    state.parsed = &parsed;

    if (!bc_duplicate_cli_bind_options(cli_store, &parsed, &state.cli_options)) {
        bc_runtime_config_store_destroy(cli_memory_context, cli_store);
        bc_allocators_context_destroy(cli_memory_context);
        return 2;
    }

    bc_concurrency_config_t parallel_config;
    bc_core_zero(&parallel_config, sizeof(parallel_config));
    if (state.cli_options.threads_mode == BC_DUPLICATE_THREADS_MODE_MONO) {
        parallel_config.worker_count_explicit = true;
        parallel_config.worker_count = 0;
    } else if (state.cli_options.threads_mode == BC_DUPLICATE_THREADS_MODE_EXPLICIT) {
        parallel_config.worker_count_explicit = true;
        parallel_config.worker_count = state.cli_options.explicit_worker_count >= 1 ? state.cli_options.explicit_worker_count - 1 : 0;
    }

    bc_runtime_config_t runtime_config = {
        .max_pool_memory = 0,
        .memory_tracking_enabled = true,
        .log_level = BC_RUNTIME_LOG_LEVEL_WARN,
        .config_file_path = NULL,
        .argument_count = 0,
        .argument_values = NULL,
        .parallel_config = &parallel_config,
    };
    bc_runtime_callbacks_t runtime_callbacks = {
        .init = bc_duplicate_application_init,
        .run = bc_duplicate_application_run,
        .cleanup = bc_duplicate_application_cleanup,
    };

    bc_runtime_t* runtime = NULL;
    if (!bc_runtime_create(&runtime_config, &runtime_callbacks, &state, &runtime)) {
        fputs("bc-duplicate: failed to initialize runtime\n", stderr);
        bc_runtime_config_store_destroy(cli_memory_context, cli_store);
        bc_allocators_context_destroy(cli_memory_context);
        return 1;
    }

    bc_runtime_run(runtime);
    bc_runtime_destroy(runtime);

    bc_runtime_config_store_destroy(cli_memory_context, cli_store);
    bc_allocators_context_destroy(cli_memory_context);

    return state.exit_code;
}
