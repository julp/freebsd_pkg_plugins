#pragma once

#include <stdbool.h>

#include "probe.h"

#define ZINT_HOOK_PROPERTY "zint:hook"
#define ZINT_VERSION_PROPERTY "zint:version"

typedef enum {
    BM_OK,
    BM_SKIP,
    BM_ERROR,
} bm_code_t;

// BE_MAXPATHLEN = 512
// ZFS_MAX_NAME_LEN = 256
typedef struct {
    const char *name;
    bm_code_t (*suitable)(paths_to_check_t *ptc, void **data, char **error);
    void (*fini)(void *data);
    bool (*snapshot)(paths_to_check_t *ptc, const char *snapshot, const char *hook, void *data, char **error);
    bool (*rollback)(paths_to_check_t *ptc, void *data, bool dry_run, bool temporary, char **error);
    // <TEST>
    // compare ?
    bool (*list)(paths_to_check_t *ptc, void *data, /*selection_t **/, char **error);
    bool (*rollback_to)(const char *name, void *data, bool temporary, char **error);
    bool (*destroy_by_name)(const char *name, void *data, bool recursive, char **error);
    // </TEST>
} backup_method_t;
