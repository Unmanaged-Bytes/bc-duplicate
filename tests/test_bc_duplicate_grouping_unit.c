// SPDX-License-Identifier: MIT

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>

#include <stdbool.h>
#include <string.h>

#include "bc_allocators.h"
#include "bc_allocators_pool.h"
#include "bc_duplicate_grouping_internal.h"

struct fixture {
    bc_allocators_context_t* memory_context;
};

static int setup(void** state)
{
    struct fixture* fixture = test_calloc(1, sizeof(*fixture));
    bc_allocators_context_config_t config = {.tracking_enabled = true};
    if (!bc_allocators_context_create(&config, &fixture->memory_context)) {
        test_free(fixture);
        return -1;
    }
    *state = fixture;
    return 0;
}

static int teardown(void** state)
{
    struct fixture* fixture = *state;
    bc_allocators_context_destroy(fixture->memory_context);
    test_free(fixture);
    return 0;
}

static void make_entry(bc_duplicate_file_entry_t* entry, size_t file_size, dev_t device, ino_t inode, uint64_t fast_hash)
{
    memset(entry, 0, sizeof(*entry));
    entry->file_size = file_size;
    entry->device_id = device;
    entry->inode_number = inode;
    entry->fast_hash = fast_hash;
}

static void test_by_size_zero_entries(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_group_t* groups = NULL;
    size_t group_count = 99;
    size_t candidate_count = 99;
    size_t hardlinks_collapsed = 99;
    assert_true(bc_duplicate_grouping_by_size(fixture->memory_context, NULL, 0, false, &groups, &group_count, &candidate_count,
                                              &hardlinks_collapsed));
    assert_null(groups);
    assert_int_equal(group_count, 0);
    assert_int_equal(candidate_count, 0);
    assert_int_equal(hardlinks_collapsed, 0);
}

static void test_by_size_no_duplicates(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[3];
    make_entry(&entries[0], 100, 1, 10, 0);
    make_entry(&entries[1], 200, 1, 11, 0);
    make_entry(&entries[2], 300, 1, 12, 0);
    bc_duplicate_group_t* groups = NULL;
    size_t group_count = 0;
    size_t candidate_count = 0;
    size_t hardlinks_collapsed = 0;
    assert_true(bc_duplicate_grouping_by_size(fixture->memory_context, entries, 3, false, &groups, &group_count, &candidate_count,
                                              &hardlinks_collapsed));
    assert_int_equal(group_count, 0);
    assert_int_equal(candidate_count, 0);
    assert_int_equal(hardlinks_collapsed, 0);
    if (groups != NULL) {
        bc_allocators_pool_free(fixture->memory_context, groups);
    }
}

static void test_by_size_groups_two_pairs(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[5];
    make_entry(&entries[0], 100, 1, 10, 0);
    make_entry(&entries[1], 200, 1, 11, 0);
    make_entry(&entries[2], 100, 1, 12, 0);
    make_entry(&entries[3], 200, 1, 13, 0);
    make_entry(&entries[4], 300, 1, 14, 0);
    bc_duplicate_group_t* groups = NULL;
    size_t group_count = 0;
    size_t candidate_count = 0;
    size_t hardlinks_collapsed = 0;
    assert_true(bc_duplicate_grouping_by_size(fixture->memory_context, entries, 5, false, &groups, &group_count, &candidate_count,
                                              &hardlinks_collapsed));
    assert_int_equal(group_count, 2);
    assert_int_equal(candidate_count, 4);
    assert_int_equal(hardlinks_collapsed, 0);
    assert_int_equal(groups[0].file_size, 200);
    assert_int_equal(groups[0].entry_count, 2);
    assert_int_equal(groups[1].file_size, 100);
    assert_int_equal(groups[1].entry_count, 2);
    bc_allocators_pool_free(fixture->memory_context, groups);
}

static void test_by_size_collapses_hardlinks_by_default(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[4];
    make_entry(&entries[0], 100, 1, 10, 0);
    make_entry(&entries[1], 100, 1, 10, 0);
    make_entry(&entries[2], 100, 1, 10, 0);
    make_entry(&entries[3], 100, 1, 11, 0);
    bc_duplicate_group_t* groups = NULL;
    size_t group_count = 0;
    size_t candidate_count = 0;
    size_t hardlinks_collapsed = 0;
    assert_true(bc_duplicate_grouping_by_size(fixture->memory_context, entries, 4, false, &groups, &group_count, &candidate_count,
                                              &hardlinks_collapsed));
    assert_int_equal(hardlinks_collapsed, 2);
    assert_int_equal(group_count, 1);
    assert_int_equal(candidate_count, 2);
    assert_int_equal(groups[0].entry_count, 2);
    bc_allocators_pool_free(fixture->memory_context, groups);
}

