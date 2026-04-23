// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "bc_core_io.h"
#include "bc_duplicate_output_internal.h"

static size_t capture_simple(const bc_duplicate_file_entry_t* entries, const bc_duplicate_group_t* groups,
                             size_t group_count, char* out, size_t capacity)
{
    int fd = (int)syscall(SYS_memfd_create, "bc-duplicate-test", 0);
    assert_true(fd >= 0);
    char writer_buffer[8192];
    bc_core_writer_t writer;
    assert_true(bc_core_writer_init(&writer, fd, writer_buffer, sizeof(writer_buffer)));
    bool ok = bc_duplicate_output_simple_write(&writer, entries, groups, group_count);
    bc_core_writer_destroy(&writer);
    assert_true(ok);
    off_t length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (length < 0 || (size_t)length >= capacity) {
        close(fd);
        return 0;
    }
    ssize_t read_count = read(fd, out, (size_t)length);
    close(fd);
    if (read_count < 0) {
        return 0;
    }
    out[read_count] = '\0';
    return (size_t)read_count;
}

static size_t capture_summary(const bc_duplicate_statistics_t* statistics, char* out, size_t capacity)
{
    int fd = (int)syscall(SYS_memfd_create, "bc-duplicate-test", 0);
    assert_true(fd >= 0);
    char writer_buffer[8192];
    bc_core_writer_t writer;
    assert_true(bc_core_writer_init(&writer, fd, writer_buffer, sizeof(writer_buffer)));
    bool ok = bc_duplicate_output_summary_write(&writer, statistics);
    bc_core_writer_destroy(&writer);
    assert_true(ok);
    off_t length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (length < 0 || (size_t)length >= capacity) {
        close(fd);
        return 0;
    }
    ssize_t read_count = read(fd, out, (size_t)length);
    close(fd);
    if (read_count < 0) {
        return 0;
    }
    out[read_count] = '\0';
    return (size_t)read_count;
}

static size_t capture_json(bc_duplicate_algorithm_t algorithm, const bc_duplicate_file_entry_t* entries,
                           const bc_duplicate_group_t* groups, size_t group_count,
                           const bc_duplicate_statistics_t* statistics, char* out, size_t capacity)
{
    int fd = (int)syscall(SYS_memfd_create, "bc-duplicate-test", 0);
    assert_true(fd >= 0);
    char writer_buffer[8192];
    bc_core_writer_t writer;
    assert_true(bc_core_writer_init(&writer, fd, writer_buffer, sizeof(writer_buffer)));
    bool ok = bc_duplicate_output_json_write(&writer, algorithm, entries, groups, group_count, statistics);
    bc_core_writer_destroy(&writer);
    assert_true(ok);
    off_t length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (length < 0 || (size_t)length >= capacity) {
        close(fd);
        return 0;
    }
    ssize_t read_count = read(fd, out, (size_t)length);
    close(fd);
    if (read_count < 0) {
        return 0;
    }
    out[read_count] = '\0';
    return (size_t)read_count;
}

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
    capture_simple(entries, groups, 2, buffer, sizeof(buffer));
    assert_string_equal(buffer, "/dir/a\n/dir/b\n\n/dir/c\n/dir/d\n/dir/e\n");
}

static void test_simple_output_no_groups_writes_nothing(void** state)
{
    (void)state;
    char buffer[64];
    size_t length = capture_simple(NULL, NULL, 0, buffer, sizeof(buffer));
    assert_int_equal(length, 0);
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
    capture_summary(&statistics, buffer, sizeof(buffer));
    assert_non_null(strstr(buffer, "Files scanned:        100"));
    assert_non_null(strstr(buffer, "Hardlinks collapsed:  7"));
    assert_non_null(strstr(buffer, "Duplicate groups:     4"));
    assert_non_null(strstr(buffer, "Wasted bytes:         4096"));
    assert_non_null(strstr(buffer, "Wall ms:              1234"));
}

static void test_json_output_basic_shape(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t entries[3] = {0};
    entries[0].absolute_path = (char*)"/dir/a";
    entries[1].absolute_path = (char*)"/dir/b";
    entries[2].absolute_path = (char*)"/dir/c";
    bc_duplicate_group_t groups[1] = {{.start_index = 0, .entry_count = 2, .file_size = 100}};
    bc_duplicate_statistics_t statistics = {.files_scanned = 3, .duplicate_group_count = 1, .duplicate_file_count = 1, .wasted_bytes = 100};

    char buffer[2048];
    capture_json(BC_DUPLICATE_ALGORITHM_XXH3, entries, groups, 1, &statistics, buffer, sizeof(buffer));

    assert_non_null(strstr(buffer, "\"version\":\"1.0.0\""));
    assert_non_null(strstr(buffer, "\"tool\":\"bc-duplicate\""));
    assert_non_null(strstr(buffer, "\"algorithm\":\"xxh3\""));
    assert_non_null(strstr(buffer, "\"files_scanned\":3"));
    assert_non_null(strstr(buffer, "\"duplicate_groups\":1"));
    assert_non_null(strstr(buffer, "\"wasted_bytes\":100"));
    assert_non_null(strstr(buffer, "\"groups\":[{\"size\":100,\"files\":[\"/dir/a\",\"/dir/b\"]}]"));
}

static void test_json_output_escapes_special_characters(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t entries[2] = {0};
    entries[0].absolute_path = (char*)"/dir/with \"quote\"";
    entries[1].absolute_path = (char*)"/dir/with\\backslash";
    bc_duplicate_group_t groups[1] = {{.start_index = 0, .entry_count = 2, .file_size = 50}};
    bc_duplicate_statistics_t statistics = {0};

    char buffer[1024];
    capture_json(BC_DUPLICATE_ALGORITHM_SHA256, entries, groups, 1, &statistics, buffer, sizeof(buffer));

    assert_non_null(strstr(buffer, "/dir/with \\\"quote\\\""));
    assert_non_null(strstr(buffer, "/dir/with\\\\backslash"));
    assert_non_null(strstr(buffer, "\"algorithm\":\"sha256\""));
}

static void test_json_output_empty_groups_array(void** state)
{
    (void)state;
    bc_duplicate_statistics_t statistics = {0};
    char buffer[512];
    capture_json(BC_DUPLICATE_ALGORITHM_XXH128, NULL, NULL, 0, &statistics, buffer, sizeof(buffer));
    assert_non_null(strstr(buffer, "\"groups\":[]"));
    assert_non_null(strstr(buffer, "\"algorithm\":\"xxh128\""));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_simple_output_writes_groups_separated_by_blank_line),
        cmocka_unit_test(test_simple_output_no_groups_writes_nothing),
        cmocka_unit_test(test_summary_output_includes_all_fields),
        cmocka_unit_test(test_json_output_basic_shape),
        cmocka_unit_test(test_json_output_escapes_special_characters),
        cmocka_unit_test(test_json_output_empty_groups_array),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
