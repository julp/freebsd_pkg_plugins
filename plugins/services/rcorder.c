#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "rcorder.h"
#include "private_rcorder.h"

rcorder_options_t *rcorder_options_create(char **error)
{
    rcorder_options_t *ro;

    if (NULL == (ro = malloc(sizeof(*ro)))) {
        set_malloc_error(error, sizeof(*ro));
    } else {
        ro->keep_count = 0;
        ro->reverse = false;
        ro->include_orphans = false;
        hashtable_ascii_cs_init(&ro->ks, NULL, NULL, NULL);
    }

    return ro;
}

void rcorder_options_destroy(rcorder_options_t *ro)
{
    assert(NULL != ro);

    hashtable_destroy(&ro->ks);
    free(ro);
}

void rcorder_options_set_reverse(rcorder_options_t *ro, bool reverse)
{
    assert(NULL != ro);

    ro->reverse = reverse;
}

void rcorder_options_set_include_orphans(rcorder_options_t *ro, bool include_orphans)
{
    assert(NULL != ro);

    ro->include_orphans = include_orphans;
}

rcorder_action_t rcorder_options_add_ks(rcorder_options_t *ro, const char *name, rcorder_action_t action)
{
    intptr_t old_action;

    assert(NULL != ro);
    assert(NULL != name);
    assert(RCORDER_ACTION_KEEP == action || RCORDER_ACTION_SKIP == action);

    old_action = RCORDER_ACTION_NONE;
    hashtable_put(&ro->ks, 0, name, (intptr_t) action, &old_action);
    if (RCORDER_ACTION_KEEP == action && RCORDER_ACTION_KEEP != old_action) {
        // increment keep_count if a same keyword is overwritten from skip to keep (-s keyword -k keyword)
        ++ro->keep_count;
    } else if (RCORDER_ACTION_SKIP == action && RCORDER_ACTION_KEEP == old_action) {
        // decrement keep_count if a same keyword is overwritten from keep to skip (-k keyword -s keyword)
        --ro->keep_count;
    }

    return old_action;
}
