#pragma once

#include <stdbool.h>

typedef enum {
    RCORDER_ACTION_NONE,
    RCORDER_ACTION_KEEP,
    RCORDER_ACTION_SKIP,
} rcorder_action_t;

typedef struct rcorder_options_t rcorder_options_t;

rcorder_action_t rcorder_options_add_ks(rcorder_options_t *, const char *, rcorder_action_t);
void rcorder_options_destroy(rcorder_options_t *);
rcorder_options_t *rcorder_options_create(char **);
void rcorder_options_set_include_orphans(rcorder_options_t *, bool);
void rcorder_options_set_reverse(rcorder_options_t *, bool);
