#include <stdio.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "backup_method.h"

static bm_code_t none_suitable(paths_to_check_t *UNUSED(ptc), void **data, char **UNUSED(error))
{
    assert(NULL != data);

    *data = NULL;

    return BM_OK;
}

static bool none_take_snapshot(paths_to_check_t *UNUSED(ptc), const char *UNUSED(snapshot), const char *UNUSED(hook), void *UNUSED(data), char **UNUSED(error))
{
    fprintf(stderr, "%s: sorry, you are on your own, nothing I can't do for you, it seems that %s is not located on a ZFS filesystem\n", NAME, localbase());

    return true;
}

static bool none_list(paths_to_check_t *UNUSED(ptc), void *UNUSED(data), DList *UNUSED(l), char **UNUSED(error))
{
    return true;
}

static bool none_rollback_to(const snapshot_t *UNUSED(snap), void *UNUSED(data), bool UNUSED(temporary), char **error)
{
    set_generic_error(error, "a rollback is not possible on a non-ZFS system");

    return true;
}

static bool none_destroy(snapshot_t *UNUSED(snap), void *UNUSED(data), char **UNUSED(error))
{
    return true;
}

static void none_fini(void *UNUSED(data))
{
    /* NOP */
}

const backup_method_t none_method = {
    "none",
    none_suitable,
    none_fini,
    none_take_snapshot,
    none_list,
    none_rollback_to,
    none_destroy,
};
