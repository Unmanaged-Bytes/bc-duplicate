# SPDX-License-Identifier: MIT

bc_duplicate_sources = [
  'bc_duplicate_main.c',
  'cli/bc_duplicate_cli_spec.c',
]

cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required : true)

bc_duplicate_bin = executable('bc-duplicate',
  bc_duplicate_sources,
  include_directories : [bc_duplicate_internal_inc],
  dependencies : bc_duplicate_deps + [libm_dep],
  c_args : lto_c_args,
  link_args : lto_link_args,
  install : true,
)
