#pragma once

#include <inttypes.h>

#include "zfs.h"

// SNAPSHOT_MAX_NAME_LEN should be at leat MAX(BE_MAXPATHLEN, ZFS_MAX_NAME_LEN)
// current values (FreeBSD 13.1) are:
// BE_MAXPATHLEN = 512
// ZFS_MAX_NAME_LEN = 256
#define SNAPSHOT_MAX_NAME_LEN 4096

typedef struct {
    char name[SNAPSHOT_MAX_NAME_LEN];
    uint64_t creation;
    uzfs_ptr_t *fs;
    int hook;
    uint64_t version;
} snapshot_t;

int snapshot_compare_by_creation_date_desc(const void *, const void *);
void snapshot_destroy(void *);
void *snapshot_copy(void *);

#ifdef DEBUG
void snapshot_print(snapshot_t *);
#endif /* DEBUG */
