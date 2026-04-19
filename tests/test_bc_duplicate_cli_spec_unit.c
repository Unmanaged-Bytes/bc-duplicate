// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bc_allocators.h"
#include "bc_duplicate_cli_internal.h"
#include "bc_runtime.h"
#include "bc_runtime_cli.h"

extern bool bc_runtime_config_store_set(bc_runtime_config_store_t* store, const char* key, const char* value);
extern bool bc_runtime_config_store_sort(bc_runtime_config_store_t* store);

struct fixture {
    bc_allocators_context_t* memory_context;
    bc_runtime_config_store_t* store;
};

static int setup(void** state)
{
    struct fixture* fixture = test_calloc(1, sizeof(*fixture));
    bc_allocators_context_config_t config = {.tracking_enabled = true};
    if (!bc_allocators_context_create(&config, &fixture->memory_context)) {
        test_free(fixture);
        return -1;
    }
    if (!bc_runtime_config_store_create(fixture->memory_context, &fixture->store)) {
        bc_allocators_context_destroy(fixture->memory_context);
        test_free(fixture);
        return -1;
    }
    *state = fixture;
    return 0;
}

static int teardown(void** state)
{
    struct fixture* fixture = *state;
    bc_runtime_config_store_destroy(fixture->memory_context, fixture->store);
    bc_allocators_context_destroy(fixture->memory_context);
    test_free(fixture);
    return 0;
}

static bc_runtime_cli_parse_status_t parse_argv(bc_runtime_config_store_t* store, int argc, const char** argv,
                                                bc_runtime_cli_parsed_t* out_parsed)
{
    FILE* err = fmemopen(NULL, 4096, "w");
    bc_runtime_cli_parse_status_t status = bc_runtime_cli_parse(bc_duplicate_cli_program_spec(), argc, argv, store, out_parsed, err);
    fclose(err);
    return status;
}

static void test_program_spec_has_scan_and_summary(void** state)
{
    (void)state;
    const bc_runtime_cli_program_spec_t* spec = bc_duplicate_cli_program_spec();
    assert_string_equal(spec->program_name, "bc-duplicate");
    assert_string_equal(spec->version, "1.0.0");
    assert_int_equal(spec->command_count, 2);
    assert_string_equal(spec->commands[0].name, "scan");
    assert_string_equal(spec->commands[1].name, "summary");
    assert_int_equal(spec->global_option_count, 1);
    assert_string_equal(spec->global_options[0].long_name, "threads");
}

static void test_bind_global_threads_auto(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 3, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_threads_mode_t mode = BC_DUPLICATE_THREADS_MODE_EXPLICIT;
    size_t count = 99;
    assert_true(bc_duplicate_cli_bind_global_threads(fixture->store, &mode, &count));
    assert_int_equal(mode, BC_DUPLICATE_THREADS_MODE_AUTO);
    assert_int_equal(count, 0);
}

static void test_bind_global_threads_mono(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "--threads=0", "scan", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 4, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_threads_mode_t mode = BC_DUPLICATE_THREADS_MODE_AUTO;
    size_t count = 99;
    assert_true(bc_duplicate_cli_bind_global_threads(fixture->store, &mode, &count));
    assert_int_equal(mode, BC_DUPLICATE_THREADS_MODE_MONO);
    assert_int_equal(count, 0);
}

static void test_bind_global_threads_explicit(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "--threads=7", "scan", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 4, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_threads_mode_t mode = BC_DUPLICATE_THREADS_MODE_AUTO;
    size_t count = 0;
    assert_true(bc_duplicate_cli_bind_global_threads(fixture->store, &mode, &count));
    assert_int_equal(mode, BC_DUPLICATE_THREADS_MODE_EXPLICIT);
    assert_int_equal(count, 7);
}

