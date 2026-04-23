// SPDX-License-Identifier: MIT

#include "bc_duplicate_cli_internal.h"
#include "bc_duplicate_strings_internal.h"

#include "bc_core.h"
#include "bc_runtime.h"
#include "bc_runtime_cli.h"

#include <stdio.h>
#include <stdlib.h>

static const char* const bc_duplicate_algorithm_values[] = {"xxh3", "xxh128", "sha256", NULL};

static const bc_runtime_cli_option_spec_t bc_duplicate_global_options[] = {
    {
        .long_name = "threads",
        .type = BC_RUNTIME_CLI_OPTION_STRING,
        .default_value = "auto",
        .value_placeholder = "auto|0|N",
        .help_summary = "worker count: auto, 0 (single-thread), or N",
    },
};

static const bc_runtime_cli_option_spec_t bc_duplicate_scan_options[] = {
    {
        .long_name = "algorithm",
        .type = BC_RUNTIME_CLI_OPTION_ENUM,
        .allowed_values = bc_duplicate_algorithm_values,
        .default_value = "xxh3",
        .value_placeholder = "ALGO",
        .help_summary = "full-file hash algorithm",
    },
    {
        .long_name = "minimum-size",
        .type = BC_RUNTIME_CLI_OPTION_STRING,
        .default_value = "1",
        .value_placeholder = "BYTES",
        .help_summary = "skip files smaller than BYTES",
    },
    {
        .long_name = "include",
        .type = BC_RUNTIME_CLI_OPTION_LIST,
        .value_placeholder = "GLOB",
        .help_summary = "only include files whose basename matches",
    },
    {
        .long_name = "exclude",
        .type = BC_RUNTIME_CLI_OPTION_LIST,
        .value_placeholder = "GLOB",
        .help_summary = "skip files or directories whose basename matches",
    },
    {
        .long_name = "hidden",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "include hidden files (basename starting with '.')",
    },
    {
        .long_name = "follow-symlinks",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "follow symbolic links during traversal",
    },
    {
        .long_name = "match-hardlinks",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "report files sharing an inode as duplicates",
    },
    {
        .long_name = "one-file-system",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "do not cross filesystem boundaries",
    },
    {
        .long_name = "output",
        .type = BC_RUNTIME_CLI_OPTION_STRING,
        .default_value = "auto",
        .value_placeholder = "auto|-|PATH",
        .help_summary = "output destination (file -> json, stdout -> jdupes-style)",
    },
};

static const bc_runtime_cli_option_spec_t bc_duplicate_summary_options[] = {
    {
        .long_name = "algorithm",
        .type = BC_RUNTIME_CLI_OPTION_ENUM,
        .allowed_values = bc_duplicate_algorithm_values,
        .default_value = "xxh3",
        .value_placeholder = "ALGO",
        .help_summary = "full-file hash algorithm",
    },
    {
        .long_name = "minimum-size",
        .type = BC_RUNTIME_CLI_OPTION_STRING,
        .default_value = "1",
        .value_placeholder = "BYTES",
        .help_summary = "skip files smaller than BYTES",
    },
    {
        .long_name = "include",
        .type = BC_RUNTIME_CLI_OPTION_LIST,
        .value_placeholder = "GLOB",
        .help_summary = "only include files whose basename matches",
    },
    {
        .long_name = "exclude",
        .type = BC_RUNTIME_CLI_OPTION_LIST,
        .value_placeholder = "GLOB",
        .help_summary = "skip files or directories whose basename matches",
    },
    {
        .long_name = "hidden",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "include hidden files (basename starting with '.')",
    },
    {
        .long_name = "follow-symlinks",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "follow symbolic links during traversal",
    },
    {
        .long_name = "match-hardlinks",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "count files sharing an inode as duplicates",
    },
    {
        .long_name = "one-file-system",
        .type = BC_RUNTIME_CLI_OPTION_FLAG,
        .help_summary = "do not cross filesystem boundaries",
    },
};

static const bc_runtime_cli_command_spec_t bc_duplicate_commands[] = {
    {
        .name = "scan",
        .summary = "find duplicate files and report them as groups",
        .options = bc_duplicate_scan_options,
        .option_count = sizeof(bc_duplicate_scan_options) / sizeof(bc_duplicate_scan_options[0]),
        .positional_usage = "<path>...",
        .positional_min = 1,
        .positional_max = 0,
    },
    {
        .name = "summary",
        .summary = "scan for duplicates and print statistics only",
        .options = bc_duplicate_summary_options,
        .option_count = sizeof(bc_duplicate_summary_options) / sizeof(bc_duplicate_summary_options[0]),
        .positional_usage = "<path>...",
        .positional_min = 1,
        .positional_max = 0,
    },
};

