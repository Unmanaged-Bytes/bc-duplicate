// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_CLI_INTERNAL_H
#define BC_DUPLICATE_CLI_INTERNAL_H

#include "bc_duplicate_types_internal.h"

#include "bc_runtime.h"
#include "bc_runtime_cli.h"

#include <stdbool.h>

const bc_runtime_cli_program_spec_t* bc_duplicate_cli_program_spec(void);

bool bc_duplicate_cli_bind_options(const bc_runtime_config_store_t* store, const bc_runtime_cli_parsed_t* parsed,
                                   bc_duplicate_cli_options_t* out_options);

bool bc_duplicate_cli_bind_global_threads(const bc_runtime_config_store_t* store, bc_duplicate_threads_mode_t* out_mode,
                                          size_t* out_explicit_worker_count);

#endif /* BC_DUPLICATE_CLI_INTERNAL_H */
