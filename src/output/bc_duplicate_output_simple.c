// SPDX-License-Identifier: MIT

#include "bc_duplicate_output_internal.h"

#include <stdio.h>

bool bc_duplicate_output_simple_write(FILE* stream, const bc_duplicate_file_entry_t* entries, const bc_duplicate_group_t* groups,
                                      size_t group_count)
{
    for (size_t group_index = 0; group_index < group_count; ++group_index) {
        const bc_duplicate_group_t* group = &groups[group_index];
        for (size_t entry_offset = 0; entry_offset < group->entry_count; ++entry_offset) {
            const bc_duplicate_file_entry_t* entry = &entries[group->start_index + entry_offset];
            if (fputs(entry->absolute_path, stream) == EOF) {
                return false;
            }
            if (fputc('\n', stream) == EOF) {
                return false;
            }
        }
        if (group_index + 1 < group_count) {
            if (fputc('\n', stream) == EOF) {
                return false;
            }
        }
    }
    return true;
}

bool bc_duplicate_output_summary_write(FILE* stream, const bc_duplicate_statistics_t* statistics)
{
    int written = fprintf(stream,
                          "Files scanned:        %zu\n"
                          "Directories scanned:  %zu\n"
                          "Files skipped:        %zu\n"
                          "Hardlinks collapsed:  %zu\n"
                          "Size candidates:      %zu\n"
                          "Files hashed (fast):  %zu\n"
                          "Files hashed (full):  %zu\n"
                          "Duplicate groups:     %zu\n"
                          "Duplicate files:      %zu\n"
                          "Wasted bytes:         %zu\n"
                          "Wall ms:              %llu\n",
                          statistics->files_scanned, statistics->directories_scanned, statistics->files_skipped, statistics->hardlinks_collapsed,
                          statistics->size_candidate_count, statistics->files_hashed_fast, statistics->files_hashed_full,
                          statistics->duplicate_group_count, statistics->duplicate_file_count, statistics->wasted_bytes,
                          (unsigned long long)statistics->wall_ms);
    return written > 0;
}
