#include <stdlib.h>
#include <inttypes.h>

#include "common.h"
#include "error.h"
#include "zfs.h"
#include "probe.h"
#include "backup_method.h"
#include "hashtable.h"

typedef struct {
    bool recursive;
} raw_zfs_context_t;

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

// <NOTE: also used by be_method.c>
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
        if (!uzfs_set_prop_numeric(fs, ZINT_VERSION_PROPERTY, ZINT_VERSION_NUMBER, error)) {
            set_generic_error(error, "setting property '%s' to '%" PRIu64 "' on '%s' failed", ZINT_VERSION_PROPERTY, ZINT_VERSION_NUMBER, uzfs_get_name(fs));
            break;
        }
        if (!uzfs_set_prop(fs, ZINT_HOOK_PROPERTY, hook, error)) {
            set_generic_error(error, "setting property '%s' to '%s' on '%s' failed", ZINT_HOOK_PROPERTY, hook, uzfs_get_name(fs));
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

bool has_zfs_properties(uzfs_ptr_t *fs, int *output_hook, uint64_t *output_version)
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
        if (!uzfs_get_prop_numeric(fs, ZINT_VERSION_PROPERTY, &version)) {
            // <TODO: for transition, to be removed in a future version>
            if (str_starts_with(basename, "pkg_pre_upgrade_") && strlen(basename) == STR_LEN("pkg_pre_upgrade_YYYY-mm-dd_HH:ii:ss")) {
                if (set_zfs_properties(fs, "PRE:UPGRADE", NULL)) {
                    goto retry;
                }
            }
            // </TODO: for transition, to be removed in a future version>
            debug("DEBUG: ignoring '%s', not created by zint (property '%s' missing)", fullname, ZINT_VERSION_PROPERTY);
            break;
        }
        if (!uzfs_get_prop(fs, ZINT_HOOK_PROPERTY, hook, STR_SIZE(hook))) {
            debug("DEBUG: ignoring '%s', not created by zint (property '%s' missing)", fullname, ZINT_HOOK_PROPERTY);
            break;
        }
        if (NULL != output_hook) {
            *output_hook = name_to_hook(hook);
        }
        if (NULL != output_version) {
            *output_version = version;
        }
        ok = true;
    } while (false);

    return ok;
}
// </NOTE: also used by be_method.c>

// TODO: handle canmount=off
static bm_code_t raw_zfs_suitable(paths_to_check_t *ptc, void **data, char **error)
{
    bm_code_t retval;
    raw_zfs_context_t *ctxt;

    assert(NULL != ptc);
    assert(NULL != data);

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
            fprintf(stderr, "WARNING: pkg database is not located on a ZFS filesystem (%s), reverting '%s' will lead pkg to believe you use newer packages than they really are\n", ptc->pkg_dbdir.path, ptc->localbase.path);
        }
        *data = ctxt;
#if 1
        uzfs_depth(ptc->root.fs, ptc->localbase.fs);
#endif
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

    assert(NULL != ptc);
    assert(NULL != snapshot);
    assert(NULL != hook);
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

static bool raw_zfs_cast_to_snapshot(snapshot_t *snap, char **UNUSED(error))
{
    bool ok;

    assert(NULL != snap);
    assert(NULL != snap->fs);

    ok = false;
    do {
        if (strlcpy(snap->name, uzfs_get_name(snap->fs), SNAPSHOT_MAX_NAME_LEN) >= SNAPSHOT_MAX_NAME_LEN) {
            break;
        }
        if (!uzfs_get_prop_numeric(snap->fs, "creation", &snap->creation)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static bool snaphosts_iter_callback_build_array(uzfs_ptr_t *fs, void *data, char **error)
{
    bool ok;
    DList *l;

    assert(NULL != fs);
    assert(NULL != data);

    ok = false;
    do {
        snapshot_t snap;

        l = (DList *) data;
        snap.fs = fs;
        if (has_zfs_properties(fs, &snap.hook, &snap.version)) {
            if (!raw_zfs_cast_to_snapshot(&snap, error)) {
                break;
            }
            if (!dlist_append(l, &snap, error)) {
                break;
            }
        }
        ok = true;
    } while (false);

    return ok;
}

static bool raw_zfs_list(paths_to_check_t *ptc, void *UNUSED(data), DList *l, char **error)
{
    bool ok;

    assert(NULL != ptc);
    assert(NULL != l);

    ok = false;
    do {
        size_t i;
        Iterator it;
        HashTable ht;
        uzfs_ptr_t *fs;

        hashtable_ascii_cs_init(&ht, NULL, NULL, NULL);
        for (i = 0; i < _FS_COUNT; i++) {
            if (NULL != ptc->paths[i].fs) {
                hashtable_put(&ht, HT_PUT_ON_DUP_KEY_PRESERVE, uzfs_get_name(ptc->paths[i].fs), ptc->paths[i].fs, NULL);
            }
        }
        hashtable_to_iterator(&it, &ht);
        for (ok = true, iterator_first(&it); ok && iterator_is_valid(&it, NULL, &fs); iterator_next(&it)) {
            DList *snapshots;

            if (!(ok &= (NULL != (snapshots = dlist_new(snapshot_copy, snapshot_destroy, error))))) {
                break;
            }
            if (!(ok &= dlist_append(l, snapshots, error))) {
                break;
            }
            ok &= uzfs_iter_snapshots(fs, snaphosts_iter_callback_build_array, snapshots, error);
        }
        iterator_close(&it);
        hashtable_destroy(&ht);
    } while (false);

    return ok;
}

static bool raw_zfs_rollback_to(const snapshot_t *snap, void *UNUSED(data), bool UNUSED(temporary), char **UNUSED(error))
{
    bool ok;

    assert(NULL != snap);
//     assert(NULL != data);

    ok = false;
    do {
        // TODO: handle recursivity/others subfilesystems
        // TODO: a rollback can't be done on an active (booted) filesystem
//         if (uzfs_rollback(ptc->root.fs, name, bool force, error)) {
//             break;
//         }
        ok = true;
    } while (false);

    return ok;
}

static bool raw_zfs_destroy(snapshot_t *snap, void *UNUSED(data), char **error)
{
    bool ok;

    assert(NULL != snap);

    ok = false;
    do {
        if (!uzfs_filesystem_destroy(&snap->fs, error)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
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
    raw_zfs_list,
    raw_zfs_rollback_to,
    raw_zfs_destroy,
};