static void test_by_size_keeps_hardlinks_when_match_enabled(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[4];
    make_entry(&entries[0], 100, 1, 10, 0);
    make_entry(&entries[1], 100, 1, 10, 0);
    make_entry(&entries[2], 100, 1, 10, 0);
    make_entry(&entries[3], 100, 1, 11, 0);
    bc_duplicate_group_t* groups = NULL;
    size_t group_count = 0;
    size_t candidate_count = 0;
    size_t hardlinks_collapsed = 0;
    assert_true(bc_duplicate_grouping_by_size(fixture->memory_context, entries, 4, true, &groups, &group_count, &candidate_count,
                                              &hardlinks_collapsed));
    assert_int_equal(hardlinks_collapsed, 0);
    assert_int_equal(group_count, 1);
    assert_int_equal(candidate_count, 4);
    assert_int_equal(groups[0].entry_count, 4);
    bc_allocators_pool_free(fixture->memory_context, groups);
}

static void test_by_size_collapse_singleton_drops_group(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[3];
    make_entry(&entries[0], 100, 1, 10, 0);
    make_entry(&entries[1], 100, 1, 10, 0);
    make_entry(&entries[2], 200, 1, 11, 0);
    bc_duplicate_group_t* groups = NULL;
    size_t group_count = 0;
    size_t candidate_count = 0;
    size_t hardlinks_collapsed = 0;
    assert_true(bc_duplicate_grouping_by_size(fixture->memory_context, entries, 3, false, &groups, &group_count, &candidate_count,
                                              &hardlinks_collapsed));
    assert_int_equal(hardlinks_collapsed, 1);
    assert_int_equal(group_count, 0);
    assert_int_equal(candidate_count, 0);
    if (groups != NULL) {
        bc_allocators_pool_free(fixture->memory_context, groups);
    }
}

static void test_by_fast_hash_splits_size_group(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[5];
    make_entry(&entries[0], 100, 1, 10, 0xAAAA);
    make_entry(&entries[1], 100, 1, 11, 0xBBBB);
    make_entry(&entries[2], 100, 1, 12, 0xAAAA);
    make_entry(&entries[3], 100, 1, 13, 0xCCCC);
    make_entry(&entries[4], 100, 1, 14, 0xBBBB);
    bc_duplicate_group_t size_group = {.start_index = 0, .entry_count = 5, .file_size = 100};

    bc_duplicate_group_t* fast_groups = NULL;
    size_t fast_group_count = 0;
    size_t candidate_count = 0;
    assert_true(bc_duplicate_grouping_by_fast_hash(fixture->memory_context, entries, &size_group, 1, &fast_groups, &fast_group_count,
                                                   &candidate_count));
    assert_int_equal(fast_group_count, 2);
    assert_int_equal(candidate_count, 4);
    assert_int_equal(fast_groups[0].entry_count, 2);
    assert_int_equal(fast_groups[1].entry_count, 2);
    bc_allocators_pool_free(fixture->memory_context, fast_groups);
}

static void test_by_fast_hash_drops_unique_singletons(void** state)
{
    struct fixture* fixture = *state;
    bc_duplicate_file_entry_t entries[3];
    make_entry(&entries[0], 100, 1, 10, 0xAAAA);
    make_entry(&entries[1], 100, 1, 11, 0xBBBB);
    make_entry(&entries[2], 100, 1, 12, 0xCCCC);
    bc_duplicate_group_t size_group = {.start_index = 0, .entry_count = 3, .file_size = 100};

    bc_duplicate_group_t* fast_groups = NULL;
    size_t fast_group_count = 0;
    size_t candidate_count = 0;
    assert_true(bc_duplicate_grouping_by_fast_hash(fixture->memory_context, entries, &size_group, 1, &fast_groups, &fast_group_count,
                                                   &candidate_count));
    assert_int_equal(fast_group_count, 0);
    assert_int_equal(candidate_count, 0);
    if (fast_groups != NULL) {
        bc_allocators_pool_free(fixture->memory_context, fast_groups);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_by_size_zero_entries, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_size_no_duplicates, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_size_groups_two_pairs, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_size_collapses_hardlinks_by_default, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_size_keeps_hardlinks_when_match_enabled, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_size_collapse_singleton_drops_group, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_fast_hash_splits_size_group, setup, teardown),
        cmocka_unit_test_setup_teardown(test_by_fast_hash_drops_unique_singletons, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
