#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "zfs.h"
#include "probe.h"
#include "backup_method.h"

typedef struct {
    bool recursive;
    paths_to_check_t *ptc;
} raw_zfs_context_t;

static bm_code_t raw_zfs_suitable(paths_to_check_t *ptc, void **data, char **error)
{
    bm_code_t retval;
    raw_zfs_context_t *ctxt;

    ctxt = NULL;
    retval = BM_ERROR;
    do {
        // /usr/local is not on ZFS so it is not suitable for "backup" (snapshot)
        if (NULL == ptc->localbase.fs) {
            retval = BM_SKIP;
            break;
        }
        if (NULL == (ctxt = malloc(sizeof(*ctxt)))) {
            set_malloc_error(error, sizeof(*ctxt));
            break;
        }
        ctxt->ptc = ptc;
        /* from here we know that /usr/local is on ZFS */
        if (NULL != ptc->root.fs) {
            if (uzfs_same_pool(ptc->root.fs, ptc->localbase.fs)) {
                // take a recursive snapshot
                ctxt->recursive = true;
            } else {
                // separated snapshots for / + /usr/local + PKG_DBDIR if relevant
                ctxt->recursive = false;
            }
        }
        if (NULL == ptc->pkg_dbdir.fs) {
            fprintf(stderr, "WARNING: pkg database is not located on a ZFS filesystem (%s), reverting %s will lead pkg to believe you use newer packages than they really are\n", ptc->pkg_dbdir.path, ptc->localbase.path);
        }
        *data = ctxt;
        retval = BM_OK;
    } while (false);

    return retval;
}

static bool raw_zfs_snapshot(const char *snapshot, void *data, char **error)
{
    bool ok;
    raw_zfs_context_t *ctxt;
    assert(NULL != data);

    ok = false;
    ctxt = (raw_zfs_context_t *) data;
    do {
        char buffer[ZFS_MAX_NAME_LEN];

        if (!uzfs_snapshot(ctxt->ptc->root.fs, snapshot, false, buffer, STR_SIZE(buffer), ctxt->recursive, error)) {
            break;
        }
        if (!ctxt->recursive) {
            if (NULL != ctxt->ptc->localbase.fs) {
                if (!uzfs_snapshot(ctxt->ptc->localbase.fs, snapshot, false, buffer, STR_SIZE(buffer), ctxt->recursive, error)) {
                    break;
                }
            }
            if (NULL != ctxt->ptc->pkg_dbdir.fs) {
                if (!uzfs_snapshot(ctxt->ptc->pkg_dbdir.fs, snapshot, false, buffer, STR_SIZE(buffer), ctxt->recursive, error)) {
                    break;
                }
            }
        }
        ok = true;
    } while (false);

    return ok;
}

static bool raw_zfs_rollback(void *UNUSED(data), bool UNUSED(temporary), char **error)
{
    set_generic_error(error, "this functionnality is not (yet) implemented for raw ZFS");

    return false;
}

static void raw_zfs_fini(void *data)
{
    raw_zfs_context_t *ctxt;

    assert(NULL != data);

    ctxt = (raw_zfs_context_t *) data;
    free(ctxt);
}

const backup_method_t raw_zfs_method = {
    "zfs",
    raw_zfs_suitable,
    raw_zfs_fini,
    raw_zfs_snapshot,
    raw_zfs_rollback,
};
