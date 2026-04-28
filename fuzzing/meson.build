# SPDX-License-Identifier: MIT

fuzzer_c_args = []
fuzzer_link_args = []
if cc.get_id() == 'clang'
    fuzzer_c_args += ['-fsanitize=fuzzer', '-DBC_FUZZ_LIBFUZZER']
    fuzzer_link_args += ['-fsanitize=fuzzer']
endif

bc_duplicate_fuzz_cli_sources = [
    '../src/cli/bc_duplicate_cli_spec.c',
    '../src/filter/bc_duplicate_filter.c',
]

bc_duplicate_fuzz_filter_sources = [
    '../src/filter/bc_duplicate_filter.c',
]

executable('fuzz_bc_duplicate_cli_parse',
    'fuzz_bc_duplicate_cli_parse.c',
    bc_duplicate_fuzz_cli_sources,
    include_directories : [bc_duplicate_internal_inc],
    dependencies : bc_duplicate_deps,
    c_args : fuzzer_c_args + [bc_duplicate_version_c_arg],
    link_args : fuzzer_link_args,
)

executable('fuzz_bc_duplicate_filter',
    'fuzz_bc_duplicate_filter.c',
    bc_duplicate_fuzz_filter_sources,
    include_directories : [bc_duplicate_internal_inc],
    dependencies : bc_duplicate_deps,
    c_args : fuzzer_c_args,
    link_args : fuzzer_link_args,
)
