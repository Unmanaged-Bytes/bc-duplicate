// SPDX-License-Identifier: MIT

#include "bc_duplicate_discovery_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/vfs.h>

#ifndef PROC_SUPER_MAGIC
#define PROC_SUPER_MAGIC 0x9fa0
#endif
#ifndef SYSFS_MAGIC
#define SYSFS_MAGIC 0x62656572
#endif
#ifndef DEVPTS_SUPER_MAGIC
#define DEVPTS_SUPER_MAGIC 0x1cd1
#endif
#ifndef CGROUP_SUPER_MAGIC
#define CGROUP_SUPER_MAGIC 0x27e0eb
#endif
#ifndef CGROUP2_SUPER_MAGIC
#define CGROUP2_SUPER_MAGIC 0x63677270
#endif
#ifndef BPF_FS_MAGIC
#define BPF_FS_MAGIC 0xcafe4a11
#endif
#ifndef TRACEFS_MAGIC
#define TRACEFS_MAGIC 0x74726163
#endif
#ifndef DEBUGFS_MAGIC
#define DEBUGFS_MAGIC 0x64626720
#endif
#ifndef SECURITYFS_MAGIC
#define SECURITYFS_MAGIC 0x73636673
#endif
#ifndef PSTOREFS_MAGIC
#define PSTOREFS_MAGIC 0x6165676C
#endif
#ifndef SELINUX_MAGIC
#define SELINUX_MAGIC 0xf97cff8c
#endif
#ifndef MQUEUE_MAGIC
#define MQUEUE_MAGIC 0x19800202
#endif
#ifndef BINFMTFS_MAGIC
#define BINFMTFS_MAGIC 0x42494e4d
#endif
#ifndef AUTOFS_SUPER_MAGIC
#define AUTOFS_SUPER_MAGIC 0x0187
#endif

bool bc_duplicate_discovery_glob_contains_metacharacter(const char* pattern, bool* out_contains)
{
    bool escaped = false;
    for (const char* cursor = pattern; *cursor != '\0'; ++cursor) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (*cursor == '\\') {
            escaped = true;
            continue;
        }
        if (*cursor == '*' || *cursor == '?' || *cursor == '[') {
            *out_contains = true;
            return true;
        }
    }
    *out_contains = false;
    return true;
}

bool bc_duplicate_discovery_path_is_pseudo_filesystem(const char* path, bool* out_is_pseudo)
{
    *out_is_pseudo = false;
    struct statfs filesystem_stat;
    if (statfs(path, &filesystem_stat) != 0) {
        return false;
    }
    switch ((unsigned long)filesystem_stat.f_type) {
    case PROC_SUPER_MAGIC:
    case SYSFS_MAGIC:
    case DEVPTS_SUPER_MAGIC:
    case CGROUP_SUPER_MAGIC:
    case CGROUP2_SUPER_MAGIC:
    case BPF_FS_MAGIC:
    case TRACEFS_MAGIC:
    case DEBUGFS_MAGIC:
    case SECURITYFS_MAGIC:
    case PSTOREFS_MAGIC:
    case SELINUX_MAGIC:
    case MQUEUE_MAGIC:
    case BINFMTFS_MAGIC:
    case AUTOFS_SUPER_MAGIC:
        *out_is_pseudo = true;
        break;
    default:
        break;
    }
    return true;
}
