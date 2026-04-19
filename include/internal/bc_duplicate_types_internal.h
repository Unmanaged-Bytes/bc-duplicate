// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_TYPES_INTERNAL_H
#define BC_DUPLICATE_TYPES_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define BC_DUPLICATE_FAST_HASH_BLOCK_SIZE 4096
#define BC_DUPLICATE_XXH3_DIGEST_SIZE     8
#define BC_DUPLICATE_XXH3_HEX_LENGTH      16
#define BC_DUPLICATE_XXH128_DIGEST_SIZE   16
#define BC_DUPLICATE_XXH128_HEX_LENGTH    32
#define BC_DUPLICATE_SHA256_DIGEST_SIZE   32
#define BC_DUPLICATE_SHA256_HEX_LENGTH    64
#define BC_DUPLICATE_MAX_DIGEST_SIZE      BC_DUPLICATE_SHA256_DIGEST_SIZE
#define BC_DUPLICATE_MAX_HEX_LENGTH       BC_DUPLICATE_SHA256_HEX_LENGTH

typedef enum {
    BC_DUPLICATE_ALGORITHM_XXH3 = 0,
    BC_DUPLICATE_ALGORITHM_XXH128 = 1,
    BC_DUPLICATE_ALGORITHM_SHA256 = 2,
} bc_duplicate_algorithm_t;

typedef enum {
    BC_DUPLICATE_OUTPUT_FORMAT_SIMPLE = 0,
    BC_DUPLICATE_OUTPUT_FORMAT_JSON = 1,
} bc_duplicate_output_format_t;

typedef enum {
    BC_DUPLICATE_THREADS_MODE_AUTO = 0,
    BC_DUPLICATE_THREADS_MODE_MONO = 1,
    BC_DUPLICATE_THREADS_MODE_EXPLICIT = 2,
} bc_duplicate_threads_mode_t;

typedef enum {
    BC_DUPLICATE_OUTPUT_DESTINATION_AUTO = 0,
    BC_DUPLICATE_OUTPUT_DESTINATION_STDOUT = 1,
    BC_DUPLICATE_OUTPUT_DESTINATION_FILE = 2,
} bc_duplicate_output_destination_mode_t;

typedef enum {
    BC_DUPLICATE_COMMAND_SCAN = 0,
    BC_DUPLICATE_COMMAND_SUMMARY = 1,
} bc_duplicate_command_t;

typedef struct bc_duplicate_cli_options {
    bc_duplicate_command_t command;
    bc_duplicate_algorithm_t algorithm;
    bc_duplicate_threads_mode_t threads_mode;
    size_t explicit_worker_count;
    size_t minimum_file_size;
    bc_duplicate_output_destination_mode_t output_destination_mode;
    const char* output_destination_path;
    const char* include_list;
    const char* exclude_list;
    bool include_hidden;
    bool follow_symlinks;
    bool match_hardlinks;
    bool one_file_system;
    int positional_argument_count;
    const char* const* positional_argument_values;
} bc_duplicate_cli_options_t;

typedef struct bc_duplicate_file_entry {
    char* absolute_path;
    size_t absolute_path_length;
    size_t file_size;
    dev_t device_id;
    ino_t inode_number;
    uint64_t fast_hash;
    uint8_t full_hash[BC_DUPLICATE_MAX_DIGEST_SIZE];
    bool fast_hash_computed;
    bool full_hash_computed;
    int fast_hash_errno;
    int full_hash_errno;
} bc_duplicate_file_entry_t;

typedef struct bc_duplicate_group {
    size_t start_index;
    size_t entry_count;
    size_t file_size;
} bc_duplicate_group_t;

typedef struct bc_duplicate_statistics {
    size_t files_scanned;
    size_t directories_scanned;
    size_t files_skipped;
    size_t hardlinks_collapsed;
    size_t size_candidate_count;
    size_t files_hashed_fast;
    size_t files_hashed_full;
    size_t duplicate_group_count;
    size_t duplicate_file_count;
    size_t wasted_bytes;
    uint64_t wall_ms;
} bc_duplicate_statistics_t;

#endif /* BC_DUPLICATE_TYPES_INTERNAL_H */
