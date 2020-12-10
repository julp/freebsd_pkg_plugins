#pragma once

#include <stdbool.h>

// NOTE: for compatibility with FreeBSD < 11.1
#ifndef ZFS_MAX_DATASET_NAME_LEN
# define ZFS_MAX_DATASET_NAME_LEN MAXNAMELEN
#endif /* !ZFS_MAX_DATASET_NAME_LEN */

// HACK: this constant is intended to provide a non-random and consistent buffer size for pool,
// filesystem, snapshot, bookmark names to "consumers"
#define ZFS_MAX_NAME_LEN 256

typedef struct uzfs_lib_t uzfs_lib_t;
typedef struct uzfs_fs_t uzfs_fs_t;
typedef struct uzfs_pool_t uzfs_pool_t;

uzfs_lib_t *uzfs_init(char **);
void uzfs_fini(uzfs_lib_t *);

bool uzfs_is_fs(const char *);
bool uzfs_same_fs(uzfs_fs_t *, uzfs_fs_t *);
bool uzfs_same_pool(uzfs_fs_t *, uzfs_fs_t *);

void uzfs_pool_close(uzfs_pool_t *);
uzfs_pool_t *uzfs_pool_from_fs(uzfs_fs_t *);
uzfs_pool_t *uzfs_pool_from_name(uzfs_lib_t *, const char *);
const char *uzfs_pool_get_name(uzfs_pool_t *);

void uzfs_fs_close(uzfs_fs_t *);
uzfs_fs_t *uzfs_fs_from_file(uzfs_lib_t *, const char *);
uzfs_fs_t *uzfs_fs_from_name(uzfs_lib_t *, const char *);
const char *uzfs_fs_get_name(uzfs_fs_t *);

bool uzfs_snapshot(uzfs_fs_t *, const char *, char *, size_t, bool, char **);