static const bc_runtime_cli_program_spec_t bc_duplicate_program_spec_value = {
    .program_name = "bc-duplicate",
    .version = BC_DUPLICATE_VERSION_STRING,
    .summary = "parallel duplicate-file finder",
    .global_options = bc_duplicate_global_options,
    .global_option_count = sizeof(bc_duplicate_global_options) / sizeof(bc_duplicate_global_options[0]),
    .commands = bc_duplicate_commands,
    .command_count = sizeof(bc_duplicate_commands) / sizeof(bc_duplicate_commands[0]),
};

const bc_runtime_cli_program_spec_t* bc_duplicate_cli_program_spec(void)
{
    return &bc_duplicate_program_spec_value;
}

static bool bc_duplicate_cli_bind_algorithm(const char* value, bc_duplicate_algorithm_t* out_algorithm)
{
    if (bc_duplicate_strings_equal(value, "xxh3")) {
        *out_algorithm = BC_DUPLICATE_ALGORITHM_XXH3;
        return true;
    }
    if (bc_duplicate_strings_equal(value, "xxh128")) {
        *out_algorithm = BC_DUPLICATE_ALGORITHM_XXH128;
        return true;
    }
    if (bc_duplicate_strings_equal(value, "sha256")) {
        *out_algorithm = BC_DUPLICATE_ALGORITHM_SHA256;
        return true;
    }
    return false;
}

static bool bc_duplicate_cli_bind_threads(const char* value, bc_duplicate_threads_mode_t* out_mode, size_t* out_explicit_worker_count)
{
    if (bc_duplicate_strings_equal(value, "auto")) {
        *out_mode = BC_DUPLICATE_THREADS_MODE_AUTO;
        *out_explicit_worker_count = 0;
        return true;
    }
    if (value[0] == '\0') {
        return false;
    }
    char* end_pointer = NULL;
    unsigned long parsed_value = strtoul(value, &end_pointer, 10);
    if (end_pointer == value || *end_pointer != '\0') {
        return false;
    }
    if (parsed_value == 0) {
        *out_mode = BC_DUPLICATE_THREADS_MODE_MONO;
        *out_explicit_worker_count = 0;
        return true;
    }
    *out_mode = BC_DUPLICATE_THREADS_MODE_EXPLICIT;
    *out_explicit_worker_count = (size_t)parsed_value;
    return true;
}

static bool bc_duplicate_cli_bind_output(const char* value, bc_duplicate_output_destination_mode_t* out_mode, const char** out_path)
{
    if (bc_duplicate_strings_equal(value, "auto")) {
        *out_mode = BC_DUPLICATE_OUTPUT_DESTINATION_AUTO;
        *out_path = NULL;
        return true;
    }
    if (bc_duplicate_strings_equal(value, "-")) {
        *out_mode = BC_DUPLICATE_OUTPUT_DESTINATION_STDOUT;
        *out_path = NULL;
        return true;
    }
    if (value[0] == '\0') {
        return false;
    }
    *out_mode = BC_DUPLICATE_OUTPUT_DESTINATION_FILE;
    *out_path = value;
    return true;
}

static bool bc_duplicate_cli_bind_minimum_size(const char* value, size_t* out_minimum_size)
{
    if (value == NULL || value[0] == '\0') {
        return false;
    }
    char* end_pointer = NULL;
    unsigned long long parsed_value = strtoull(value, &end_pointer, 10);
    if (end_pointer == value || *end_pointer != '\0') {
        return false;
    }
    *out_minimum_size = (size_t)parsed_value;
    return true;
}

bool bc_duplicate_cli_bind_global_threads(const bc_runtime_config_store_t* store, bc_duplicate_threads_mode_t* out_mode,
                                          size_t* out_explicit_worker_count)
{
    const char* threads_value = NULL;
    if (!bc_runtime_config_store_get_string(store, "global.threads", &threads_value)) {
        fputs("bc-duplicate: internal error: missing global.threads\n", stderr);
        return false;
    }
    if (!bc_duplicate_cli_bind_threads(threads_value, out_mode, out_explicit_worker_count)) {
        fprintf(stderr, "bc-duplicate: invalid value for --threads: '%s'\n", threads_value);
        return false;
    }
    return true;
}

