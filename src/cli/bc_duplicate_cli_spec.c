// SPDX-License-Identifier: MIT

#include "bc_duplicate_cli_internal.h"
#include "bc_duplicate_strings_internal.h"

#include "bc_core.h"
#include "bc_runtime.h"
#include "bc_runtime_cli.h"

static void bc_duplicate_cli_log_cstring(const char* message)
{
    char buffer[512];
    bc_core_writer_t writer;
    if (!bc_core_writer_init_standard_error(&writer, buffer, sizeof(buffer))) {
        return;
    }
    bc_core_writer_write_cstring(&writer, message);
    bc_core_writer_destroy(&writer);
}

static void bc_duplicate_cli_log_invalid_value(const char* option_name, const char* value)
{
    char buffer[512];
    bc_core_writer_t writer;
    if (!bc_core_writer_init_standard_error(&writer, buffer, sizeof(buffer))) {
        return;
    }
    bc_core_writer_write_cstring(&writer, "bc-duplicate: invalid value for ");
    bc_core_writer_write_cstring(&writer, option_name);
    bc_core_writer_write_cstring(&writer, ": '");
    bc_core_writer_write_cstring(&writer, value);
    bc_core_writer_write_cstring(&writer, "'\n");
    bc_core_writer_destroy(&writer);
}

static void bc_duplicate_cli_log_internal_missing(const char* key)
{
    char buffer[512];
    bc_core_writer_t writer;
    if (!bc_core_writer_init_standard_error(&writer, buffer, sizeof(buffer))) {
        return;
    }
    bc_core_writer_write_cstring(&writer, "bc-duplicate: internal error: missing ");
    bc_core_writer_write_cstring(&writer, key);
    bc_core_writer_write_char(&writer, '\n');
    bc_core_writer_destroy(&writer);
}

static bool bc_duplicate_cli_format_command_key(char* buffer, size_t capacity, const char* command_name, const char* suffix)
{
    bc_core_writer_t writer;
    if (!bc_core_writer_init_buffer_only(&writer, buffer, capacity - 1U)) {
        return false;
    }
    if (!bc_core_writer_write_cstring(&writer, command_name)) {
        return false;
    }
    if (!bc_core_writer_write_cstring(&writer, suffix)) {
        return false;
    }
    const char* data = NULL;
    size_t length = 0;
    if (!bc_core_writer_buffer_data(&writer, &data, &length)) {
        return false;
    }
    buffer[length] = '\0';
    return true;
}

static const char* const bc_duplicate_algorithm_values[] = {"xxh3", "xxh128", "sha256", NULL};

