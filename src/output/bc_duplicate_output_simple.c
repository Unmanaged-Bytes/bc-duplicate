// SPDX-License-Identifier: MIT

#include "bc_duplicate_output_internal.h"

#include "bc_core.h"

bool bc_duplicate_output_simple_write(bc_core_writer_t* writer, const bc_duplicate_file_entry_t* entries,
                                      const bc_duplicate_group_t* groups, size_t group_count)
{
    for (size_t group_index = 0; group_index < group_count; ++group_index) {
        const bc_duplicate_group_t* group = &groups[group_index];
        for (size_t entry_offset = 0; entry_offset < group->entry_count; ++entry_offset) {
            const bc_duplicate_file_entry_t* entry = &entries[group->start_index + entry_offset];
            size_t path_length = 0;
            if (!bc_core_length(entry->absolute_path, 0, &path_length)) {
                return false;
            }
            if (!bc_core_writer_write_bytes(writer, entry->absolute_path, path_length)) {
                return false;
            }
            if (!bc_core_writer_write_char(writer, '\n')) {
                return false;
            }
        }
        if (group_index + 1 < group_count) {
            if (!bc_core_writer_write_char(writer, '\n')) {
                return false;
            }
        }
    }
    return true;
}

static bool write_labeled_uint(bc_core_writer_t* writer, const char* label, size_t label_length, uint64_t value)
{
    if (!bc_core_writer_write_bytes(writer, label, label_length)) {
        return false;
    }
    if (!bc_core_writer_write_unsigned_integer_64_decimal(writer, value)) {
        return false;
    }
    return bc_core_writer_write_char(writer, '\n');
}

bool bc_duplicate_output_summary_write(bc_core_writer_t* writer, const bc_duplicate_statistics_t* statistics)
{
    if (!write_labeled_uint(writer, "Files scanned:        ", 22U, (uint64_t)statistics->files_scanned)) return false;
    if (!write_labeled_uint(writer, "Directories scanned:  ", 22U, (uint64_t)statistics->directories_scanned)) return false;
    if (!write_labeled_uint(writer, "Files skipped:        ", 22U, (uint64_t)statistics->files_skipped)) return false;
    if (!write_labeled_uint(writer, "Hardlinks collapsed:  ", 22U, (uint64_t)statistics->hardlinks_collapsed)) return false;
    if (!write_labeled_uint(writer, "Size candidates:      ", 22U, (uint64_t)statistics->size_candidate_count)) return false;
    if (!write_labeled_uint(writer, "Files hashed (fast):  ", 22U, (uint64_t)statistics->files_hashed_fast)) return false;
    if (!write_labeled_uint(writer, "Files hashed (full):  ", 22U, (uint64_t)statistics->files_hashed_full)) return false;
    if (!write_labeled_uint(writer, "Duplicate groups:     ", 22U, (uint64_t)statistics->duplicate_group_count)) return false;
    if (!write_labeled_uint(writer, "Duplicate files:      ", 22U, (uint64_t)statistics->duplicate_file_count)) return false;
    if (!write_labeled_uint(writer, "Wasted bytes:         ", 22U, (uint64_t)statistics->wasted_bytes)) return false;
    if (!write_labeled_uint(writer, "Wall ms:              ", 22U, statistics->wall_ms)) return false;
    return true;
}