static bool bc_duplicate_cli_bind_command_options(const bc_runtime_config_store_t* store, const char* command_name,
                                                  bc_duplicate_cli_options_t* out_options)
{
    char key_buffer[128];

    snprintf(key_buffer, sizeof(key_buffer), "%s.algorithm", command_name);
    const char* algorithm_value = NULL;
    if (!bc_runtime_config_store_get_string(store, key_buffer, &algorithm_value)) {
        fprintf(stderr, "bc-duplicate: internal error: missing %s\n", key_buffer);
        return false;
    }
    if (!bc_duplicate_cli_bind_algorithm(algorithm_value, &out_options->algorithm)) {
        fprintf(stderr, "bc-duplicate: invalid value for --algorithm: '%s'\n", algorithm_value);
        return false;
    }

    snprintf(key_buffer, sizeof(key_buffer), "%s.minimum-size", command_name);
    const char* minimum_size_value = NULL;
    if (!bc_runtime_config_store_get_string(store, key_buffer, &minimum_size_value)) {
        fprintf(stderr, "bc-duplicate: internal error: missing %s\n", key_buffer);
        return false;
    }
    if (!bc_duplicate_cli_bind_minimum_size(minimum_size_value, &out_options->minimum_file_size)) {
        fprintf(stderr, "bc-duplicate: invalid value for --minimum-size: '%s'\n", minimum_size_value);
        return false;
    }

    snprintf(key_buffer, sizeof(key_buffer), "%s.include", command_name);
    const char* include_value = NULL;
    if (bc_runtime_config_store_get_string(store, key_buffer, &include_value)) {
        out_options->include_list = include_value;
    }

    snprintf(key_buffer, sizeof(key_buffer), "%s.exclude", command_name);
    const char* exclude_value = NULL;
    if (bc_runtime_config_store_get_string(store, key_buffer, &exclude_value)) {
        out_options->exclude_list = exclude_value;
    }

    snprintf(key_buffer, sizeof(key_buffer), "%s.hidden", command_name);
    bool hidden_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &hidden_value);
    out_options->include_hidden = hidden_value;

    snprintf(key_buffer, sizeof(key_buffer), "%s.follow-symlinks", command_name);
    bool follow_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &follow_value);
    out_options->follow_symlinks = follow_value;

    snprintf(key_buffer, sizeof(key_buffer), "%s.match-hardlinks", command_name);
    bool match_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &match_value);
    out_options->match_hardlinks = match_value;

    snprintf(key_buffer, sizeof(key_buffer), "%s.one-file-system", command_name);
    bool one_fs_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &one_fs_value);
    out_options->one_file_system = one_fs_value;

    return true;
}

bool bc_duplicate_cli_bind_options(const bc_runtime_config_store_t* store, const bc_runtime_cli_parsed_t* parsed,
                                   bc_duplicate_cli_options_t* out_options)
{
    bc_core_zero(out_options, sizeof(*out_options));

    if (!bc_duplicate_cli_bind_global_threads(store, &out_options->threads_mode, &out_options->explicit_worker_count)) {
        return false;
    }

    if (parsed->command == NULL) {
        fputs("bc-duplicate: internal error: no command parsed\n", stderr);
        return false;
    }

    const char* command_name = parsed->command->name;

    if (bc_duplicate_strings_equal(command_name, "scan")) {
        out_options->command = BC_DUPLICATE_COMMAND_SCAN;
    } else if (bc_duplicate_strings_equal(command_name, "summary")) {
        out_options->command = BC_DUPLICATE_COMMAND_SUMMARY;
    } else {
        fprintf(stderr, "bc-duplicate: internal error: unknown command '%s'\n", command_name);
        return false;
    }

    if (!bc_duplicate_cli_bind_command_options(store, command_name, out_options)) {
        return false;
    }

    if (out_options->command == BC_DUPLICATE_COMMAND_SCAN) {
        const char* output_value = NULL;
        if (!bc_runtime_config_store_get_string(store, "scan.output", &output_value)) {
            fputs("bc-duplicate: internal error: missing scan.output\n", stderr);
            return false;
        }
        if (!bc_duplicate_cli_bind_output(output_value, &out_options->output_destination_mode, &out_options->output_destination_path)) {
            fprintf(stderr, "bc-duplicate: invalid value for --output: '%s'\n", output_value);
            return false;
        }
    } else {
        out_options->output_destination_mode = BC_DUPLICATE_OUTPUT_DESTINATION_STDOUT;
        out_options->output_destination_path = NULL;
    }

    out_options->positional_argument_count = (int)parsed->positional_count;
    out_options->positional_argument_values = parsed->positional_values;
    return true;
}
