#pragma once

#include <stdint.h>
#include <stdbool.h>

// NOTE: for compatibility with FreeBSD < 11.1
#ifndef ZFS_MAX_DATASET_NAME_LEN
# define ZFS_MAX_DATASET_NAME_LEN MAXPATHLEN
#endif /* !ZFS_MAX_DATASET_NAME_LEN */

// HACK: this constant is intended to provide a non-random and consistent buffer size for pool,
// filesystem, snapshot, bookmark names to "consumers"
#define ZFS_MAX_NAME_LEN 256

typedef enum {
    UZFS_TYPE_POOL,
    UZFS_TYPE_FIRST = UZFS_TYPE_POOL,
    UZFS_TYPE_FILESYSTEM,
    UZFS_TYPE_SNAPSHOT,
    UZFS_TYPE_LAST = UZFS_TYPE_SNAPSHOT,
} uzfs_type_t;

typedef struct uzfs_lib_t uzfs_lib_t;
typedef struct uzfs_ptr_t uzfs_ptr_t;

uzfs_lib_t *uzfs_init(char **);
void uzfs_fini(uzfs_lib_t *);

void uzfs_close(uzfs_ptr_t **);
uzfs_ptr_t *uzfs_from_name(uzfs_lib_t *, const char *, uzfs_type_t);

const char *uzfs_get_name(uzfs_ptr_t *);

bool uzfs_is_fs(const char *);
uzfs_type_t uzfs_get_type(uzfs_ptr_t *);
uzfs_ptr_t *uzfs_pool_from_fs(uzfs_ptr_t *);
bool uzfs_same_fs(uzfs_ptr_t *, uzfs_ptr_t *);
bool uzfs_same_pool(uzfs_ptr_t *, uzfs_ptr_t *);
uzfs_ptr_t *uzfs_fs_from_file(uzfs_lib_t *, const char *);

bool uzfs_rollback(uzfs_ptr_t *, uzfs_ptr_t *, bool, char **);
bool uzfs_snapshot(uzfs_ptr_t *, const char *, bool, char *, size_t, bool, char **);
bool uzfs_iter_snapshots(uzfs_ptr_t *, bool (*)(uzfs_ptr_t *, void *, char **), void *, char **);

bool uzfs_fs_prop_get(uzfs_ptr_t *, const char *, char *, size_t); // TODO
bool uzfs_fs_prop_get_numeric(uzfs_ptr_t *, const char *, uint64_t *); // TODO
bool uzfs_prop_set(uzfs_ptr_t *, const char *, const char *, char **);
bool uzfs_prop_set_numeric(uzfs_ptr_t *, const char *, uint64_t, char **);
