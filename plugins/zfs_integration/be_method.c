#include <limits.h>
#include <be.h>

#include "common.h"
#include "error.h"
#include "shared/os.h"
#include "kissc/stpcpy_sp.h"
#include "shared/path_join.h"
#include "backup_method.h"

#ifdef DEBUG
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, "[%s:%d] " format ": %s", __func__, __LINE__, ## __VA_ARGS__, libbe_error_description(lbh))
#else
# define set_be_error(error, lbh, format, ...) \
    _error_set(error, format ": %s", ## __VA_ARGS__, libbe_error_description(lbh))
#endif /* DEBUG */

extern bool set_zfs_properties(uzfs_fs_t *, const char *, char **);

static const char *extract_name_from_be(nvpair_t *be)
{
    assert(NULL != be);

    return nvpair_name(be); // <=> nvlist_lookup_string(cur, "name", &name);
}

static bool extract_creation_from_be(nvpair_t *be, uint64_t *creation)
{
    bool ok;
    char property[] = "creation";

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
            debug("skipping BE '%s', couldn't retrieve property '%s'", extract_name_from_be(be), property); // TODO
            break;
        }
        errno = 0;
        value = strtoull(v, &endptr, 10);
        if ('\0' != *endptr || (errno == ERANGE && value == ULLONG_MAX)) {
            debug("skipping BE '%s', value of property '%s' couldn't be properly parsed", extract_name_from_be(be), property); // TODO
            break;
        }
        *creation = (uint64_t) value;
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

static uzfs_fs_t *be_to_fs(paths_to_check_t *ptc, libbe_handle_t *lbh, const char *be, char **error)
{
    uzfs_fs_t *fs;

    fs = NULL;
    do {
        char dataset[ZFS_MAX_DATASET_NAME_LEN];

        if (!path_join(dataset, dataset + STR_SIZE(dataset), error, be_root_path(lbh), be, NULL)) {
            break;
        }
        if (NULL == (fs = uzfs_fs_from_name(ptc->lh, dataset))) {
            set_generic_error(error, "couldn't acquire a ZFS descriptor for BE '%s'", be);
            break;
        }
    } while (false);

    return fs;
}

static bm_code_t be_suitable(paths_to_check_t *ptc, void **data, char **error)
{
    size_t i;
    bm_code_t retval;

    retval = BM_ERROR;
    do {
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
    uzfs_fs_t *fs;

    assert(NULL != data);

    fs = NULL;
    ok = false;
    do {
        libbe_handle_t *lbh;

        lbh = (libbe_handle_t *) data;
        if (BE_ERR_SUCCESS != be_create(lbh, snapshot)) {
            set_be_error(error, lbh, "failed to create bootenv %s", snapshot);
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
        uzfs_fs_close(fs);
    }

    return ok;
}

static bool be_rollback(paths_to_check_t *ptc, void *data, bool temporary, char **error)
{
    bool ok;
    nvlist_t *props;

    ok = false;
    props = NULL;
    do {
        nvpair_t *cur;
        libbe_handle_t *lbh;
        uint64_t last_bootenv_creation;
        const char *last_bootenv_name;

        last_bootenv_name = NULL;
        lbh = (libbe_handle_t *) data;
        if (0 != be_prop_list_alloc(&props)) {
            set_be_error(error, lbh, "be_prop_list_alloc failed");
            break;
        }
        if (0 != be_get_bootenv_props(lbh, props)) {
            set_be_error(error, lbh, "be_get_bootenv_props failed");
            break;
        }
        for (cur = nvlist_next_nvpair(props, NULL); NULL != cur; cur = nvlist_next_nvpair(props, cur)) {
            uzfs_fs_t *fs;
            const char *name;
            uint64_t version, creation;

            name = extract_name_from_be(cur);
            if (!extract_creation_from_be(cur, &creation)) {
                continue;
            }
            if (NULL == (fs = be_to_fs(ptc, lbh, name, error))) {
                // TODO: real error handling
                continue;
            }
retry:
            if (!uzfs_fs_prop_get_numeric(fs, ZINT_VERSION_PROPERTY, &version)) {
                // <TODO: for transition, to be removed in a future version>
                if (str_starts_with(name, "pkg_pre_upgrade_") && strlen(name) == STR_LEN("pkg_pre_upgrade_YYYY-mm-dd_HH:ii:ss")) {
                    if (!set_zfs_properties(fs, "PRE:UPGRADE", NULL)) {
                        continue;
                    } else {
                        goto retry;
                    }
                }
                // </TODO: for transition, to be removed in a future version>
                debug("property '%s' not set on '%s'", ZINT_VERSION_PROPERTY, uzfs_fs_get_name(fs));
                debug("skipping BE '%s', not created by pkg zint", name);
                continue;
            } else {
                debug("%s = %" PRIu64 " for %s", ZINT_VERSION_PROPERTY, version, uzfs_fs_get_name(fs));
            }
            if (NULL == last_bootenv_name || creation > last_bootenv_creation) {
                last_bootenv_name = name;
                last_bootenv_creation = creation;
            }
// debug("%s = %s", name, ctime((time_t *) &creation));
            if (NULL != fs) {
                uzfs_fs_close(fs);
            }
        }
        if (NULL == last_bootenv_name) {
            set_generic_error(error, "no BE identified to rollback to");
            break;
        } else if (BE_ERR_SUCCESS != be_activate(lbh, last_bootenv_name, temporary)) {
// debug("%s = %s", last_bootenv_name, ctime((time_t *) &last_bootenv_creation));
            set_be_error(error, lbh, "failed to activate BE '%s'", last_bootenv_name);
            break;
        }
        ok = true;
    } while (false);
    if (NULL != props) {
        be_prop_list_free(props);
    }

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
    be_rollback,
};
