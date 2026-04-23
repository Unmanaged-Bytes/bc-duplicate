// SPDX-License-Identifier: MIT

#include "bc_duplicate_output_internal.h"

#include "bc_core.h"

#include <string.h>

#define BC_DUPLICATE_OUTPUT_JSON_VERSION "1.0.0"
#define BC_DUPLICATE_OUTPUT_JSON_TOOL    "bc-duplicate"

static bool bc_duplicate_output_json_write_escaped(bc_core_writer_t* writer, const char* input)
{
    if (!bc_core_writer_write_char(writer, '"')) return false;
    const unsigned char* cursor = (const unsigned char*)input;
    while (*cursor != 0) {
        unsigned char byte = *cursor;
        bool ok = true;
        switch (byte) {
        case '"':
            ok = BC_CORE_WRITER_PUTS(writer, "\\\"");
            break;
        case '\\':
            ok = BC_CORE_WRITER_PUTS(writer, "\\\\");
            break;
        case '\b':
            ok = BC_CORE_WRITER_PUTS(writer, "\\b");
            break;
        case '\f':
            ok = BC_CORE_WRITER_PUTS(writer, "\\f");
            break;
        case '\n':
            ok = BC_CORE_WRITER_PUTS(writer, "\\n");
            break;
        case '\r':
            ok = BC_CORE_WRITER_PUTS(writer, "\\r");
            break;
        case '\t':
            ok = BC_CORE_WRITER_PUTS(writer, "\\t");
            break;
        default:
            if (byte < 0x20u) {
                ok = BC_CORE_WRITER_PUTS(writer, "\\u")
                     && bc_core_writer_write_uint64_hex_padded(writer, (uint64_t)byte, 4U);
            } else {
                ok = bc_core_writer_write_char(writer, (char)byte);
            }
            break;
        }
        if (!ok) return false;
        cursor++;
    }
    return bc_core_writer_write_char(writer, '"');
}

static const char* bc_duplicate_output_json_algorithm_name(bc_duplicate_algorithm_t algorithm)
{
    switch (algorithm) {
    case BC_DUPLICATE_ALGORITHM_XXH3:
        return "xxh3";
    case BC_DUPLICATE_ALGORITHM_XXH128:
        return "xxh128";
    case BC_DUPLICATE_ALGORITHM_SHA256:
        return "sha256";
    }
    return "unknown";
}

static bool write_stat_key_value(bc_core_writer_t* writer, const char* key, size_t key_length, uint64_t value)
{
    if (!bc_core_writer_write_char(writer, '"')) return false;
    if (!bc_core_writer_write_bytes(writer, key, key_length)) return false;
    if (!BC_CORE_WRITER_PUTS(writer, "\":")) return false;
    return bc_core_writer_write_uint64_dec(writer, value);
}

static bool bc_duplicate_output_json_write_stats(bc_core_writer_t* writer, const bc_duplicate_statistics_t* statistics)
{
    if (!BC_CORE_WRITER_PUTS(writer, "\"stats\":{")) return false;
    if (!write_stat_key_value(writer, "files_scanned", 13U, (uint64_t)statistics->files_scanned)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "directories_scanned", 19U, (uint64_t)statistics->directories_scanned)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "files_skipped", 13U, (uint64_t)statistics->files_skipped)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "hardlinks_collapsed", 19U, (uint64_t)statistics->hardlinks_collapsed)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "size_candidates", 15U, (uint64_t)statistics->size_candidate_count)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "files_hashed_fast", 17U, (uint64_t)statistics->files_hashed_fast)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "files_hashed_full", 17U, (uint64_t)statistics->files_hashed_full)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "duplicate_groups", 16U, (uint64_t)statistics->duplicate_group_count)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "duplicate_files", 15U, (uint64_t)statistics->duplicate_file_count)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "wasted_bytes", 12U, (uint64_t)statistics->wasted_bytes)) return false;
    if (!bc_core_writer_write_char(writer, ',')) return false;
    if (!write_stat_key_value(writer, "wall_ms", 7U, statistics->wall_ms)) return false;
    return bc_core_writer_write_char(writer, '}');
}

bool bc_duplicate_output_json_write(bc_core_writer_t* writer, bc_duplicate_algorithm_t algorithm, const bc_duplicate_file_entry_t* entries,
                                    const bc_duplicate_group_t* groups, size_t group_count, const bc_duplicate_statistics_t* statistics)
{
    if (!BC_CORE_WRITER_PUTS(writer, "{\"version\":\"" BC_DUPLICATE_OUTPUT_JSON_VERSION "\",\"tool\":\"" BC_DUPLICATE_OUTPUT_JSON_TOOL "\",\"algorithm\":\"")) return false;
    const char* algorithm_name = bc_duplicate_output_json_algorithm_name(algorithm);
    size_t algorithm_name_length = 0;
    if (!bc_core_length(algorithm_name, 0, &algorithm_name_length)) return false;
    if (!bc_core_writer_write_bytes(writer, algorithm_name, algorithm_name_length)) return false;
    if (!BC_CORE_WRITER_PUTS(writer, "\",")) return false;

    if (!bc_duplicate_output_json_write_stats(writer, statistics)) return false;
    if (!BC_CORE_WRITER_PUTS(writer, ",\"groups\":[")) return false;

    for (size_t group_index = 0; group_index < group_count; ++group_index) {
        const bc_duplicate_group_t* group = &groups[group_index];
        if (group_index > 0) {
            if (!bc_core_writer_write_char(writer, ',')) return false;
        }
        if (!BC_CORE_WRITER_PUTS(writer, "{\"size\":")) return false;
        if (!bc_core_writer_write_uint64_dec(writer, (uint64_t)group->file_size)) return false;
        if (!BC_CORE_WRITER_PUTS(writer, ",\"files\":[")) return false;
        for (size_t entry_offset = 0; entry_offset < group->entry_count; ++entry_offset) {
            if (entry_offset > 0) {
                if (!bc_core_writer_write_char(writer, ',')) return false;
            }
            if (!bc_duplicate_output_json_write_escaped(writer, entries[group->start_index + entry_offset].absolute_path)) return false;
        }
        if (!BC_CORE_WRITER_PUTS(writer, "]}")) return false;
    }
    return BC_CORE_WRITER_PUTS(writer, "]}\n");
}
