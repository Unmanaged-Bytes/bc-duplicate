// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "bc_duplicate_output_internal.h"

static void test_simple_output_writes_groups_separated_by_blank_line(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t entries[5] = {0};
    entries[0].absolute_path = (char*)"/dir/a";
    entries[1].absolute_path = (char*)"/dir/b";
    entries[2].absolute_path = (char*)"/dir/c";
    entries[3].absolute_path = (char*)"/dir/d";
    entries[4].absolute_path = (char*)"/dir/e";
    bc_duplicate_group_t groups[2] = {
        {.start_index = 0, .entry_count = 2, .file_size = 100},
        {.start_index = 2, .entry_count = 3, .file_size = 200},
    };

    char buffer[512];
    FILE* stream = fmemopen(buffer, sizeof(buffer), "w");
    assert_non_null(stream);
    assert_true(bc_duplicate_output_simple_write(stream, entries, groups, 2));
    fflush(stream);
    long position = ftell(stream);
    fclose(stream);
    buffer[position] = '\0';
    assert_string_equal(buffer, "/dir/a\n/dir/b\n\n/dir/c\n/dir/d\n/dir/e\n");
}

static void test_simple_output_no_groups_writes_nothing(void** state)
{
    (void)state;
    char buffer[64];
    FILE* stream = fmemopen(buffer, sizeof(buffer), "w");
    assert_non_null(stream);
    assert_true(bc_duplicate_output_simple_write(stream, NULL, NULL, 0));
    fflush(stream);
    long position = ftell(stream);
    fclose(stream);
    assert_int_equal(position, 0);
}

static void test_summary_output_includes_all_fields(void** state)
{
    (void)state;
    bc_duplicate_statistics_t statistics = {
        .files_scanned = 100,
        .directories_scanned = 5,
        .files_skipped = 2,
        .hardlinks_collapsed = 7,
        .size_candidate_count = 50,
        .files_hashed_fast = 50,
        .files_hashed_full = 30,
        .duplicate_group_count = 4,
        .duplicate_file_count = 12,
        .wasted_bytes = 4096,
        .wall_ms = 1234,
    };
    char buffer[1024];
    FILE* stream = fmemopen(buffer, sizeof(buffer), "w");
    assert_non_null(stream);
    assert_true(bc_duplicate_output_summary_write(stream, &statistics));
    fflush(stream);
    long position = ftell(stream);
    fclose(stream);
    buffer[position] = '\0';
    assert_non_null(strstr(buffer, "Files scanned:        100"));
    assert_non_null(strstr(buffer, "Hardlinks collapsed:  7"));
    assert_non_null(strstr(buffer, "Duplicate groups:     4"));
    assert_non_null(strstr(buffer, "Wasted bytes:         4096"));
    assert_non_null(strstr(buffer, "Wall ms:              1234"));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_simple_output_writes_groups_separated_by_blank_line),
        cmocka_unit_test(test_simple_output_no_groups_writes_nothing),
        cmocka_unit_test(test_summary_output_includes_all_fields),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
