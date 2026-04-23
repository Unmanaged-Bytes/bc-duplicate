// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_OUTPUT_INTERNAL_H
#define BC_DUPLICATE_OUTPUT_INTERNAL_H

#include "bc_duplicate_types_internal.h"
#include "bc_core_io.h"

#include <stdbool.h>
#include <stddef.h>

bool bc_duplicate_output_simple_write(bc_core_writer_t* writer, const bc_duplicate_file_entry_t* entries,
                                      const bc_duplicate_group_t* groups, size_t group_count);

bool bc_duplicate_output_summary_write(bc_core_writer_t* writer, const bc_duplicate_statistics_t* statistics);

bool bc_duplicate_output_json_write(bc_core_writer_t* writer, bc_duplicate_algorithm_t algorithm, const bc_duplicate_file_entry_t* entries,
                                    const bc_duplicate_group_t* groups, size_t group_count, const bc_duplicate_statistics_t* statistics);

#endif /* BC_DUPLICATE_OUTPUT_INTERNAL_H */
