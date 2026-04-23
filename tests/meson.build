# SPDX-License-Identifier: MIT

cmocka_dep = dependency('cmocka')

fixtures_directory = meson.current_build_dir() / 'fixtures'

test_bc_duplicate_cli_spec_unit = executable('test_bc_duplicate_cli_spec_unit',
  'test_bc_duplicate_cli_spec_unit.c',
  '../src/cli/bc_duplicate_cli_spec.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep, bc_runtime_dep],
  c_args : [bc_duplicate_version_c_arg],
)
test('bc_duplicate_cli_spec_unit', test_bc_duplicate_cli_spec_unit)

test_bc_duplicate_filter_unit = executable('test_bc_duplicate_filter_unit',
  'test_bc_duplicate_filter_unit.c',
  '../src/filter/bc_duplicate_filter.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep],
)
test('bc_duplicate_filter_unit', test_bc_duplicate_filter_unit)

test_bc_duplicate_grouping_unit = executable('test_bc_duplicate_grouping_unit',
  'test_bc_duplicate_grouping_unit.c',
  '../src/grouping/bc_duplicate_grouping_by_size.c',
  '../src/grouping/bc_duplicate_grouping_by_fast_hash.c',
  '../src/grouping/bc_duplicate_grouping_by_full_hash.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep],
)
test('bc_duplicate_grouping_unit', test_bc_duplicate_grouping_unit)

test_bc_duplicate_dispatch_decision = executable('test_bc_duplicate_dispatch_decision',
  'test_bc_duplicate_dispatch_decision.c',
  bc_duplicate_dispatch_decision_sources,
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep, bc_concurrency_dep],
  link_args : ['-lm'],
)
test('bc_duplicate_dispatch_decision', test_bc_duplicate_dispatch_decision)

test_bc_duplicate_throughput = executable('test_bc_duplicate_throughput',
  'test_bc_duplicate_throughput.c',
  bc_duplicate_throughput_sources,
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep, bc_concurrency_dep, xxhash_dep],
)
test('bc_duplicate_throughput', test_bc_duplicate_throughput, timeout : 60)

test_bc_duplicate_output_unit = executable('test_bc_duplicate_output_unit',
  'test_bc_duplicate_output_unit.c',
  '../src/output/bc_duplicate_output_simple.c',
  '../src/output/bc_duplicate_output_json.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep],
)
test('bc_duplicate_output_unit', test_bc_duplicate_output_unit)

test_bc_duplicate_discovery_e2e = executable('test_bc_duplicate_discovery_e2e',
  'test_bc_duplicate_discovery_e2e.c',
  dependencies : [cmocka_dep],
  c_args : [
    '-DBC_DUPLICATE_TEST_BINARY_PATH="' + bc_duplicate_bin.full_path() + '"',
    '-DBC_DUPLICATE_TEST_FIXTURES_DIRECTORY="' + fixtures_directory + '"',
  ],
)
test('bc_duplicate_discovery_e2e', test_bc_duplicate_discovery_e2e, depends : [bc_duplicate_bin])

if liburing_dep.found()
  test_bc_duplicate_reader_ring_growth = executable('test_bc_duplicate_reader_ring_growth',
    'test_bc_duplicate_reader_ring_growth.c',
    '../src/worker/bc_duplicate_worker_batch_uring.c',
    '../src/worker/bc_duplicate_worker_reader.c',
    include_directories : [bc_duplicate_internal_inc],
    dependencies : [cmocka_dep, bc_core_dep, liburing_dep],
  )
  test('bc_duplicate_reader_ring_growth', test_bc_duplicate_reader_ring_growth, timeout : 30)
endif