static void test_scan_default_options(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 3, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_int_equal(options.command, BC_DUPLICATE_COMMAND_SCAN);
    assert_int_equal(options.algorithm, BC_DUPLICATE_ALGORITHM_XXH3);
    assert_int_equal(options.threads_mode, BC_DUPLICATE_THREADS_MODE_AUTO);
    assert_int_equal(options.minimum_file_size, 1);
    assert_int_equal(options.output_destination_mode, BC_DUPLICATE_OUTPUT_DESTINATION_AUTO);
    assert_false(options.include_hidden);
    assert_false(options.follow_symlinks);
    assert_false(options.match_hardlinks);
    assert_false(options.one_file_system);
    assert_int_equal(options.positional_argument_count, 1);
    assert_string_equal(options.positional_argument_values[0], "/path");
}

static void test_scan_all_algorithms(void** state)
{
    struct fixture* fixture = *state;
    const char* algorithms[3] = {"xxh3", "xxh128", "sha256"};
    const bc_duplicate_algorithm_t expected[3] = {
        BC_DUPLICATE_ALGORITHM_XXH3,
        BC_DUPLICATE_ALGORITHM_XXH128,
        BC_DUPLICATE_ALGORITHM_SHA256,
    };
    for (size_t index = 0; index < 3; ++index) {
        bc_runtime_config_store_t* local_store = NULL;
        assert_true(bc_runtime_config_store_create(fixture->memory_context, &local_store));
        char algorithm_argument[64];
        snprintf(algorithm_argument, sizeof(algorithm_argument), "--algorithm=%s", algorithms[index]);
        const char* argv[] = {"bc-duplicate", "scan", algorithm_argument, "/path"};
        bc_runtime_cli_parsed_t parsed;
        FILE* err = fmemopen(NULL, 4096, "w");
        assert_int_equal(bc_runtime_cli_parse(bc_duplicate_cli_program_spec(), 4, argv, local_store, &parsed, err), BC_RUNTIME_CLI_PARSE_OK);
        fclose(err);

        bc_duplicate_cli_options_t options;
        assert_true(bc_duplicate_cli_bind_options(local_store, &parsed, &options));
        assert_int_equal(options.algorithm, expected[index]);
        bc_runtime_config_store_destroy(fixture->memory_context, local_store);
    }
}

static void test_scan_output_stdout(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "--output=-", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 4, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_int_equal(options.output_destination_mode, BC_DUPLICATE_OUTPUT_DESTINATION_STDOUT);
    assert_null(options.output_destination_path);
}

static void test_scan_output_file(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "--output=/tmp/dup.json", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 4, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_int_equal(options.output_destination_mode, BC_DUPLICATE_OUTPUT_DESTINATION_FILE);
    assert_string_equal(options.output_destination_path, "/tmp/dup.json");
}

static void test_scan_flags_set(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "--hidden", "--follow-symlinks", "--match-hardlinks", "--one-file-system", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 7, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_true(options.include_hidden);
    assert_true(options.follow_symlinks);
    assert_true(options.match_hardlinks);
    assert_true(options.one_file_system);
}

static void test_scan_minimum_size(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "--minimum-size=4096", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 4, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_int_equal(options.minimum_file_size, 4096);
}

static void test_scan_include_and_exclude_propagated(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "scan", "--include=*.jpg", "--exclude=*.tmp", "--exclude=.git", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 6, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_string_equal(options.include_list, "*.jpg");
    assert_string_equal(options.exclude_list, "*.tmp\n.git");
}

