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
  'error/bc_duplicate_error_collector.c',
  'filter/bc_duplicate_filter.c',
  'grouping/bc_duplicate_grouping_by_size.c',
  'grouping/bc_duplicate_grouping_by_fast_hash.c',
  'grouping/bc_duplicate_grouping_by_full_hash.c',
  'output/bc_duplicate_output_simple.c',
  'output/bc_duplicate_output_json.c',
  'worker/bc_duplicate_worker_fast.c',
  'worker/bc_duplicate_worker_full.c',
  bc_duplicate_throughput_sources,
  bc_duplicate_dispatch_decision_sources,
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
