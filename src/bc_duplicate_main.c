// SPDX-License-Identifier: MIT

#include "bc_duplicate_cli_internal.h"
#include "bc_duplicate_types_internal.h"

#include "bc_allocators.h"
#include "bc_concurrency.h"
#include "bc_core.h"
#include "bc_runtime.h"
#include "bc_runtime_cli.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct bc_duplicate_application_state {
    const bc_runtime_cli_parsed_t* parsed;
    bc_duplicate_cli_options_t cli_options;
    int exit_code;
} bc_duplicate_application_state_t;

static bool bc_duplicate_application_init(const bc_runtime_t* application, void* user_data)
{
    (void)application;
    (void)user_data;
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

    fputs("bc-duplicate: scan pipeline not implemented yet (iter 1 skeleton)\n", stderr);
    state->exit_code = 0;
    return true;
}

static void bc_duplicate_application_cleanup(const bc_runtime_t* application, void* user_data)
{
    (void)application;
    (void)user_data;
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
