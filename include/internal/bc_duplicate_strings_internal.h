// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_STRINGS_INTERNAL_H
#define BC_DUPLICATE_STRINGS_INTERNAL_H

#include "bc_core.h"

#include <stdbool.h>
#include <stddef.h>

static inline size_t bc_duplicate_strings_length(const char* null_terminated_string)
{
    size_t length = 0;
    (void)bc_core_length(null_terminated_string, '\0', &length);
    return length;
}

static inline bool bc_duplicate_strings_equal(const char* left, const char* right)
{
    size_t left_length = bc_duplicate_strings_length(left);
    size_t right_length = bc_duplicate_strings_length(right);
    if (left_length != right_length) {
        return false;
    }
    bool result = false;
    (void)bc_core_equal(left, right, left_length, &result);
    return result;
}

#endif /* BC_DUPLICATE_STRINGS_INTERNAL_H */
