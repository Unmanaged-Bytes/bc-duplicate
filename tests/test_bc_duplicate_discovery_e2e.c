// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef BC_DUPLICATE_TEST_BINARY_PATH
#error "BC_DUPLICATE_TEST_BINARY_PATH must be defined"
#endif

#ifndef BC_DUPLICATE_TEST_FIXTURES_DIRECTORY
#error "BC_DUPLICATE_TEST_FIXTURES_DIRECTORY must be defined"
#endif

#define BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE 8192

static int bc_duplicate_test_write_file(const char* absolute_path, const void* payload, size_t payload_size)
{
    int file_descriptor = open(absolute_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor < 0) {
        return -1;
    }
    ssize_t written = write(file_descriptor, payload, payload_size);
    close(file_descriptor);
    if (written < 0 || (size_t)written != payload_size) {
        return -1;
    }
    return 0;
}

static int bc_duplicate_test_ensure_directory(const char* absolute_path)
{
    if (mkdir(absolute_path, 0755) == 0) {
        return 0;
    }
    if (errno == EEXIST) {
        return 0;
    }
    return -1;
}

static int bc_duplicate_test_run(const char* const* argv, char* stdout_buffer, size_t stdout_buffer_size, char* stderr_buffer,
                                 size_t stderr_buffer_size, int* out_exit_status)
{
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe(stdout_pipe) != 0) {
        return -1;
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return -1;
    }

    pid_t child_pid = fork();
    if (child_pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return -1;
    }
    if (child_pid == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        execv(BC_DUPLICATE_TEST_BINARY_PATH, (char* const*)argv);
        _exit(127);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    size_t stdout_total = 0;
    size_t stderr_total = 0;
    while (stdout_total + 1 < stdout_buffer_size) {
        ssize_t bytes_read = read(stdout_pipe[0], stdout_buffer + stdout_total, stdout_buffer_size - 1 - stdout_total);
        if (bytes_read <= 0) {
            break;
        }
        stdout_total += (size_t)bytes_read;
    }
    stdout_buffer[stdout_total] = '\0';
    while (stderr_total + 1 < stderr_buffer_size) {
        ssize_t bytes_read = read(stderr_pipe[0], stderr_buffer + stderr_total, stderr_buffer_size - 1 - stderr_total);
        if (bytes_read <= 0) {
            break;
        }
        stderr_total += (size_t)bytes_read;
    }
    stderr_buffer[stderr_total] = '\0';

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int child_status = 0;
    waitpid(child_pid, &child_status, 0);
    *out_exit_status = WIFEXITED(child_status) ? WEXITSTATUS(child_status) : -1;
    return 0;
}

static void test_help_succeeds(void** state)
{
    (void)state;
    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "--help", NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stdout_buffer, "scan"));
    assert_non_null(strstr(stdout_buffer, "summary"));
}

static void test_discovery_finds_seeded_files(void** state)
{
    (void)state;
    const char* base_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_basic";
    const char* nested_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_basic/nested";
    assert_int_equal(bc_duplicate_test_ensure_directory(base_directory), 0);
    assert_int_equal(bc_duplicate_test_ensure_directory(nested_directory), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_basic/a.txt", "hello", 5), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_basic/b.txt", "world", 5), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_basic/nested/c.txt", "nested", 6), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_basic/.hidden", "hidden", 6), 0);

    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", base_directory, NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 3 file(s) found"));
}

static void test_hidden_flag_includes_dotfiles(void** state)
{
    (void)state;
    const char* base_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_hidden";
    assert_int_equal(bc_duplicate_test_ensure_directory(base_directory), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_hidden/visible", "v", 1), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_hidden/.dotfile", "d", 1), 0);

    const char* without_hidden_argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", base_directory, NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(without_hidden_argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer),
                                           &exit_status),
                     0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 1 file(s) found"));

    const char* with_hidden_argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", "--hidden", base_directory, NULL};
    assert_int_equal(bc_duplicate_test_run(with_hidden_argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer),
                                           &exit_status),
                     0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 2 file(s) found"));
}

