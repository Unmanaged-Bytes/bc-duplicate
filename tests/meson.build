# SPDX-License-Identifier: MIT

cmocka_dep = dependency('cmocka')

fixtures_directory = meson.current_build_dir() / 'fixtures'

test_bc_duplicate_cli_spec_unit = executable('test_bc_duplicate_cli_spec_unit',
  'test_bc_duplicate_cli_spec_unit.c',
  '../src/cli/bc_duplicate_cli_spec.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep, bc_runtime_dep],
)
test('bc_duplicate_cli_spec_unit', test_bc_duplicate_cli_spec_unit)

test_bc_duplicate_filter_unit = executable('test_bc_duplicate_filter_unit',
  'test_bc_duplicate_filter_unit.c',
  '../src/filter/bc_duplicate_filter.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep],
)
test('bc_duplicate_filter_unit', test_bc_duplicate_filter_unit)

test_bc_duplicate_discovery_e2e = executable('test_bc_duplicate_discovery_e2e',
  'test_bc_duplicate_discovery_e2e.c',
  dependencies : [cmocka_dep],
  c_args : [
    '-DBC_DUPLICATE_TEST_BINARY_PATH="' + bc_duplicate_bin.full_path() + '"',
    '-DBC_DUPLICATE_TEST_FIXTURES_DIRECTORY="' + fixtures_directory + '"',
  ],
)
test('bc_duplicate_discovery_e2e', test_bc_duplicate_discovery_e2e, depends : [bc_duplicate_bin])
