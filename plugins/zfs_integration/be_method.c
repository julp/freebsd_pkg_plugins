#include <limits.h>
#include <be.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "kissc/stpcpy_sp.h"
#include "shared/path_join.h"
#include "backup_method.h"

#define BE_PROPERTY_CREATION "creation"

#ifdef DEBUG
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, "[%s:%d] " format ": %s", __func__, __LINE__, ## __VA_ARGS__, libbe_error_description(lbh))
#else
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, format ": %s", ## __VA_ARGS__, libbe_error_description(lbh))
#endif /* DEBUG */

// <defined by raw_zfs_method.c>
extern bool has_zfs_properties(uzfs_ptr_t *, int *, uint64_t *);
extern bool set_zfs_properties(uzfs_ptr_t *, const char *, char **);
// </defined by raw_zfs_method.c>

static const char *extract_name_from_be(nvpair_t *be)
{
    assert(NULL != be);

    return nvpair_name(be); // <=> nvlist_lookup_string(cur, "name", &name);
}

static bool be_get_prop_numeric(nvpair_t *be, const char *property, uint64_t *creation, char **error)
{
    bool ok;

    assert(NULL != be);
    assert(NULL != creation);

    ok = false;
    *creation = 0;
    do {
        char *endptr, *v;
        nvlist_t *dsprops;
        unsigned long long value;

        dsprops = NULL;
        nvpair_value_nvlist(be, &dsprops);
        if (0 != nvlist_lookup_string(dsprops, property, &v)) {
            set_generic_error(error, "couldn't retrieve property '%s' for BE '%s'", property, extract_name_from_be(be));
            break;
        }
        errno = 0;
        value = strtoull(v, &endptr, 10);
        if ('\0' != *endptr || (errno == ERANGE && value == ULLONG_MAX)) {
            set_generic_error(error, "value '%s' of property '%s' couldn't be properly parsed for BE '%s'", v, property, extract_name_from_be(be));
            break;
        }
        *creation = (uint64_t) value;
        ok = true;
    } while (false);

    return ok;
}

static uzfs_ptr_t *be_to_fs(paths_to_check_t *ptc, libbe_handle_t *lbh, const char *be, char **error)
{
    uzfs_ptr_t *fs;

    fs = NULL;
    do {
        char dataset[ZFS_MAX_DATASET_NAME_LEN];

        if (!path_join(dataset, dataset + STR_SIZE(dataset), error, be_root_path(lbh), be, NULL)) {
            break;
        }
        if (NULL == (fs = uzfs_from_name(ptc->lh, dataset, UZFS_TYPE_FILESYSTEM))) {
            set_generic_error(error, "couldn't acquire a ZFS descriptor for BE '%s'", be);
            break;
        }
    } while (false);

    return fs;
}

static bm_code_t be_suitable(paths_to_check_t *ptc, void **data, char **error)
{
    bm_code_t retval;

    assert(NULL != ptc);
    assert(NULL != data);

    retval = BM_ERROR;
    do {
        size_t i;
        bool all_on_zfs;
        libbe_handle_t *lbh;

        all_on_zfs = true;
        for (i = 0; i < _FS_COUNT; i++) {
            all_on_zfs &= NULL != ptc->paths[i].fs;
        }
        // /usr/local is not on ZFS so it is not suitable for recovery
//         if (NULL == ptc->localbase.fs) {
        // not really suitable
        if (!all_on_zfs || !uzfs_same_fs(ptc->root.fs, ptc->localbase.fs) || !uzfs_same_fs(ptc->root.fs, ptc->pkg_dbdir.fs)) {
            retval = BM_SKIP;
            break;
        }
        if (NULL == (lbh = libbe_init(NULL))) {
            set_generic_error(error, "libbe initialisation failed");
            break;
        }
#ifdef DEBUG
        libbe_print_on_error(lbh, true); // TODO: explicit false when !DEBUG ?
#endif /* DEBUG */
        *data = lbh;
        retval = BM_OK;
    } while (false);

    return retval;
}

