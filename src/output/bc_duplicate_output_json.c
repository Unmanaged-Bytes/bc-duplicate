// SPDX-License-Identifier: MIT

#include "bc_duplicate_output_internal.h"

#include <inttypes.h>
#include <stdio.h>

#define BC_DUPLICATE_OUTPUT_JSON_VERSION "1.0.0"
#define BC_DUPLICATE_OUTPUT_JSON_TOOL    "bc-duplicate"

static void bc_duplicate_output_json_write_escaped(FILE* stream, const char* input)
{
    fputc('"', stream);
    const unsigned char* cursor = (const unsigned char*)input;
    while (*cursor != 0) {
        unsigned char byte = *cursor;
        switch (byte) {
        case '"':
            fputs("\\\"", stream);
            break;
        case '\\':
            fputs("\\\\", stream);
            break;
        case '\b':
            fputs("\\b", stream);
            break;
        case '\f':
            fputs("\\f", stream);
            break;
        case '\n':
            fputs("\\n", stream);
            break;
        case '\r':
            fputs("\\r", stream);
            break;
        case '\t':
            fputs("\\t", stream);
            break;
        default:
            if (byte < 0x20u) {
                fprintf(stream, "\\u%04x", (unsigned int)byte);
            } else {
                fputc((int)byte, stream);
            }
            break;
        }
        cursor++;
    }
    fputc('"', stream);
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

static void bc_duplicate_output_json_write_stats(FILE* stream, const bc_duplicate_statistics_t* statistics)
{
    fprintf(stream,
            "\"stats\":{"
            "\"files_scanned\":%zu,"
            "\"directories_scanned\":%zu,"
            "\"files_skipped\":%zu,"
            "\"hardlinks_collapsed\":%zu,"
            "\"size_candidates\":%zu,"
            "\"files_hashed_fast\":%zu,"
            "\"files_hashed_full\":%zu,"
            "\"duplicate_groups\":%zu,"
            "\"duplicate_files\":%zu,"
            "\"wasted_bytes\":%zu,"
            "\"wall_ms\":%" PRIu64 "}",
            statistics->files_scanned, statistics->directories_scanned, statistics->files_skipped, statistics->hardlinks_collapsed,
            statistics->size_candidate_count, statistics->files_hashed_fast, statistics->files_hashed_full, statistics->duplicate_group_count,
            statistics->duplicate_file_count, statistics->wasted_bytes, statistics->wall_ms);
}

bool bc_duplicate_output_json_write(FILE* stream, bc_duplicate_algorithm_t algorithm, const bc_duplicate_file_entry_t* entries,
                                    const bc_duplicate_group_t* groups, size_t group_count, const bc_duplicate_statistics_t* statistics)
{
    fputc('{', stream);
    fputs("\"version\":\"" BC_DUPLICATE_OUTPUT_JSON_VERSION "\",", stream);
    fputs("\"tool\":\"" BC_DUPLICATE_OUTPUT_JSON_TOOL "\",", stream);
    fprintf(stream, "\"algorithm\":\"%s\",", bc_duplicate_output_json_algorithm_name(algorithm));
    bc_duplicate_output_json_write_stats(stream, statistics);
    fputs(",\"groups\":[", stream);
    for (size_t group_index = 0; group_index < group_count; ++group_index) {
        const bc_duplicate_group_t* group = &groups[group_index];
        if (group_index > 0) {
            fputc(',', stream);
        }
        fprintf(stream, "{\"size\":%zu,\"files\":[", group->file_size);
        for (size_t entry_offset = 0; entry_offset < group->entry_count; ++entry_offset) {
            if (entry_offset > 0) {
                fputc(',', stream);
            }
            bc_duplicate_output_json_write_escaped(stream, entries[group->start_index + entry_offset].absolute_path);
        }
        fputs("]}", stream);
    }
    fputs("]}\n", stream);
    return ferror(stream) == 0;
}
