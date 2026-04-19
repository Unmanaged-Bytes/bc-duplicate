# SPDX-License-Identifier: MIT

cmocka_dep = dependency('cmocka')

test_bc_duplicate_cli_spec_unit = executable('test_bc_duplicate_cli_spec_unit',
  'test_bc_duplicate_cli_spec_unit.c',
  '../src/cli/bc_duplicate_cli_spec.c',
  include_directories : [bc_duplicate_internal_inc],
  dependencies : [cmocka_dep, bc_core_dep, bc_allocators_dep, bc_runtime_dep],
)
test('bc_duplicate_cli_spec_unit', test_bc_duplicate_cli_spec_unit)