static const bc_runtime_cli_option_spec_t bc_duplicate_global_options[] = {
    {
        .long_name = "threads",
        .type = BC_RUNTIME_CLI_OPTION_STRING,
        .default_value = "auto",
        .value_placeholder = "mono|auto|io|N",
        .help_summary = "thread mode: mono (single-thread, alias 0), auto (CPU-bound, physical cores - 1, default), io (I/O-bound, logical processors - 1, oversubscribe), or N (1..logical_cpu_count)",
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
    if (bc_duplicate_strings_equal(value, "mono")) {
        *out_mode = BC_DUPLICATE_THREADS_MODE_MONO;
        *out_explicit_worker_count = 0;
        return true;
    }
    if (bc_duplicate_strings_equal(value, "auto")) {
        *out_mode = BC_DUPLICATE_THREADS_MODE_AUTO;
        *out_explicit_worker_count = 0;
        return true;
    }
    if (bc_duplicate_strings_equal(value, "io")) {
        *out_mode = BC_DUPLICATE_THREADS_MODE_IO;
        *out_explicit_worker_count = 0;
        return true;
    }
    if (value[0] == '\0') {
        return false;
    }
    size_t value_length = 0;
    if (!bc_core_length(value, '\0', &value_length)) {
        return false;
    }
    uint64_t parsed_value = 0;
    size_t consumed = 0;
    if (!bc_core_parse_unsigned_integer_64_decimal(value, value_length, &parsed_value, &consumed)) {
        return false;
    }
    if (consumed != value_length) {
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
    size_t value_length = 0;
    if (!bc_core_length(value, '\0', &value_length)) {
        return false;
    }
    uint64_t parsed_value = 0;
    size_t consumed = 0;
    if (!bc_core_parse_unsigned_integer_64_decimal(value, value_length, &parsed_value, &consumed)) {
        return false;
    }
    if (consumed != value_length) {
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
        bc_duplicate_cli_log_cstring("bc-duplicate: internal error: missing global.threads\n");
        return false;
    }
    if (!bc_duplicate_cli_bind_threads(threads_value, out_mode, out_explicit_worker_count)) {
        bc_duplicate_cli_log_invalid_value("--threads", threads_value);
        return false;
    }
    return true;
}

static bool bc_duplicate_cli_bind_command_options(const bc_runtime_config_store_t* store, const char* command_name,
                                                  bc_duplicate_cli_options_t* out_options)
{
    char key_buffer[128];

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".algorithm")) {
        return false;
    }
    const char* algorithm_value = NULL;
    if (!bc_runtime_config_store_get_string(store, key_buffer, &algorithm_value)) {
        bc_duplicate_cli_log_internal_missing(key_buffer);
        return false;
    }
    if (!bc_duplicate_cli_bind_algorithm(algorithm_value, &out_options->algorithm)) {
        bc_duplicate_cli_log_invalid_value("--algorithm", algorithm_value);
        return false;
    }

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".minimum-size")) {
        return false;
    }
    const char* minimum_size_value = NULL;
    if (!bc_runtime_config_store_get_string(store, key_buffer, &minimum_size_value)) {
        bc_duplicate_cli_log_internal_missing(key_buffer);
        return false;
    }
    if (!bc_duplicate_cli_bind_minimum_size(minimum_size_value, &out_options->minimum_file_size)) {
        bc_duplicate_cli_log_invalid_value("--minimum-size", minimum_size_value);
        return false;
    }

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".include")) {
        return false;
    }
    const char* include_value = NULL;
    if (bc_runtime_config_store_get_string(store, key_buffer, &include_value)) {
        out_options->include_list = include_value;
    }

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".exclude")) {
        return false;
    }
    const char* exclude_value = NULL;
    if (bc_runtime_config_store_get_string(store, key_buffer, &exclude_value)) {
        out_options->exclude_list = exclude_value;
    }

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".hidden")) {
        return false;
    }
    bool hidden_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &hidden_value);
    out_options->include_hidden = hidden_value;

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".follow-symlinks")) {
        return false;
    }
    bool follow_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &follow_value);
    out_options->follow_symlinks = follow_value;

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".match-hardlinks")) {
        return false;
    }
    bool match_value = false;
    (void)bc_runtime_config_store_get_boolean(store, key_buffer, &match_value);
    out_options->match_hardlinks = match_value;

    if (!bc_duplicate_cli_format_command_key(key_buffer, sizeof(key_buffer), command_name, ".one-file-system")) {
        return false;
    }
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
        bc_duplicate_cli_log_cstring("bc-duplicate: internal error: no command parsed\n");
        return false;
    }

    const char* command_name = parsed->command->name;

    if (bc_duplicate_strings_equal(command_name, "scan")) {
        out_options->command = BC_DUPLICATE_COMMAND_SCAN;
    } else if (bc_duplicate_strings_equal(command_name, "summary")) {
        out_options->command = BC_DUPLICATE_COMMAND_SUMMARY;
    } else {
        char buffer[512];
        bc_core_writer_t writer;
        if (bc_core_writer_init_standard_error(&writer, buffer, sizeof(buffer))) {
            bc_core_writer_write_cstring(&writer, "bc-duplicate: internal error: unknown command '");
            bc_core_writer_write_cstring(&writer, command_name);
            bc_core_writer_write_cstring(&writer, "'\n");
            bc_core_writer_destroy(&writer);
        }
        return false;
    }

    if (!bc_duplicate_cli_bind_command_options(store, command_name, out_options)) {
        return false;
    }

    if (out_options->command == BC_DUPLICATE_COMMAND_SCAN) {
        const char* output_value = NULL;
        if (!bc_runtime_config_store_get_string(store, "scan.output", &output_value)) {
            bc_duplicate_cli_log_cstring("bc-duplicate: internal error: missing scan.output\n");
            return false;
        }
        if (!bc_duplicate_cli_bind_output(output_value, &out_options->output_destination_mode, &out_options->output_destination_path)) {
            bc_duplicate_cli_log_invalid_value("--output", output_value);
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
