# SPDX-License-Identifier: MIT

bc_duplicate_throughput_sources = files(
  'bench/bc_duplicate_throughput.c',
  'bench/bc_duplicate_throughput_cache.c',
)

bc_duplicate_dispatch_decision_sources = files(
  'bench/bc_duplicate_dispatch_decision.c',
)

bc_duplicate_sources = [
  'bc_duplicate_main.c',
  'cli/bc_duplicate_cli_spec.c',
  'discovery/bc_duplicate_discovery_glob.c',
  'discovery/bc_duplicate_discovery_walk.c',
  'discovery/bc_duplicate_discovery_walk_parallel.c',
  'filter/bc_duplicate_filter.c',
  'grouping/bc_duplicate_grouping_by_size.c',
  'grouping/bc_duplicate_grouping_by_fast_hash.c',
  'grouping/bc_duplicate_grouping_by_full_hash.c',
  'output/bc_duplicate_output_simple.c',
  'output/bc_duplicate_output_json.c',
  'worker/bc_duplicate_worker_batch_uring.c',
  'worker/bc_duplicate_worker_fast.c',
  'worker/bc_duplicate_worker_full.c',
  'worker/bc_duplicate_worker_reader.c',
  'worker/bc_duplicate_worker_sort.c',
  bc_duplicate_throughput_sources,
  bc_duplicate_dispatch_decision_sources,
]

cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required : true)

bc_duplicate_version_c_arg = '-DBC_DUPLICATE_VERSION_STRING="@0@"'.format(meson.project_version())

bc_duplicate_bin = executable('bc-duplicate',
  bc_duplicate_sources,
  include_directories : [bc_duplicate_internal_inc],
  dependencies : bc_duplicate_deps + [libm_dep],
  c_args : lto_c_args + [bc_duplicate_version_c_arg],
  link_args : lto_link_args,
  install : true,
)
