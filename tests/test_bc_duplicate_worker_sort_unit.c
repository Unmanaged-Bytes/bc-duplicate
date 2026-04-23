// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bc_duplicate_worker_sort_internal.h"
#include "bc_duplicate_types_internal.h"

static void test_sort_places_largest_first(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t entries[5] = {0};
    entries[0].file_size = 1024U;
    entries[1].file_size = 8U;
    entries[2].file_size = 4096U;
    entries[3].file_size = 512U;
    entries[4].file_size = 2048U;

    size_t candidate_indices[5] = {0U, 1U, 2U, 3U, 4U};
    bc_duplicate_worker_sort_indices_by_size_desc(entries, candidate_indices, 5U);

    assert_int_equal(entries[candidate_indices[0]].file_size, 4096U);
    assert_int_equal(entries[candidate_indices[1]].file_size, 2048U);
    assert_int_equal(entries[candidate_indices[2]].file_size, 1024U);
    assert_int_equal(entries[candidate_indices[3]].file_size, 512U);
    assert_int_equal(entries[candidate_indices[4]].file_size, 8U);
}

static void test_sort_is_stable_on_equal_sizes(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t entries[4] = {0};
    entries[0].file_size = 100U;
    entries[1].file_size = 100U;
    entries[2].file_size = 100U;
    entries[3].file_size = 100U;

    size_t candidate_indices[4] = {0U, 1U, 2U, 3U};
    bc_duplicate_worker_sort_indices_by_size_desc(entries, candidate_indices, 4U);

    for (size_t index = 0U; index < 4U; ++index) {
        assert_int_equal(entries[candidate_indices[index]].file_size, 100U);
    }
}

static void test_sort_handles_zero_count(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t dummy_entry = {0};
    size_t indices[1] = {0U};
    bc_duplicate_worker_sort_indices_by_size_desc(&dummy_entry, indices, 0U);
}

static void test_sort_handles_single_element(void** state)
{
    (void)state;
    bc_duplicate_file_entry_t entries[1] = {0};
    entries[0].file_size = 42U;
    size_t indices[1] = {0U};
    bc_duplicate_worker_sort_indices_by_size_desc(entries, indices, 1U);
    assert_int_equal(indices[0], 0U);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sort_places_largest_first),
        cmocka_unit_test(test_sort_is_stable_on_equal_sizes),
        cmocka_unit_test(test_sort_handles_zero_count),
        cmocka_unit_test(test_sort_handles_single_element),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
