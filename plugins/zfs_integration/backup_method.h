#pragma once

#include <stdbool.h>

#include "probe.h"
#include "dlist.h"
#include "snapshot.h"

#define ZINT_HOOK_PROPERTY "zint:hook"
#define ZINT_VERSION_PROPERTY "zint:version"

typedef enum {
    BM_OK,
    BM_SKIP,
    BM_ERROR,
} bm_code_t;

typedef struct {
    const char *name;
    bm_code_t (*suitable)(paths_to_check_t *ptc, void **data, char **error);
    void (*fini)(void *data);
    bool (*snapshot)(paths_to_check_t *ptc, const char *snapshot, const char *hook, void *data, char **error);
    bool (*list)(paths_to_check_t *ptc, void *data, DList *l, char **error);
    bool (*rollback_to)(const snapshot_t *snap, void *data, bool temporary, char **error);
    bool (*destroy)(snapshot_t *snap, void *data, char **error);
} backup_method_t;
