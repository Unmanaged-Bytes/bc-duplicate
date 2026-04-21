// SPDX-License-Identifier: MIT

#ifndef BC_DUPLICATE_DIRENT_INTERNAL_H
#define BC_DUPLICATE_DIRENT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define BC_DUPLICATE_DIRENT_BUFFER_SIZE ((size_t)8192)

typedef struct bc_duplicate_dirent_reader {
    int dir_fd;
    int last_errno;
    ssize_t buffer_used;
    size_t cursor;
    char buffer[BC_DUPLICATE_DIRENT_BUFFER_SIZE];
} bc_duplicate_dirent_reader_t;

typedef struct bc_duplicate_dirent_entry {
    const char* name;
    size_t name_length;
    unsigned char d_type;
} bc_duplicate_dirent_entry_t;

void bc_duplicate_dirent_reader_init(bc_duplicate_dirent_reader_t* reader, int dir_fd);

bool bc_duplicate_dirent_reader_next(bc_duplicate_dirent_reader_t* reader, bc_duplicate_dirent_entry_t* out_entry, bool* out_has_entry);

#endif /* BC_DUPLICATE_DIRENT_INTERNAL_H */
