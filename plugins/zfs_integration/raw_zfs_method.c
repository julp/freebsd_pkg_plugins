#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "error.h"
#include "zfs.h"
#include "probe.h"
#include "backup_method.h"
#include "selection.h"

typedef struct {
    bool recursive;
} raw_zfs_context_t;

bool set_zfs_properties(uzfs_ptr_t *fs, const char *hook, char **error)
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
            set_generic_error(error, "setting property '%s' to '%" PRIu64 "' on '%s' failed", ZINT_VERSION_PROPERTY, ZINT_VERSION_NUMBER, uzfs_get_name(fs));
            break;
        }
        if (!uzfs_fs_prop_set(fs, ZINT_HOOK_PROPERTY, hook, error)) {
            set_generic_error(error, "setting property '%s' to '%s' on '%s' failed", ZINT_HOOK_PROPERTY, hook, uzfs_get_name(fs));
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

bool has_zfs_properties(uzfs_ptr_t *fs)
{
    bool ok;
    extern int name_to_hook(const char *);

    assert(NULL != fs);

    ok = false;
    do {
        char hook[100];
        uint64_t version;
        const char *fullname, *basename, *arobase;

        basename = fullname = uzfs_get_name(fs);
        if (NULL != (arobase = strchr(fullname, '@'))) {
            basename = arobase + 1;
        }
retry:
        if (!uzfs_fs_prop_get_numeric(fs, ZINT_VERSION_PROPERTY, &version)) {
            // <TODO: for transition, to be removed in a future version>
            if (str_starts_with(basename, "pkg_pre_upgrade_") && strlen(basename) == STR_LEN("pkg_pre_upgrade_YYYY-mm-dd_HH:ii:ss")) {
                if (set_zfs_properties(fs, "PRE:UPGRADE", NULL)) {
                    goto retry;
                }
            }
            // </TODO: for transition, to be removed in a future version>
            debug("skipping '%s', not created by zint (property '%s' missing)", fullname, ZINT_VERSION_PROPERTY);
            break;
        }
        if (!uzfs_fs_prop_get(fs, ZINT_HOOK_PROPERTY, hook, STR_SIZE(hook))) {
            debug("skipping '%s', not created by zint (property '%s' missing)", fullname, ZINT_HOOK_PROPERTY);
            break;
        }
        debug("%s was created by zint version %" PRIu64 " for '%s' (%d)", fullname, version, hook, name_to_hook(hook));
        ok = true;
    } while (false);

    return ok;
}

typedef struct {
    char name[ZFS_MAX_NAME_LEN];
    uint64_t creation;
    uzfs_ptr_t *fs;
} snapshot_t;

static int compare_snapshot_by_creation_date_desc(snapshot_t *a, snapshot_t *b)
{
    assert(NULL != a);
    assert(NULL != b);

    return (b->creation >= a->creation ? b->creation - a->creation : -1);
}

#if 0
static int compare_snapshot_by_creation_date_asc(snapshot_t *a, snapshot_t *b)
{
    assert(NULL != a);
    assert(NULL != b);

    return (a->creation >= b->creation ? a->creation - b->creation : -1);
}
#endif

static void destroy_snapshot(snapshot_t *snap)
{
    assert(NULL != snap);

    if (NULL != snap->fs) {
        uzfs_close(&snap->fs);
    }
    free(snap);
}

// used as DupFunc
static snapshot_t *copy_snapshot(uzfs_ptr_t *fs)
{
    snapshot_t *snap, *ret;

    snap = ret = NULL;
    do {
        if (NULL == (snap = malloc(sizeof(*snap)))) {
            break;
        }
        if (strlcpy(snap->name, uzfs_get_name(fs), STR_SIZE(snap->name)) >= STR_SIZE(snap->name)) {
            break;
        }
        if (!uzfs_fs_prop_get_numeric(fs, "creation", &snap->creation)) {
            break;
        }
        ret = snap;
    } while (false);
    if (ret != snap) {
        free(snap);
    }

    return ret;
}

#ifdef DEBUG
# include <time.h>
// selection_dump(bes, (void (*)(void *)) print_snapshot);
static void print_snapshot(snapshot_t *snap)
{
    time_t time;
    struct tm tm;
    size_t written;
    char buffer[128];

    assert(NULL != snap);

    time = (time_t) snap->creation;
    localtime_r(&time, &tm);
    written = strftime(buffer, STR_SIZE(buffer), "%F %T", &tm);
    assert(written < STR_LEN(buffer));
    fprintf(stderr, "(%s) %s = %s (%" PRIu64 ")\n", __func__, snap->name, buffer, snap->creation);
}
#endif /* DEBUG */

static bool snaphosts_iter_callback_build_array(uzfs_ptr_t *fs, void *data, char **error)
{
    bool ok;
    selection_t *bes;

    assert(NULL != fs);
    assert(NULL != data);

    ok = false;
    do {
        bes = (selection_t *) data;
        if (has_zfs_properties(fs)) {
            if (!selection_add(bes, fs)) {
                // TODO: error handling
            }
        }
        ok = true;
    } while (false);

    return ok;
}

static selection_t *fetch_sorted_zint_snapshot(uzfs_ptr_t *fs, CmpFunc cmp, char **error)
{
    selection_t *ret, *bes;

    assert(NULL != fs);
    assert(NULL != cmp);

    bes = ret = NULL;
    do {
        if (NULL == (bes = selection_new(cmp, (DtorFunc) destroy_snapshot, (DupFunc) copy_snapshot))) {
            set_generic_error(error, "TODO");
            break;
        }
        if (!uzfs_iter_snapshots(fs, snaphosts_iter_callback_build_array, bes, error)) {
            break;
        }
        ret = bes;
    } while (false);
    if (ret != bes) {
        selection_destroy(bes);
    }

    return ret;
}

// TODO: handle canmount=off
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
#if 1 // TODO: TEST
        {
            bool ok;
            selection_t *bes;

            ok = false;
            bes = NULL;
            do {
                if (NULL == (bes = fetch_sorted_zint_snapshot(ptc->root.fs, (CmpFunc) compare_snapshot_by_creation_date_desc, error))) {
                    break;
                }
                selection_dump(bes, (void (*)(void *)) print_snapshot);
//                 if (!selection_apply(bes, (bool (*)(void *, void *, char **)) apply_retention, NULL /* TODO: data */, error)) {
//                     break;
//                 }
                ok = true;
            } while (false);
            if (NULL != bes) {
                selection_destroy(bes);
            }
        }
#endif
        *data = ctxt;
        retval = BM_OK;
    } while (false);

    return retval;
}

static bool individual_snapshot(raw_zfs_context_t *ctxt, paths_to_check_t *ptc, uzfs_ptr_t *fs, const char *snapshot, const char *hook, char **error)
{
    bool ok;

    ok = false;
    do {
        uzfs_ptr_t *snap;
        char buffer[ZFS_MAX_NAME_LEN];

        if (!uzfs_snapshot(fs, snapshot, false, buffer, STR_SIZE(buffer), ctxt->recursive, error)) {
            break;
        }
        if (NULL == (snap = uzfs_from_name(ptc->lh, buffer, UZFS_TYPE_SNAPSHOT))) {
            set_generic_error(error, "couldn't acquire a ZFS descriptor on snapshot '%s'", buffer);
            break;
        }
        if (!set_zfs_properties(snap, hook, error)) {
             break;
         }
        uzfs_close(&snap);
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

static bool raw_zfs_rollback(paths_to_check_t *UNUSED(ptc), void *UNUSED(data), bool UNUSED(dry_run), bool UNUSED(temporary), char **error)
{
    set_generic_error(error, "sorry, this functionnality is not (yet) implemented for raw ZFS");

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
