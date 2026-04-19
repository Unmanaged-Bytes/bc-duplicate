// SPDX-License-Identifier: MIT

#include "bc_duplicate_reader_internal.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct bc_duplicate_reader_ring {
    char unused_byte;
} bc_duplicate_reader_ring_t;

size_t bc_duplicate_reader_ring_struct_size(void)
{
    return sizeof(bc_duplicate_reader_ring_t);
}

bool bc_duplicate_reader_ring_init(bc_duplicate_reader_ring_t* ring)
{
    (void)ring;
    return true;
}

void bc_duplicate_reader_ring_destroy(bc_duplicate_reader_ring_t* ring)
{
    (void)ring;
}

bool bc_duplicate_reader_consume_batch(bc_duplicate_reader_ring_t* ring, bc_duplicate_reader_batch_item_t* items, size_t item_count,
                                       bc_duplicate_reader_consumer_fn_t consumer_function)
{
    (void)ring;
    for (size_t index = 0; index < item_count; ++index) {
        int fallback_errno = 0;
        if (bc_duplicate_reader_consume_file(items[index].absolute_path, items[index].file_size, items[index].consumer_context,
                                             consumer_function, &fallback_errno)) {
            items[index].success = true;
            items[index].errno_value = 0;
        } else {
            items[index].success = false;
            items[index].errno_value = fallback_errno;
        }
    }
    return true;
}
