#pragma once

#include "zfs.h"

enum {
    FS_ROOT,
    FS_LOCALBASE,
    FS_PKG_DBDIR,
    _FS_COUNT,
};

typedef struct {
    uzfs_fs_t *fs;
    const char *path;
} path_to_check_t;

typedef struct {
    uzfs_lib_t *lh;
    union {
        struct {
            path_to_check_t root;
            path_to_check_t localbase;
            path_to_check_t pkg_dbdir;
        };
        path_to_check_t paths[_FS_COUNT];
    };
} paths_to_check_t;

paths_to_check_t *prober_create(char **);
void prober_destroy(paths_to_check_t *);