static void test_summary_command_binds(void** state)
{
    struct fixture* fixture = *state;
    const char* argv[] = {"bc-duplicate", "summary", "--algorithm=sha256", "--minimum-size=1024", "/path"};
    bc_runtime_cli_parsed_t parsed;
    assert_int_equal(parse_argv(fixture->store, 5, argv, &parsed), BC_RUNTIME_CLI_PARSE_OK);

    bc_duplicate_cli_options_t options;
    assert_true(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
    assert_int_equal(options.command, BC_DUPLICATE_COMMAND_SUMMARY);
    assert_int_equal(options.algorithm, BC_DUPLICATE_ALGORITHM_SHA256);
    assert_int_equal(options.minimum_file_size, 1024);
    assert_int_equal(options.output_destination_mode, BC_DUPLICATE_OUTPUT_DESTINATION_STDOUT);
}

static void test_invalid_algorithm_fails(void** state)
{
    struct fixture* fixture = *state;
    assert_true(bc_runtime_config_store_set(fixture->store, "global.threads", "auto"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.algorithm", "md5"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.minimum-size", "1"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.output", "auto"));
    bc_runtime_config_store_sort(fixture->store);

    bc_runtime_cli_command_spec_t fake_command = {.name = "scan"};
    bc_runtime_cli_parsed_t parsed = {.command = &fake_command, .positional_values = NULL, .positional_count = 0};
    bc_duplicate_cli_options_t options;
    assert_false(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
}

static void test_invalid_minimum_size_fails(void** state)
{
    struct fixture* fixture = *state;
    assert_true(bc_runtime_config_store_set(fixture->store, "global.threads", "auto"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.algorithm", "xxh3"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.minimum-size", "12abc"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.output", "auto"));
    bc_runtime_config_store_sort(fixture->store);

    bc_runtime_cli_command_spec_t fake_command = {.name = "scan"};
    bc_runtime_cli_parsed_t parsed = {.command = &fake_command, .positional_values = NULL, .positional_count = 0};
    bc_duplicate_cli_options_t options;
    assert_false(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
}

static void test_invalid_output_fails(void** state)
{
    struct fixture* fixture = *state;
    assert_true(bc_runtime_config_store_set(fixture->store, "global.threads", "auto"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.algorithm", "xxh3"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.minimum-size", "1"));
    assert_true(bc_runtime_config_store_set(fixture->store, "scan.output", ""));
    bc_runtime_config_store_sort(fixture->store);

    bc_runtime_cli_command_spec_t fake_command = {.name = "scan"};
    bc_runtime_cli_parsed_t parsed = {.command = &fake_command, .positional_values = NULL, .positional_count = 0};
    bc_duplicate_cli_options_t options;
    assert_false(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
}

static void test_invalid_threads_fails(void** state)
{
    struct fixture* fixture = *state;
    assert_true(bc_runtime_config_store_set(fixture->store, "global.threads", "7xyz"));
    bc_runtime_config_store_sort(fixture->store);
    bc_duplicate_threads_mode_t mode;
    size_t count;
    assert_false(bc_duplicate_cli_bind_global_threads(fixture->store, &mode, &count));
}

static void test_unknown_command_fails(void** state)
{
    struct fixture* fixture = *state;
    assert_true(bc_runtime_config_store_set(fixture->store, "global.threads", "auto"));
    bc_runtime_config_store_sort(fixture->store);
    bc_runtime_cli_command_spec_t fake_command = {.name = "foobar"};
    bc_runtime_cli_parsed_t parsed = {.command = &fake_command, .positional_values = NULL, .positional_count = 0};
    bc_duplicate_cli_options_t options;
    assert_false(bc_duplicate_cli_bind_options(fixture->store, &parsed, &options));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_program_spec_has_scan_and_summary),
        cmocka_unit_test_setup_teardown(test_bind_global_threads_auto, setup, teardown),
        cmocka_unit_test_setup_teardown(test_bind_global_threads_mono, setup, teardown),
        cmocka_unit_test_setup_teardown(test_bind_global_threads_explicit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_default_options, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_all_algorithms, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_output_stdout, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_output_file, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_flags_set, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_minimum_size, setup, teardown),
        cmocka_unit_test_setup_teardown(test_scan_include_and_exclude_propagated, setup, teardown),
        cmocka_unit_test_setup_teardown(test_summary_command_binds, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_algorithm_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_minimum_size_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_output_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_threads_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_unknown_command_fails, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
