// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdbool.h>

#include "bc_duplicate_dispatch_decision_internal.h"

static bc_duplicate_throughput_constants_t make_constants(double xxh3, double sha256, double startup_us, double per_file_us)
{
    bc_duplicate_throughput_constants_t constants = {0};
    constants.xxh3_gigabytes_per_second = xxh3;
    constants.xxh128_gigabytes_per_second = xxh3;
    constants.sha256_gigabytes_per_second = sha256;
    constants.memory_bandwidth_gigabytes_per_second = 30.0;
    constants.parallel_startup_overhead_microseconds = startup_us;
    constants.per_file_cost_warm_microseconds = per_file_us;
    return constants;
}

static void test_zero_files_is_single(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t constants = make_constants(18.0, 1.7, 40.0, 3.5);
    assert_false(bc_duplicate_dispatch_decision_should_go_multithread(0, 0, BC_DUPLICATE_ALGORITHM_XXH3, &constants, 8));
}

static void test_single_worker_is_single(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t constants = make_constants(18.0, 1.7, 40.0, 3.5);
    assert_false(bc_duplicate_dispatch_decision_should_go_multithread(1000, (size_t)10 * 1024 * 1024 * 1024, BC_DUPLICATE_ALGORITHM_XXH3, &constants, 1));
}

static void test_tiny_workload_is_single(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t constants = make_constants(18.0, 1.7, 200.0, 3.5);
    assert_false(bc_duplicate_dispatch_decision_should_go_multithread(5, 50, BC_DUPLICATE_ALGORITHM_XXH3, &constants, 8));
}

static void test_large_workload_is_multi(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t constants = make_constants(18.0, 1.7, 40.0, 3.5);
    assert_true(bc_duplicate_dispatch_decision_should_go_multithread(100000, (size_t)5 * 1024 * 1024 * 1024, BC_DUPLICATE_ALGORITHM_XXH3, &constants, 8));
}

static void test_sha256_chooses_multi_sooner_than_xxh3(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t constants = make_constants(18.0, 1.7, 1000.0, 3.5);
    bool xxh3_decision = bc_duplicate_dispatch_decision_should_go_multithread(50, 5 * 1024 * 1024, BC_DUPLICATE_ALGORITHM_XXH3, &constants, 8);
    bool sha256_decision = bc_duplicate_dispatch_decision_should_go_multithread(50, 5 * 1024 * 1024, BC_DUPLICATE_ALGORITHM_SHA256, &constants, 8);
    assert_false(xxh3_decision);
    assert_true(sha256_decision);
}

static void test_zero_throughput_falls_back_to_multi(void** state)
{
    (void)state;
    bc_duplicate_throughput_constants_t constants = make_constants(0.0, 0.0, 40.0, 3.5);
    assert_true(bc_duplicate_dispatch_decision_should_go_multithread(100, 1000000, BC_DUPLICATE_ALGORITHM_XXH3, &constants, 8));
}

static void test_optimal_worker_count_memory_bound(void** state)
{
    (void)state;
    size_t workers = bc_duplicate_dispatch_decision_optimal_worker_count(2.0, 8.0, 8);
    assert_int_equal(workers, 4);
}

static void test_optimal_worker_count_compute_bound(void** state)
{
    (void)state;
    size_t workers = bc_duplicate_dispatch_decision_optimal_worker_count(0.5, 30.0, 4);
    assert_int_equal(workers, 3);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_zero_files_is_single),
        cmocka_unit_test(test_single_worker_is_single),
        cmocka_unit_test(test_tiny_workload_is_single),
        cmocka_unit_test(test_large_workload_is_multi),
        cmocka_unit_test(test_sha256_chooses_multi_sooner_than_xxh3),
        cmocka_unit_test(test_zero_throughput_falls_back_to_multi),
        cmocka_unit_test(test_optimal_worker_count_memory_bound),
        cmocka_unit_test(test_optimal_worker_count_compute_bound),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
