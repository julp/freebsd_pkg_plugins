#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "error.h"
#include "zfs.h"
#include "probe.h"
#include "backup_method.h"

typedef struct {
    bool recursive;
} raw_zfs_context_t;

bool set_zfs_properties(uzfs_fs_t *fs, const char *hook, char **error)
{
    bool ok;

    assert(NULL != fs);
    assert(NULL != hook);

    ok = false;
    do {
#if 0
        zfs_type_t type;
        const char *kind;

        type = zfs_get_type(const zfs_handle_t *);
        if (ZFS_TYPE_FILESYSTEM == type) {
            kind = "BE";
        } else if (ZFS_TYPE_SNAPSHOT == type) {
            kind = "snapshot";
        } else {
            kind = "unknown";
        }
#endif
        if (!uzfs_fs_prop_set_numeric(fs, ZINT_VERSION_PROPERTY, ZINT_VERSION_NUMBER, error)) {
            set_generic_error(error, "setting property '%s' to '%" PRIu64 "' on '%s' failed", ZINT_VERSION_PROPERTY, ZINT_VERSION_NUMBER, uzfs_fs_get_name(fs));
            break;
        }
        if (!uzfs_fs_prop_set(fs, ZINT_HOOK_PROPERTY, hook, error)) {
            set_generic_error(error, "setting property '%s' to '%s' on '%s' failed", ZINT_HOOK_PROPERTY, hook, uzfs_fs_get_name(fs));
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

// <TODO: for transition, to be removed in a future version>
static bool str_starts_with(const char *string, const char *prefix)
{
    size_t prefix_len;

    assert(NULL != string);
    assert(NULL != prefix);

    prefix_len = strlen(prefix);

    return prefix_len <= strlen(string) && 0 == strncmp(string, prefix, prefix_len);
}
// </TODO: for transition, to be removed in a future version>

bool has_zfs_properties(uzfs_fs_t *fs)
{
    bool ok;
    extern int name_to_hook(const char *);

    assert(NULL != fs);

    ok = false;
    do {
        char hook[100];
        uint64_t version;
        const char *name;

        name = uzfs_fs_get_name(fs);
retry:
        if (!uzfs_fs_prop_get_numeric(fs, ZINT_VERSION_PROPERTY, &version)) {
            // <TODO: for transition, to be removed in a future version>
            if (str_starts_with(name, "pkg_pre_upgrade_") && strlen(name) == STR_LEN("pkg_pre_upgrade_YYYY-mm-dd_HH:ii:ss")) {
                if (set_zfs_properties(fs, "PRE:UPGRADE", NULL)) {
                    goto retry;
                }
            }
            // </TODO: for transition, to be removed in a future version>
            debug("skipping '%s', not created by zint (propery '%s' missing)", name, ZINT_VERSION_PROPERTY);
            break;
        }
        if (!uzfs_fs_prop_get(fs, ZINT_HOOK_PROPERTY, hook, STR_SIZE(hook))) {
            debug("skipping '%s', not created by zint (propery '%s' missing)", name, ZINT_HOOK_PROPERTY);
            break;
        }
        debug("%s was created by zint version %" PRIu64 " for '%s' (%d)", name, version, hook, name_to_hook(hook));
        ok = true;
    } while (false);

    return ok;
}

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

static bool individual_snapshot(raw_zfs_context_t *ctxt, paths_to_check_t *ptc, uzfs_fs_t *fs, const char *snapshot, const char *hook, char **error)
{
    bool ok;

    ok = false;
    do {
        uzfs_fs_t *snap;
        char buffer[ZFS_MAX_NAME_LEN];

        if (!uzfs_snapshot(fs, snapshot, false, buffer, STR_SIZE(buffer), ctxt->recursive, error)) {
            break;
        }
        if (NULL == (snap = uzfs_fs_from_name(ptc->lh, buffer))) {
            set_generic_error(error, "couldn't acquire a ZFS descriptor on snapshot '%s'", buffer);
            break;
        }
        if (!set_zfs_properties(snap, hook, error)) {
             break;
         }
        uzfs_fs_close(snap);
        ok = true;
    } while (false);

    return ok;
}

static bool raw_zfs_snapshot(paths_to_check_t *ptc, const char *snapshot, const char *hook, void *data, char **error)
{
    bool ok;
    raw_zfs_context_t *ctxt;

    assert(NULL != data);

    ok = false;
    ctxt = (raw_zfs_context_t *) data;
    do {
        if (!individual_snapshot(ctxt, ptc, ptc->root.fs, snapshot, hook, error)) {
            break;
        }
        if (!ctxt->recursive) {
            if (NULL != ptc->localbase.fs) {
                if (!individual_snapshot(ctxt, ptc, ptc->localbase.fs, snapshot, hook, error)) {
                    break;
                }
            }
            if (NULL != ptc->pkg_dbdir.fs) {
                if (!individual_snapshot(ctxt, ptc, ptc->pkg_dbdir.fs, snapshot, hook, error)) {
                    break;
                }
            }
        }
        ok = true;
    } while (false);

    return ok;
}

static bool raw_zfs_rollback(paths_to_check_t *UNUSED(ptc), void *UNUSED(data), bool UNUSED(temporary), char **error)
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
