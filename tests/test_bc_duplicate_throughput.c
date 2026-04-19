// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bc_allocators.h"
#include "bc_concurrency.h"
#include "bc_duplicate_throughput_internal.h"

static void test_measure_produces_positive_constants(void** state)
{
    (void)state;
    bc_allocators_context_config_t memory_config = {.tracking_enabled = false};
    bc_allocators_context_t* memory_context = NULL;
    assert_true(bc_allocators_context_create(&memory_config, &memory_context));

    bc_concurrency_config_t parallel_config = {0};
    bc_concurrency_context_t* concurrency_context = NULL;
    assert_true(bc_concurrency_create(memory_context, &parallel_config, &concurrency_context));

    bc_duplicate_throughput_constants_t constants;
    assert_true(bc_duplicate_throughput_measure(concurrency_context, &constants));
    assert_true(constants.xxh3_gigabytes_per_second > 0.0);
    assert_true(constants.xxh128_gigabytes_per_second > 0.0);
    assert_true(constants.sha256_gigabytes_per_second > 0.0);
    assert_true(constants.memory_bandwidth_gigabytes_per_second > 0.0);
    assert_true(constants.per_file_cost_warm_microseconds > 0.0);

    bc_concurrency_destroy(concurrency_context);
    bc_allocators_context_destroy(memory_context);
}

static void test_cache_round_trip(void** state)
{
    (void)state;
    char cache_path[] = "/tmp/bc_duplicate_throughput_cache_XXXXXX.txt";
    int fd = mkstemps(cache_path, 4);
    assert_true(fd >= 0);
    close(fd);

    bc_duplicate_throughput_constants_t expected = {
        .xxh3_gigabytes_per_second = 18.5,
        .xxh128_gigabytes_per_second = 17.9,
        .sha256_gigabytes_per_second = 1.6,
        .memory_bandwidth_gigabytes_per_second = 35.0,
        .parallel_startup_overhead_microseconds = 40.0,
        .per_file_cost_warm_microseconds = 3.5,
    };
    assert_true(bc_duplicate_throughput_cache_store(cache_path, &expected));

    bc_duplicate_throughput_constants_t loaded = {0};
    assert_true(bc_duplicate_throughput_cache_load(cache_path, &loaded));
    assert_true(loaded.xxh3_gigabytes_per_second > 18.4 && loaded.xxh3_gigabytes_per_second < 18.6);
    assert_true(loaded.sha256_gigabytes_per_second > 1.59 && loaded.sha256_gigabytes_per_second < 1.61);
    assert_true(loaded.memory_bandwidth_gigabytes_per_second > 34.99 && loaded.memory_bandwidth_gigabytes_per_second < 35.01);

    unlink(cache_path);
}

static void test_cache_load_missing_file_returns_false(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t loaded = {0};
    assert_false(bc_duplicate_throughput_cache_load("/tmp/bc_duplicate_throughput_does_not_exist_xyz.txt", &loaded));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_measure_produces_positive_constants),
        cmocka_unit_test(test_cache_round_trip),
        cmocka_unit_test(test_cache_load_missing_file_returns_false),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
