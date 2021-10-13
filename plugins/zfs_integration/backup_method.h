#pragma once

#include <stdbool.h>

#include "probe.h"

typedef enum {
    BM_OK,
    BM_SKIP,
    BM_ERROR,
} bm_code_t;

typedef struct {
    const char *name;
    bm_code_t (*suitable)(paths_to_check_t *, void **, char **);
    void (*fini)(void *);
    bool (*snapshot)(const char *, void *, char **);
    bool (*rollback)(void *, bool, char **);
} backup_method_t;