static void test_minimum_size_skips_small_files(void** state)
{
    (void)state;
    const char* base_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_minsize";
    assert_int_equal(bc_duplicate_test_ensure_directory(base_directory), 0);
    char small[8];
    char large[1024];
    memset(small, 'a', sizeof(small));
    memset(large, 'b', sizeof(large));
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_minsize/small.bin", small, sizeof(small)), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_minsize/large.bin", large, sizeof(large)), 0);

    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", "--minimum-size=512", base_directory, NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 1 file(s) found"));
}

static void test_exclude_pattern_skips_files(void** state)
{
    (void)state;
    const char* base_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_exclude";
    assert_int_equal(bc_duplicate_test_ensure_directory(base_directory), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_exclude/keep.c", "k", 1), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_exclude/skip.tmp", "s", 1), 0);

    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", "--exclude=*.tmp", base_directory, NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 1 file(s) found"));
}

static void test_pseudo_filesystem_skipped(void** state)
{
    (void)state;
    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", "/proc", NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 0 file(s) found"));
}

static void test_symlink_input_skipped(void** state)
{
    (void)state;
    const char* base_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_symlink";
    assert_int_equal(bc_duplicate_test_ensure_directory(base_directory), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_symlink/real.bin", "r", 1), 0);
    const char* link_path = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_symlink/link.bin";
    unlink(link_path);
    assert_int_equal(symlink(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/discovery_symlink/real.bin", link_path), 0);

    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "summary", base_directory, NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 1 file(s) found"));
}

static void test_fast_hash_groups_identical_files(void** state)
{
    (void)state;
    const char* base_directory = BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/fast_hash_dups";
    assert_int_equal(bc_duplicate_test_ensure_directory(base_directory), 0);
    const char payload[] = "the quick brown fox jumps over the lazy dog";
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/fast_hash_dups/a.txt", payload, sizeof(payload)), 0);
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/fast_hash_dups/b.txt", payload, sizeof(payload)), 0);
    const char different[] = "different content of the exact same length, X";
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/fast_hash_dups/c.txt", different, sizeof(different)), 0);
    const char short_payload[] = "tiny";
    assert_int_equal(bc_duplicate_test_write_file(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY "/fast_hash_dups/d.txt", short_payload, sizeof(short_payload)),
                     0);

    const char* argv[] = {BC_DUPLICATE_TEST_BINARY_PATH, "scan", base_directory, NULL};
    char stdout_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    char stderr_buffer[BC_DUPLICATE_TEST_OUTPUT_BUFFER_SIZE];
    int exit_status = -1;
    assert_int_equal(bc_duplicate_test_run(argv, stdout_buffer, sizeof(stdout_buffer), stderr_buffer, sizeof(stderr_buffer), &exit_status), 0);
    assert_int_equal(exit_status, 0);
    assert_non_null(strstr(stderr_buffer, "discovery: 4 file(s) found"));
    assert_non_null(strstr(stderr_buffer, "size groups: 1"));
    assert_non_null(strstr(stderr_buffer, "fast-hash groups: 1"));
}

int main(void)
{
    if (bc_duplicate_test_ensure_directory(BC_DUPLICATE_TEST_FIXTURES_DIRECTORY) != 0) {
        fprintf(stderr, "cannot create fixtures directory %s\n", BC_DUPLICATE_TEST_FIXTURES_DIRECTORY);
        return 1;
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_help_succeeds),
        cmocka_unit_test(test_discovery_finds_seeded_files),
        cmocka_unit_test(test_hidden_flag_includes_dotfiles),
        cmocka_unit_test(test_minimum_size_skips_small_files),
        cmocka_unit_test(test_exclude_pattern_skips_files),
        cmocka_unit_test(test_pseudo_filesystem_skipped),
        cmocka_unit_test(test_symlink_input_skipped),
        cmocka_unit_test(test_fast_hash_groups_identical_files),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
