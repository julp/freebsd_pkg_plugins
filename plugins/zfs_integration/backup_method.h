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

typedef struct {
    const char *name;
    bm_code_t (*suitable)(paths_to_check_t *, void **, char **);
    void (*fini)(void *);
    bool (*snapshot)(paths_to_check_t *, const char *, const char *, void *, char **);
    bool (*rollback)(paths_to_check_t *, void *, bool, bool, char **);
} backup_method_t;