static bool be_take_snapshot(paths_to_check_t *ptc, const char *snapshot, const char *hook, void *data, char **error)
{
    bool ok;
    uzfs_ptr_t *fs;

    assert(NULL != ptc);
    assert(NULL != snapshot);
    assert(NULL != hook);
    assert(NULL != data);

    fs = NULL;
    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_create(lbh, snapshot)) {
            set_be_error(error, lbh, "failed to create BE '%s'", snapshot);
            break;
        }
        if (NULL == (fs = be_to_fs(ptc, lbh, snapshot, error))) {
            break;
        }
        if (!set_zfs_properties(fs, hook, error)) {
            break;
        }
        ok = true;
    } while (false);
    if (NULL != fs) {
        uzfs_close(&fs);
    }

    return ok;
}

static bool be_cast_to_snapshot(snapshot_t *snap, nvpair_t *bepair, const char *name, char **error)
{
    bool ok;
    extern int name_to_hook(const char *);

    assert(NULL != snap);
    assert(NULL != bepair);

    ok = false;
    do {
        if (strlcpy(snap->name, name, SNAPSHOT_MAX_NAME_LEN) >= SNAPSHOT_MAX_NAME_LEN) {
            set_generic_error(error, "BE name '%s' is too long %zu for %zu", name, strlen(name), SNAPSHOT_MAX_NAME_LEN);
            break;
        }
        if (!be_get_prop_numeric(bepair, BE_PROPERTY_CREATION, &snap->creation, error)) {
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static bool be_list(paths_to_check_t *ptc, void *data, DList *l, char **error)
{
    bool ok;
    DList *bes;
    nvlist_t *props;

    assert(NULL != ptc);
    assert(NULL != data);
    assert(NULL != l);

    ok = false;
    bes = NULL;
    props = NULL;
    do {
        nvpair_t *cur;
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (NULL == (bes = malloc(sizeof(*bes)))) {
            set_malloc_error(error, sizeof(*bes));
            break;
        }
        dlist_init(bes, /*snapshot_copy, */snapshot_destroy);
        if (!dlist_append(l, bes)) {
            set_generic_error(error, "dlist_append failed");
            break;
        }
        if (0 != be_prop_list_alloc(&props)) {
            set_be_error(error, lbh, "be_prop_list_alloc failed");
            break;
        }
        if (0 != be_get_bootenv_props(lbh, props)) {
            set_be_error(error, lbh, "be_get_bootenv_props failed");
            break;
        }
        for (ok = true, cur = nvlist_next_nvpair(props, NULL); ok && NULL != cur; cur = nvlist_next_nvpair(props, cur)) {
            const char *name;
            snapshot_t *snap;

            snap = malloc(sizeof(*snap)); // TODO
            name = extract_name_from_be(cur);
            if (!(ok &= (NULL != (snap->fs = be_to_fs(ptc, lbh, name, error))))) {
                break;
            }
            if (has_zfs_properties(snap->fs, &snap->hook, &snap->version)) {
                if (!(ok &= be_cast_to_snapshot(snap, cur, name, error) && dlist_append(bes, snap))) {
                    // TODO: dlist_append failure
                    break;
                }
            }
#if 0
            if (NULL != snap->fs) {
                uzfs_close(&snap->fs);
            }
#endif
        }
    } while (false);
#if 0
    if (!ok && NULL != bes) {
        dlist_clear(bes);
        free(bes);
    }
#endif
    if (NULL != props) {
        be_prop_list_free(props);
    }

    return ok;
}

static bool be_rollback_to(const snapshot_t *snap, void *data, bool temporary, char **error)
{
    bool ok;

    assert(NULL != snap);
    assert(NULL != data);

    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_activate(lbh, snap->name, temporary)) {
            set_be_error(error, lbh, "failed to activate BE '%s'", snap->name);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static bool be_destroy_snapshot(snapshot_t *snap, void *data, bool UNUSED(recursive), char **error)
{
    bool ok;

    assert(NULL != snap);
    assert(NULL != data);

    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_destroy(lbh, snap->name, BE_DESTROY_ORIGIN)) {
            set_be_error(error, lbh, "failed to destroy BE '%s'", snap->name);
            break;
        }
        ok = true;
    } while (false);

    return ok;
}

static void be_fini(void *data)
{
    libbe_handle_t *lbh;

    assert(NULL != data);

    lbh = (libbe_handle_t *) data;
    libbe_close(lbh);
}

const backup_method_t be_method = {
    "be",
    be_suitable,
    be_fini,
    be_take_snapshot,
    be_list,
    be_rollback_to,
    be_destroy_snapshot,
};
